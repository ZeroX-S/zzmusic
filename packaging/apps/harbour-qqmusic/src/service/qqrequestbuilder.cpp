#include "qqrequestbuilder.h"

#include "core/qqidentifier.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QUrlQuery>
#include <QtGlobal>

namespace {

// Contract source: Listen1 qq.js at commit 3f24efa045125875a609dcb0f3f3f0be4edb8b37.
QUrl musicuUrl(const QJsonObject &request)
{
    QUrl url(QStringLiteral("https://u.y.qq.com/cgi-bin/musicu.fcg"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("inCharset"), QStringLiteral("utf8"));
    query.addQueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    query.addQueryItem(QStringLiteral("platform"), QStringLiteral("yqq.json"));
    query.addQueryItem(QStringLiteral("needNewCode"), QStringLiteral("0"));
    query.addQueryItem(
        QStringLiteral("data"),
        QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
    url.setQuery(query);
    return url;
}

QString randomUnitValue()
{
    const QByteArray bytes = QUuid::createUuid().toRfc4122();
    quint64 value = 0;
    for (int index = 0; index < 6; ++index) {
        value = (value << 8) | static_cast<quint8>(bytes.at(index));
    }
    return QString::number(
        static_cast<double>(value) / 281474976710656.0, 'g', 17);
}

void addPublicQueryItems(QUrlQuery *query)
{
    query->addQueryItem(QStringLiteral("picmid"), QStringLiteral("1"));
    query->addQueryItem(QStringLiteral("rnd"), randomUnitValue());
    query->addQueryItem(QStringLiteral("g_tk"), QStringLiteral("732560869"));
    query->addQueryItem(QStringLiteral("loginUin"), QStringLiteral("0"));
    query->addQueryItem(QStringLiteral("hostUin"), QStringLiteral("0"));
    query->addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query->addQueryItem(QStringLiteral("inCharset"), QStringLiteral("utf8"));
    query->addQueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    query->addQueryItem(QStringLiteral("notice"), QStringLiteral("0"));
    query->addQueryItem(QStringLiteral("platform"), QStringLiteral("yqq.json"));
    query->addQueryItem(QStringLiteral("needNewCode"), QStringLiteral("0"));
}

QString leftCodePoints(const QString &value, int maximum)
{
    int codePoints = 0;
    int position = 0;
    while (position < value.size() && codePoints < maximum) {
        const QChar current = value.at(position);
        if (current.isHighSurrogate()
                && position + 1 < value.size()
                && value.at(position + 1).isLowSurrogate()) {
            position += 2;
        } else {
            ++position;
        }
        ++codePoints;
    }
    return value.left(position);
}

bool isBoundedMusicKey(const QByteArray &value)
{
    if (value.isEmpty() || value.size() > 4096) {
        return false;
    }
    for (int index = 0; index < value.size(); ++index) {
        const uchar byte = static_cast<uchar>(value.at(index));
        if (byte < 0x20 || byte == 0x7f) {
            return false;
        }
    }
    return true;
}

QJsonObject authenticatedCommunication(const QString &accountUin,
                                        const QByteArray &musicKey)
{
    QJsonObject communication;
    communication.insert(QStringLiteral("uin"), accountUin);
    communication.insert(QStringLiteral("format"), QStringLiteral("json"));
    communication.insert(QStringLiteral("ct"), 19);
    communication.insert(QStringLiteral("cv"), 0);
    communication.insert(QStringLiteral("authst"),
                         QString::fromLatin1(musicKey));
    return communication;
}

QByteArray personalizedBody(const QString &accountUin,
                            const QByteArray &musicKey,
                            const QString &requestKey,
                            const QString &module,
                            const QString &method,
                            const QJsonObject &parameters)
{
    if (!QqIdentifier::isNumericPlaylistId(accountUin)
            || !isBoundedMusicKey(musicKey)) {
        return QByteArray();
    }
    QJsonObject block;
    block.insert(QStringLiteral("module"), module);
    block.insert(QStringLiteral("method"), method);
    block.insert(QStringLiteral("param"), parameters);

    QJsonObject request;
    request.insert(QStringLiteral("comm"),
                   authenticatedCommunication(accountUin, musicKey));
    request.insert(requestKey, block);
    return QJsonDocument(request).toJson(QJsonDocument::Compact);
}

}

QUrl QqRequestBuilder::recommendationsUrl()
{
    QJsonObject comm;
    comm.insert(QStringLiteral("ct"), 24);

    QJsonObject param;
    param.insert(QStringLiteral("async"), 1);
    param.insert(QStringLiteral("cmd"), 2);

    QJsonObject recommendation;
    recommendation.insert(QStringLiteral("method"),
                          QStringLiteral("get_hot_recommend"));
    recommendation.insert(QStringLiteral("param"), param);
    recommendation.insert(QStringLiteral("module"),
                          QStringLiteral("playlist.HotRecommendServer"));

    QJsonObject request;
    request.insert(QStringLiteral("comm"), comm);
    request.insert(QStringLiteral("recomPlaylist"), recommendation);
    QUrl url = musicuUrl(request);
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("loginUin"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("hostUin"), QStringLiteral("0"));
    url.setQuery(query);
    return url;
}

QUrl QqRequestBuilder::categoriesUrl()
{
    QUrl url(QStringLiteral(
        "https://c.y.qq.com/splcloud/fcgi-bin/fcg_get_diss_tag_conf.fcg"));
    QUrlQuery query;
    addPublicQueryItems(&query);
    url.setQuery(query);
    return url;
}

QUrl QqRequestBuilder::playlistsUrl(const QString &categoryId, int offset)
{
    const int first = qBound(0, offset, 5970);
    QUrl url(QStringLiteral(
        "https://c.y.qq.com/splcloud/fcgi-bin/fcg_get_diss_by_tag.fcg"));
    QUrlQuery query;
    addPublicQueryItems(&query);
    query.addQueryItem(QStringLiteral("categoryId"), categoryId);
    query.addQueryItem(QStringLiteral("sortId"), QStringLiteral("5"));
    query.addQueryItem(QStringLiteral("sin"), QString::number(first));
    query.addQueryItem(QStringLiteral("ein"), QString::number(first + 29));
    url.setQuery(query);
    return url;
}

QUrl QqRequestBuilder::toplistsUrl()
{
    QUrl url(QStringLiteral(
        "https://c.y.qq.com/v8/fcg-bin/fcg_myqq_toplist.fcg"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("g_tk"), QStringLiteral("5381"));
    query.addQueryItem(QStringLiteral("inCharset"), QStringLiteral("utf-8"));
    query.addQueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    query.addQueryItem(QStringLiteral("notice"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("uin"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("needNewCode"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("platform"), QStringLiteral("h5"));
    url.setQuery(query);
    return url;
}

QUrl QqRequestBuilder::toplistPeriodUrl()
{
    return QUrl(QStringLiteral("https://c.y.qq.com/node/pc/wk_v15/top.html"));
}

QUrl QqRequestBuilder::toplistUrl(int topId,
                                  const QString &period,
                                  int limit)
{
    QJsonObject comm;
    comm.insert(QStringLiteral("cv"), 1602);
    comm.insert(QStringLiteral("ct"), 20);

    QJsonObject param;
    param.insert(QStringLiteral("topid"), topId);
    param.insert(QStringLiteral("num"), qBound(1, limit, 100));
    param.insert(QStringLiteral("period"), period);

    QJsonObject toplist;
    toplist.insert(QStringLiteral("module"),
                   QStringLiteral("musicToplist.ToplistInfoServer"));
    toplist.insert(QStringLiteral("method"), QStringLiteral("GetDetail"));
    toplist.insert(QStringLiteral("param"), param);

    QJsonObject request;
    request.insert(QStringLiteral("comm"), comm);
    request.insert(QStringLiteral("toplist"), toplist);
    return musicuUrl(request);
}

QUrl QqRequestBuilder::playlistUrl(const QString &numericId)
{
    if (!QqIdentifier::isNumericPlaylistId(numericId)) {
        return QUrl();
    }

    QUrl url(QStringLiteral(
        "https://i.y.qq.com/qzone-music/fcg-bin/"
        "fcg_ucc_getcdinfo_byids_cp.fcg"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("json"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("utf8"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("onlysong"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("nosign"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("disstid"), numericId);
    query.addQueryItem(QStringLiteral("g_tk"), QStringLiteral("5381"));
    query.addQueryItem(QStringLiteral("loginUin"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("hostUin"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("inCharset"), QStringLiteral("GB2312"));
    query.addQueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    query.addQueryItem(QStringLiteral("notice"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("platform"), QStringLiteral("yqq"));
    query.addQueryItem(QStringLiteral("needNewCode"), QStringLiteral("0"));
    url.setQuery(query);
    return url;
}

QUrl QqRequestBuilder::personalizedUrl()
{
    return QUrl(QStringLiteral("https://u.y.qq.com/cgi-bin/musicu.fcg"));
}

QByteArray QqRequestBuilder::dailyThirtyBody(const QString &accountUin,
                                             const QByteArray &musicKey,
                                             int page)
{
    if (page < 1 || page > 4) {
        return QByteArray();
    }
    QJsonObject parameters;
    parameters.insert(QStringLiteral("Page"), page);
    return personalizedBody(
        accountUin, musicKey, QStringLiteral("radar"),
        QStringLiteral("music.recommend.TrackRelationServer"),
        QStringLiteral("GetRadarSong"), parameters);
}

QByteArray QqRequestBuilder::guessYouLikeBody(const QString &accountUin,
                                              const QByteArray &musicKey)
{
    return personalizedBody(
        accountUin, musicKey, QStringLiteral("radio"),
        QStringLiteral("music.radioProxy.MbTrackRadioSvr"),
        QStringLiteral("get_radio_track"), QJsonObject());
}

QUrl QqRequestBuilder::userUrl(const QString &uin)
{
    if (!QqIdentifier::isNumericPlaylistId(uin)) {
        return QUrl();
    }

    QJsonObject comm;
    comm.insert(QStringLiteral("ct"), 24);
    comm.insert(QStringLiteral("cv"), 0);

    QJsonArray users;
    users.append(uin);

    QJsonObject vipParam;
    vipParam.insert(QStringLiteral("uin_list"), users);
    QJsonObject vip;
    vip.insert(QStringLiteral("module"),
               QStringLiteral("userInfo.VipQueryServer"));
    vip.insert(QStringLiteral("method"),
               QStringLiteral("SRFVipQuery_V2"));
    vip.insert(QStringLiteral("param"), vipParam);

    QJsonObject baseParam;
    baseParam.insert(QStringLiteral("vec_uin"), users);
    QJsonObject base;
    base.insert(QStringLiteral("module"),
                QStringLiteral("userInfo.BaseUserInfoServer"));
    base.insert(QStringLiteral("method"),
                QStringLiteral("get_user_baseinfo_v2"));
    base.insert(QStringLiteral("param"), baseParam);

    QJsonObject request;
    request.insert(QStringLiteral("comm"), comm);
    request.insert(QStringLiteral("vip"), vip);
    request.insert(QStringLiteral("base"), base);

    QUrl url = musicuUrl(request);
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("loginUin"), uin);
    query.addQueryItem(QStringLiteral("hostUin"), QStringLiteral("0"));
    url.setQuery(query);
    return url;
}

QUrl QqRequestBuilder::userCreatedUrl(const QString &uin)
{
    if (!QqIdentifier::isNumericPlaylistId(uin)) {
        return QUrl();
    }

    QUrl url(QStringLiteral(
        "https://c.y.qq.com/rsc/fcgi-bin/fcg_user_created_diss"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("cv"), QStringLiteral("4747474"));
    query.addQueryItem(QStringLiteral("ct"), QStringLiteral("24"));
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("inCharset"), QStringLiteral("utf-8"));
    query.addQueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    query.addQueryItem(QStringLiteral("notice"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("platform"), QStringLiteral("yqq.json"));
    query.addQueryItem(QStringLiteral("needNewCode"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("uin"), uin);
    query.addQueryItem(QStringLiteral("hostuin"), uin);
    query.addQueryItem(QStringLiteral("sin"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("size"), QStringLiteral("100"));
    url.setQuery(query);
    return url;
}

QUrl QqRequestBuilder::userCollectedUrl(const QString &uin)
{
    if (!QqIdentifier::isNumericPlaylistId(uin)) {
        return QUrl();
    }

    QUrl url(QStringLiteral(
        "https://c.y.qq.com/fav/fcgi-bin/"
        "fcg_get_profile_order_asset.fcg"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ct"), QStringLiteral("20"));
    query.addQueryItem(QStringLiteral("cid"), QStringLiteral("205360956"));
    query.addQueryItem(QStringLiteral("userid"), uin);
    query.addQueryItem(QStringLiteral("reqtype"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("sin"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("ein"), QStringLiteral("100"));
    url.setQuery(query);
    return url;
}

QUrl QqRequestBuilder::lyricUrl(const QString &trackMid)
{
    if (!QqIdentifier::isTrackMid(trackMid)) {
        return QUrl();
    }

    QUrl url(QStringLiteral(
        "https://i.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("songmid"), trackMid);
    query.addQueryItem(QStringLiteral("g_tk"), QStringLiteral("5381"));
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("inCharset"), QStringLiteral("utf8"));
    query.addQueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    query.addQueryItem(QStringLiteral("nobase64"), QStringLiteral("1"));
    url.setQuery(query);
    return url;
}

QUrl QqRequestBuilder::vkeyUrl()
{
    return QUrl(QStringLiteral("https://u.y.qq.com/cgi-bin/musicu.fcg"));
}

QByteArray QqRequestBuilder::vkeyBody(const QString &trackMid,
                                      const QString &accountUin)
{
    if (!QqIdentifier::isTrackMid(trackMid)
            || (!accountUin.isEmpty()
                && !QqIdentifier::isNumericPlaylistId(accountUin))) {
        return QByteArray();
    }

    const QString requestUin = accountUin.isEmpty()
        ? QStringLiteral("0") : accountUin;
    QJsonArray filenames;
    filenames.append(QStringLiteral("F000%1%1.flac").arg(trackMid));
    filenames.append(QStringLiteral("M800%1%1.mp3").arg(trackMid));
    filenames.append(QStringLiteral("M500%1%1.mp3").arg(trackMid));
    QJsonArray songMids;
    QJsonArray songTypes;
    for (int index = 0; index < filenames.size(); ++index) {
        songMids.append(trackMid);
        songTypes.append(0);
    }

    QJsonObject param;
    param.insert(QStringLiteral("filename"), filenames);
    param.insert(QStringLiteral("guid"), QStringLiteral("10000"));
    param.insert(QStringLiteral("songmid"), songMids);
    param.insert(QStringLiteral("songtype"), songTypes);
    param.insert(QStringLiteral("uin"), requestUin);
    param.insert(QStringLiteral("loginflag"), 1);
    param.insert(QStringLiteral("platform"), QStringLiteral("20"));

    QJsonObject requestBlock;
    requestBlock.insert(QStringLiteral("module"),
                        QStringLiteral("vkey.GetVkeyServer"));
    requestBlock.insert(QStringLiteral("method"),
                        QStringLiteral("CgiGetVkey"));
    requestBlock.insert(QStringLiteral("param"), param);

    QJsonObject comm;
    comm.insert(QStringLiteral("uin"), requestUin);
    comm.insert(QStringLiteral("format"), QStringLiteral("json"));
    comm.insert(QStringLiteral("ct"), 24);
    comm.insert(QStringLiteral("cv"), 0);

    QJsonObject request;
    request.insert(QStringLiteral("req_1"), requestBlock);
    request.insert(QStringLiteral("loginUin"), requestUin);
    request.insert(QStringLiteral("comm"), comm);
    return QJsonDocument(request).toJson(QJsonDocument::Compact);
}

QByteArray QqRequestBuilder::searchBody(const QString &query,
                                        SearchKind kind,
                                        int page,
                                        const QString &accountUin)
{
    if (!accountUin.isEmpty()
            && !QqIdentifier::isNumericPlaylistId(accountUin)) {
        return QByteArray();
    }
    const QString requestUin = accountUin.isEmpty()
        ? QStringLiteral("0") : accountUin;
    QJsonObject comm;
    comm.insert(QStringLiteral("ct"), QStringLiteral("19"));
    comm.insert(QStringLiteral("cv"), QStringLiteral("1859"));
    comm.insert(QStringLiteral("uin"), requestUin);

    QJsonObject param;
    param.insert(QStringLiteral("grp"), 1);
    param.insert(QStringLiteral("num_per_page"), 50);
    param.insert(QStringLiteral("page_num"), qBound(1, page, 200));
    param.insert(QStringLiteral("query"), leftCodePoints(query, 128));
    param.insert(QStringLiteral("search_type"),
                 kind == SearchKind::Tracks ? 0 : 3);

    QJsonObject requestBlock;
    requestBlock.insert(QStringLiteral("method"),
                        QStringLiteral("DoSearchForQQMusicDesktop"));
    requestBlock.insert(QStringLiteral("module"),
                        QStringLiteral("music.search.SearchCgiService"));
    requestBlock.insert(QStringLiteral("param"), param);

    QJsonObject request;
    request.insert(QStringLiteral("comm"), comm);
    request.insert(QStringLiteral("req"), requestBlock);
    return QJsonDocument(request).toJson(QJsonDocument::Compact);
}
