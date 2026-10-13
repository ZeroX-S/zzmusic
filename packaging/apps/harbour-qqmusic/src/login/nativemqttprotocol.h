#ifndef NATIVEMQTTPROTOCOL_H
#define NATIVEMQTTPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <QVector>

struct NativeMqttPacket
{
    enum Type {
        Invalid = 0,
        ConnAck = 2,
        Publish = 3,
        SubAck = 9,
        PingRequest = 12,
        PingResponse = 13,
        Disconnect = 14
    };

    Type type = Invalid;
    int reasonCode = 0;
    quint16 packetId = 0;
    QString eventType;
    QByteArray payload;
    QString serverReference;
};

class NativeMqttProtocol
{
public:
    enum PacketType {
        Invalid = NativeMqttPacket::Invalid,
        ConnAck = NativeMqttPacket::ConnAck,
        Publish = NativeMqttPacket::Publish,
        SubAck = NativeMqttPacket::SubAck,
        PingReq = NativeMqttPacket::PingRequest,
        PingResp = NativeMqttPacket::PingResponse,
        Disconnect = NativeMqttPacket::Disconnect
    };

    enum EventType {
        EventInvalid = 0,
        EventConnAck,
        EventSubAck,
        EventPublish,
        EventPingReq,
        EventPingResp,
        EventDisconnect
    };

    struct UserProperty {
        QByteArray key;
        QByteArray value;
    };

    struct Packet {
        PacketType type = Invalid;
        EventType event = EventInvalid;
        quint8 flags = 0;
        quint8 reasonCode = 0;
        bool sessionPresent = false;
        int serverKeepAliveSeconds = -1;
        quint16 packetId = 0;
        QByteArray topic;
        QByteArray publishType;
        QByteArray payload;
        QString eventType;
        QString serverReference;
        QVector<UserProperty> userProperties;
        QVector<quint8> reasonCodes;
    };

    static int maximumPacketBytes();
    static int maximumRemainingLength();
    static int maximumPropertyBytes();

    static QByteArray encodeConnect(const QByteArray &clientId);
    static QByteArray encodeSubscribe(const QByteArray &topic);
    static QByteArray connectPacket(const QByteArray &qrId,
                                    const QByteArray &clientId);
    static QByteArray subscribePacket(const QByteArray &qrId,
                                      quint16 packetId = 1);
    static QByteArray pingRequestPacket();
    static QByteArray pingResponsePacket();
    static QByteArray connect(const QByteArray &clientId)
    {
        return encodeConnect(clientId);
    }
    static QByteArray subscribe(const QByteArray &topic)
    {
        return encodeSubscribe(topic);
    }

    static bool parse(const QByteArray &packet,
                      Packet *decoded,
                      QByteArray *diagnosticCode = 0);
    static bool decode(const QByteArray &packet,
                       Packet *decoded,
                       QByteArray *diagnosticCode = 0)
    {
        return parse(packet, decoded, diagnosticCode);
    }
    static bool parsePacket(const QByteArray &packet,
                            NativeMqttPacket *decoded,
                            QByteArray *diagnosticCode = 0);
};

#endif
