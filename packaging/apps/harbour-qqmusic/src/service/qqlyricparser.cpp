#include "qqlyricparser.h"

#include "core/qqjsonreader.h"

#include <QJsonDocument>
#include <QRegExp>
#include <QStringList>

#include <algorithm>
#include <limits>

namespace {

template<typename T>
ServiceResult<T> schemaChanged(quint64 operationId,
                               const QString &diagnostic)
{
    ServiceResult<T> result;
    result.error.kind = ErrorKind::SchemaChanged;
    result.error.operationId = operationId;
    result.error.diagnosticCode = diagnostic;
    return result;
}

template<typename T, typename U>
ServiceResult<T> endpointError(const ServiceResult<U> &source,
                               const QString &prefix)
{
    ServiceResult<T> result;
    result.error = source.error;
    if (!source.error.diagnosticCode.startsWith(QStringLiteral("qq."))) {
        result.error.diagnosticCode = prefix;
        if (!source.error.diagnosticCode.isEmpty()) {
            result.error.diagnosticCode += QLatin1Char('.')
                + source.error.diagnosticCode;
        }
    }
    return result;
}

bool lyricLineLessThan(const LyricLine &left, const LyricLine &right)
{
    return left.timeMs < right.timeMs;
}

}

qint64 QqLyricParser::parseTimestamp(const QString &timestamp)
{
    QRegExp expression(QStringLiteral(
        "^(\\d+):([0-5]\\d)(\\.(\\d{1,3}))?$"));
    if (!expression.exactMatch(timestamp)) {
        return -1;
    }

    bool minutesOk = false;
    bool secondsOk = false;
    const qint64 minutes = expression.cap(1).toLongLong(&minutesOk);
    const qint64 seconds = expression.cap(2).toLongLong(&secondsOk);
    if (!minutesOk || !secondsOk) {
        return -1;
    }

    qint64 fractionMs = 0;
    const QString fraction = expression.cap(4);
    if (!fraction.isEmpty()) {
        bool fractionOk = false;
        fractionMs = fraction.toLongLong(&fractionOk);
        if (!fractionOk) {
            return -1;
        }
        if (fraction.size() == 1) {
            fractionMs *= 100;
        } else if (fraction.size() == 2) {
            fractionMs *= 10;
        }
    }

    const qint64 secondsAndFraction = seconds * 1000 + fractionMs;
    const qint64 maximum = std::numeric_limits<qint64>::max();
    if (minutes > (maximum - secondsAndFraction) / 60000) {
        return -1;
    }
    return minutes * 60000 + secondsAndFraction;
}

QVector<LyricLine> QqLyricParser::parseLrc(const QString &lrc)
{
    QVector<LyricLine> parsed;
    const QStringList sourceLines =
        lrc.split(QRegExp(QStringLiteral("\\r?\\n")), QString::KeepEmptyParts);
    QRegExp timestampExpression(QStringLiteral(
        "\\[(\\d+:[0-5]\\d(\\.\\d{1,3})?)\\]"));

    for (int lineIndex = 0; lineIndex < sourceLines.size(); ++lineIndex) {
        const QString &sourceLine = sourceLines.at(lineIndex);
        QVector<qint64> times;
        int searchFrom = 0;
        int lastTimestampEnd = -1;
        while (timestampExpression.indexIn(sourceLine, searchFrom)
                == searchFrom) {
            const qint64 timeMs = parseTimestamp(timestampExpression.cap(1));
            if (timeMs >= 0) {
                times.append(timeMs);
            }
            searchFrom += timestampExpression.matchedLength();
            lastTimestampEnd = searchFrom;
        }
        if (times.isEmpty() || lastTimestampEnd < 0) {
            continue;
        }

        const QString text = sourceLine.mid(lastTimestampEnd);
        for (int timeIndex = 0; timeIndex < times.size(); ++timeIndex) {
            LyricLine line;
            line.timeMs = times.at(timeIndex);
            line.text = text;
            parsed.append(line);
        }
    }

    std::stable_sort(parsed.begin(), parsed.end(), lyricLineLessThan);
    QVector<LyricLine> merged;
    for (int index = 0; index < parsed.size(); ++index) {
        const LyricLine &line = parsed.at(index);
        if (merged.isEmpty() || merged.last().timeMs != line.timeMs) {
            merged.append(line);
            continue;
        }
        if (line.text.isEmpty() || merged.last().text == line.text) {
            continue;
        }
        if (!merged.last().text.isEmpty()) {
            merged.last().text += QLatin1Char('\n');
        }
        merged.last().text += line.text;
    }
    return merged;
}

int QqLyricParser::currentIndex(const QVector<LyricLine> &lines,
                                qint64 positionMs)
{
    if (lines.isEmpty()) {
        return -1;
    }

    int first = 0;
    int pastLast = lines.size();
    while (first < pastLast) {
        const int middle = first + (pastLast - first) / 2;
        if (lines.at(middle).timeMs <= positionMs) {
            first = middle + 1;
        } else {
            pastLast = middle;
        }
    }
    return first == 0 ? -1 : first - 1;
}

ServiceResult<LyricDocument> QqLyricParser::parse(const QByteArray &payload,
                                                   quint64 operationId)
{
    const QString prefix = QStringLiteral("qq.lyric");
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return schemaChanged<LyricDocument>(
            operationId, prefix + QStringLiteral(".root.object"));
    }

    QqJsonReader reader(document, operationId);
    const ServiceResult<QString> lyric = reader.requiredString(
        QStringList() << QStringLiteral("lyric"));
    if (!lyric.ok()) {
        return endpointError<LyricDocument>(lyric, prefix);
    }
    const ServiceResult<QString> translation = reader.requiredString(
        QStringList() << QStringLiteral("trans"));
    if (!translation.ok()) {
        return endpointError<LyricDocument>(translation, prefix);
    }

    ServiceResult<LyricDocument> result;
    result.value.lines = parseLrc(lyric.value);
    QString translationPayload = translation.value;
    translationPayload.replace(QStringLiteral("//"), QString());
    const QVector<LyricLine> translated = parseLrc(translationPayload);

    int translationIndex = 0;
    for (int lineIndex = 0;
         lineIndex < result.value.lines.size();
         ++lineIndex) {
        LyricLine &line = result.value.lines[lineIndex];
        while (translationIndex < translated.size()
               && translated.at(translationIndex).timeMs < line.timeMs) {
            ++translationIndex;
        }
        if (translationIndex < translated.size()
                && translated.at(translationIndex).timeMs == line.timeMs) {
            line.translation = translated.at(translationIndex).text;
        }
    }
    return result;
}
