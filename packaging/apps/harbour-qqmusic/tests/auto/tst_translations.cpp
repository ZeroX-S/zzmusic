#include <QtTest>

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegExp>
#include <QSet>
#include <QTranslator>
#include <QXmlStreamReader>

class TranslationsTest : public QObject
{
    Q_OBJECT

    struct TranslationEntry {
        QString translation;
        bool unfinished = false;
    };

    static QString appRoot()
    {
        return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath())
            .absoluteFilePath(QStringLiteral("../.."));
    }

    static QString messageKey(const QString &context, const QString &source)
    {
        return context + QChar(0x1f) + source;
    }

    static QSet<QString> placeholders(const QString &text)
    {
        QSet<QString> result;
        QRegExp expression(QStringLiteral("%[1-9]"));
        int offset = 0;
        while ((offset = expression.indexIn(text, offset)) >= 0) {
            result.insert(expression.cap(0));
            offset += expression.matchedLength();
        }
        return result;
    }

    static QMap<QString, TranslationEntry> readTranslations(
        const QString &path, QString *language, QString *error)
    {
        QMap<QString, TranslationEntry> result;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = file.errorString();
            return result;
        }
        QXmlStreamReader xml(&file);
        QString context;
        QString source;
        TranslationEntry entry;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) {
                continue;
            }
            if (xml.name() == QStringLiteral("TS")) {
                *language = xml.attributes().value(
                    QStringLiteral("language")).toString();
            } else if (xml.name() == QStringLiteral("name")) {
                context = xml.readElementText();
            } else if (xml.name() == QStringLiteral("source")) {
                source = xml.readElementText();
            } else if (xml.name() == QStringLiteral("translation")) {
                entry.unfinished = xml.attributes().value(
                    QStringLiteral("type")) == QStringLiteral("unfinished");
                entry.translation = xml.readElementText(
                    QXmlStreamReader::IncludeChildElements);
                if (!context.isEmpty() && !source.isEmpty()) {
                    result.insert(messageKey(context, source), entry);
                }
                source.clear();
                entry = TranslationEntry();
            }
        }
        if (xml.hasError()) {
            *error = xml.errorString();
            result.clear();
        }
        return result;
    }

    static QSet<QString> qmlMessages(const QStringList &roots)
    {
        QSet<QString> result;
        QRegExp expression(QStringLiteral("qsTr\\(\"([^\"]+)\"\\)"));
        for (const QString &root : roots) {
            QFileInfo info(root);
            QStringList paths;
            if (info.isDir()) {
                QDirIterator iterator(root, QStringList() << QStringLiteral("*.qml"),
                                      QDir::Files,
                                      QDirIterator::Subdirectories);
                while (iterator.hasNext()) {
                    paths.append(iterator.next());
                }
            } else {
                paths.append(root);
            }
            for (const QString &path : paths) {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly)) {
                    continue;
                }
                const QString text = QString::fromUtf8(file.readAll());
                int offset = 0;
                while ((offset = expression.indexIn(text, offset)) >= 0) {
                    result.insert(messageKey(QFileInfo(path).baseName(),
                                             expression.cap(1)));
                    offset += expression.matchedLength();
                }
            }
        }
        return result;
    }

    static QSet<QString> cppMessages(const QString &path,
                                     const QString &context)
    {
        QSet<QString> result;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return result;
        }
        const QString text = QString::fromUtf8(file.readAll());
        QRegExp expression(QStringLiteral("\\btr\\(\"([^\"]+)\"\\)"));
        int offset = 0;
        while ((offset = expression.indexIn(text, offset)) >= 0) {
            result.insert(messageKey(context, expression.cap(1)));
            offset += expression.matchedLength();
        }
        return result;
    }

private slots:
    void catalogCoversEveryQmlMessage()
    {
        const QString root = appRoot();
        const QString ts = QDir(root).filePath(QStringLiteral(
            "translations/harbour-qqmusic-zh_CN.ts"));
        QString language;
        QString error;
        const QMap<QString, TranslationEntry> translations =
            readTranslations(ts, &language, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(language, QStringLiteral("zh_CN"));
        QSet<QString> messages = qmlMessages(QStringList()
            << QDir(root).filePath(QStringLiteral("qml")));
        messages.unite(cppMessages(
            QDir(root).filePath(QStringLiteral(
                "src/applicationcontroller.cpp")),
            QStringLiteral("ApplicationController")));
        QCOMPARE(translations.size(), messages.size());
        for (const QString &key : messages) {
            QVERIFY2(translations.contains(key), qPrintable(key));
            const TranslationEntry entry = translations.value(key);
            QVERIFY2(!entry.unfinished, qPrintable(key));
            QVERIFY2(!entry.translation.trimmed().isEmpty(), qPrintable(key));
            const QString source = key.section(QChar(0x1f), 1);
            QCOMPARE(placeholders(entry.translation), placeholders(source));
            const QString lower = entry.translation.toLower();
            QVERIFY(!lower.contains(QStringLiteral("cookie")));
            QVERIFY(!lower.contains(QStringLiteral("qqmusic_key")));
            QVERIFY(!lower.contains(QStringLiteral("http://")));
            QVERIFY(!lower.contains(QStringLiteral("https://")));
            QVERIFY(!lower.contains(QStringLiteral(".stream.qqmusic")));
        }
    }

    void compiledZhCnCatalogLoads()
    {
        QTranslator translator;
        QVERIFY2(translator.load(QStringLiteral(QQMUSIC_TRANSLATION_QM)),
                 QQMUSIC_TRANSLATION_QM);
        QCOMPARE(translator.translate("HomePage", "Settings"),
                 QString::fromUtf8("设置"));
        QCOMPARE(translator.translate("PlayerPage", "Now playing"),
                 QString::fromUtf8("正在播放"));
        QCOMPARE(translator.translate(
                     "MyMusicPage",
                     "Sign in with QR code"),
                 QString::fromUtf8("使用二维码登录"));
    }
};

QTEST_GUILESS_MAIN(TranslationsTest)
#include "tst_translations.moc"
