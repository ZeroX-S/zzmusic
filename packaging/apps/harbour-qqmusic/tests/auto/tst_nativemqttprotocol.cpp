#include <QtTest>

#include "login/nativemqttprotocol.h"

namespace {

QByteArray property(const QByteArray &key, const QByteArray &value)
{
    QByteArray result(1, static_cast<char>(0x26));
    result.append(static_cast<char>((key.size() >> 8) & 0xff));
    result.append(static_cast<char>(key.size() & 0xff));
    result.append(key);
    result.append(static_cast<char>((value.size() >> 8) & 0xff));
    result.append(static_cast<char>(value.size() & 0xff));
    result.append(value);
    return result;
}

QByteArray frame(quint8 header, const QByteArray &body)
{
    QByteArray result(1, static_cast<char>(header));
    int value = body.size();
    do {
        quint8 byte = static_cast<quint8>(value % 128);
        value /= 128;
        if (value != 0) {
            byte |= 0x80;
        }
        result.append(static_cast<char>(byte));
    } while (value != 0);
    result.append(body);
    return result;
}

QByteArray publish(const QByteArray &type,
                   const QByteArray &payload,
                   const QByteArray &topic = QByteArrayLiteral("login/event"))
{
    QByteArray properties = property(QByteArrayLiteral("type"), type);
    QByteArray body;
    body.append(static_cast<char>((topic.size() >> 8) & 0xff));
    body.append(static_cast<char>(topic.size() & 0xff));
    body.append(topic);
    body.append(static_cast<char>(properties.size()));
    body.append(properties);
    body.append(payload);
    return frame(0x30, body);
}

}

class NativeMqttProtocolTest : public QObject
{
    Q_OBJECT

private slots:
    void encodesBoundedConnectWithFixedFields()
    {
        const QByteArray packet = NativeMqttProtocol::connectPacket(
            QByteArrayLiteral("sanitized-qr"),
            QByteArrayLiteral("client-sanitized-1"));
        QCOMPARE(packet.at(0), static_cast<char>(0x10));
        QVERIFY(packet.contains(QByteArray::fromHex("00044d51545405")));
        QVERIFY(packet.contains(QByteArrayLiteral("pass")));
        QVERIFY(packet.contains(QByteArrayLiteral("tmeAppID")));
        QVERIFY(packet.contains(QByteArrayLiteral("qqmusic")));
        QVERIFY(packet.contains(QByteArrayLiteral("business")));
        QVERIFY(packet.contains(QByteArrayLiteral("management")));
        QVERIFY(packet.contains(QByteArrayLiteral("hashTag")));
        QVERIFY(packet.contains(QByteArrayLiteral("clientTag")));
        QVERIFY(packet.contains(QByteArrayLiteral("management.user")));
        QVERIFY(packet.contains(QByteArrayLiteral("userID")));
        QVERIFY(packet.contains(QByteArrayLiteral("sanitized-qr")));
        QVERIFY(NativeMqttProtocol::encodeConnect(QByteArray()).isEmpty());
        QVERIFY(NativeMqttProtocol::encodeConnect(QByteArray(257, 'x')).isEmpty());
        QVERIFY(NativeMqttProtocol::encodeConnect(QByteArray("bad\0id", 6)).isEmpty());
    }

    void encodesProtocolBoundaryQrId()
    {
        const QByteArray maximumQrId(256, 'q');
        const QByteArray packet = NativeMqttProtocol::connectPacket(
            maximumQrId, QByteArrayLiteral("client-sanitized-1"));
        QVERIFY(!packet.isEmpty());
        QVERIFY(packet.contains(maximumQrId));
        QVERIFY(NativeMqttProtocol::connectPacket(
                    QByteArray(257, 'q'),
                    QByteArrayLiteral("client-sanitized-1")).isEmpty());
        const QByteArray subscribe = NativeMqttProtocol::subscribePacket(
            maximumQrId, 1);
        QVERIFY(!subscribe.isEmpty());
        QVERIFY(subscribe.contains(maximumQrId));
        QVERIFY(NativeMqttProtocol::subscribePacket(
                    QByteArray(257, 'q'), 1).isEmpty());
    }

    void encodesSubscribeWithPacketIdAndAuthorizationProperty()
    {
        const QByteArray packet = NativeMqttProtocol::encodeSubscribe(
            QByteArrayLiteral("management.qrcode_login/sanitized"));
        QCOMPARE(static_cast<quint8>(packet.at(0)), quint8(0x82));
        QCOMPARE(static_cast<quint8>(packet.at(2)), quint8(0x00));
        QCOMPARE(static_cast<quint8>(packet.at(3)), quint8(0x01));
        QVERIFY(packet.contains(QByteArrayLiteral("authorization")));
        QVERIFY(packet.contains(QByteArrayLiteral("tmelogin")));
        QVERIFY(packet.contains(QByteArrayLiteral("pubsub")));
        QVERIFY(packet.contains(QByteArrayLiteral("unicast")));
        QVERIFY(NativeMqttProtocol::encodeSubscribe(QByteArray()).isEmpty());
        QVERIFY(NativeMqttProtocol::encodeSubscribe(QByteArray(1025, 'x')).isEmpty());
    }

    void parsesSupportedEventTypes()
    {
        NativeMqttProtocol::Packet packet;
        QVERIFY(NativeMqttProtocol::parse(
            QByteArray::fromHex("2003000000"), &packet));
        QCOMPARE(packet.event, NativeMqttProtocol::EventConnAck);
        QCOMPARE(packet.reasonCode, quint8(0));
        QVERIFY(NativeMqttProtocol::parse(
            QByteArray::fromHex("900400010000"), &packet));
        QCOMPARE(packet.event, NativeMqttProtocol::EventSubAck);
        QCOMPARE(packet.packetId, quint16(1));
        QCOMPARE(packet.reasonCodes.size(), 1);
        QVERIFY(NativeMqttProtocol::parse(QByteArray::fromHex("c000"), &packet));
        QCOMPARE(packet.event, NativeMqttProtocol::EventPingReq);
        QVERIFY(NativeMqttProtocol::parse(
            NativeMqttProtocol::pingResponsePacket(), &packet));
        QCOMPARE(packet.event, NativeMqttProtocol::EventPingResp);
        QVERIFY(NativeMqttProtocol::parse(QByteArray::fromHex("e000"), &packet));
        QCOMPARE(packet.event, NativeMqttProtocol::EventDisconnect);
    }

    void validatesServerKeepAlive()
    {
        NativeMqttProtocol::Packet packet;
        QVERIFY(NativeMqttProtocol::parse(QByteArray::fromHex("2006000003130001"), &packet));
        QCOMPARE(packet.serverKeepAliveSeconds, 1);
        QVERIFY(!NativeMqttProtocol::parse(QByteArray::fromHex("2009000006130001130002"), &packet));
        QVERIFY(!NativeMqttProtocol::parse(QByteArray::fromHex("900700010313000100"), &packet));
    }

    void extractsPublishTypeAndPayload()
    {
        NativeMqttProtocol::Packet packet;
        QVERIFY(NativeMqttProtocol::parse(
            publish(QByteArrayLiteral("scanned"),
                    QByteArrayLiteral("sanitized-event")), &packet));
        QCOMPARE(packet.event, NativeMqttProtocol::EventPublish);
        QCOMPARE(packet.publishType, QByteArrayLiteral("scanned"));
        QCOMPARE(packet.payload, QByteArrayLiteral("sanitized-event"));
        QCOMPARE(packet.userProperties.size(), 1);

        NativeMqttPacket publicPacket;
        QVERIFY(NativeMqttProtocol::parsePacket(
            publish(QByteArrayLiteral("candidate"),
                    QByteArrayLiteral("sanitized-payload")), &publicPacket));
        QCOMPARE(publicPacket.type, NativeMqttPacket::Publish);
        QCOMPARE(publicPacket.eventType, QStringLiteral("candidate"));
        QCOMPARE(publicPacket.payload, QByteArrayLiteral("sanitized-payload"));
    }

    void extractsConnAckServerReference()
    {
        NativeMqttPacket packet;
        QVERIFY(NativeMqttProtocol::parsePacket(
            QByteArray::fromHex("20090000061c0003737276"), &packet));
        QCOMPARE(packet.type, NativeMqttPacket::ConnAck);
        QCOMPARE(packet.reasonCode, 0);
        QCOMPARE(packet.serverReference, QStringLiteral("srv"));
    }

    void rejectsMalformedUtf8AndProperties()
    {
        NativeMqttProtocol::Packet packet;
        QByteArray malformed = publish(QByteArrayLiteral("scanned"),
                                       QByteArrayLiteral("payload"));
        const int typeOffset = malformed.indexOf(QByteArrayLiteral("scanned"));
        QVERIFY(typeOffset > 0);
        malformed[typeOffset] = static_cast<char>(0xc0);
        QVERIFY(!NativeMqttProtocol::parse(malformed, &packet));

        QByteArray unknown = QByteArray::fromHex("3008000b612f73616e697a6564");
        QVERIFY(!NativeMqttProtocol::parse(unknown, &packet));

        QByteArray badPropertyLength = publish(QByteArrayLiteral("scanned"),
                                                QByteArrayLiteral("payload"));
        const int propertyLengthOffset = badPropertyLength.indexOf(
            QByteArrayLiteral("type")) - 3;
        QVERIFY(propertyLengthOffset > 0);
        badPropertyLength[propertyLengthOffset] = static_cast<char>(0xff);
        QVERIFY(!NativeMqttProtocol::parse(badPropertyLength, &packet));
    }

    void rejectsRemainingLengthAbuseAndUnknownPackets()
    {
        NativeMqttProtocol::Packet packet;
        QByteArray tooLong = QByteArray::fromHex("c0ffffff7f");
        QVERIFY(!NativeMqttProtocol::parse(tooLong, &packet));
        QVERIFY(!NativeMqttProtocol::parse(QByteArray::fromHex("1000"), &packet));
        QVERIFY(!NativeMqttProtocol::parse(QByteArray::fromHex("4000"), &packet));
        QVERIFY(!NativeMqttProtocol::parse(QByteArray::fromHex("b000"), &packet));
    }

    void rejectsNonQosZeroPublishFlags()
    {
        NativeMqttProtocol::Packet packet;
        QByteArray retained = publish(QByteArrayLiteral("scanned"),
                                      QByteArrayLiteral("payload"));
        retained[0] = static_cast<char>(0x31);
        QVERIFY(!NativeMqttProtocol::parse(retained, &packet));

        QByteArray qosOne = publish(QByteArrayLiteral("scanned"),
                                    QByteArrayLiteral("payload"));
        qosOne[0] = static_cast<char>(0x32);
        QVERIFY(!NativeMqttProtocol::parse(qosOne, &packet));
    }
};

QTEST_GUILESS_MAIN(NativeMqttProtocolTest)
#include "tst_nativemqttprotocol.moc"
