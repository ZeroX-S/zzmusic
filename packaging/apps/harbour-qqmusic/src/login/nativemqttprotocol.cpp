#include "nativemqttprotocol.h"

#include <QtGlobal>

namespace {

const int MaximumPacketBytes = 64 * 1024;
const int MaximumRemainingLength = 63 * 1024;
const int MaximumPropertyBytes = 4 * 1024;
const int MaximumClientIdBytes = 256;
const int MaximumTopicBytes = 1024;
const int MaximumTypeBytes = 64;
const int MaximumQrIdBytes = 256;
const int MaximumUserPropertyValueBytes = 256;
const int MaximumPayloadBytes = MaximumPacketBytes;

const quint8 PropertyPayloadFormat = 0x01;
const quint8 PropertyMessageExpiry = 0x02;
const quint8 PropertyContentType = 0x03;
const quint8 PropertyResponseTopic = 0x08;
const quint8 PropertyCorrelationData = 0x09;
const quint8 PropertySubscriptionIdentifier = 0x0b;
const quint8 PropertySessionExpiry = 0x11;
const quint8 PropertyAssignedClientId = 0x12;
const quint8 PropertyServerKeepAlive = 0x13;
const quint8 PropertyAuthenticationMethod = 0x15;
const quint8 PropertyAuthenticationData = 0x16;
const quint8 PropertyRequestProblemInformation = 0x17;
const quint8 PropertyRequestResponseInformation = 0x19;
const quint8 PropertyResponseInformation = 0x1a;
const quint8 PropertyServerReference = 0x1c;
const quint8 PropertyReasonString = 0x1f;
const quint8 PropertyReceiveMaximum = 0x21;
const quint8 PropertyTopicAliasMaximum = 0x22;
const quint8 PropertyTopicAlias = 0x23;
const quint8 PropertyMaximumQos = 0x24;
const quint8 PropertyRetainAvailable = 0x25;
const quint8 PropertyUser = 0x26;
const quint8 PropertyMaximumPacketSize = 0x27;
const quint8 PropertyWildcardSubscription = 0x28;
const quint8 PropertySubscriptionIdentifierAvailable = 0x29;
const quint8 PropertySharedSubscriptionAvailable = 0x2a;

enum PropertyShape {
    ShapeByte,
    ShapeTwoByte,
    ShapeFourByte,
    ShapeUtf8,
    ShapeBinary,
    ShapeVariable,
    ShapePair
};

struct Reader {
    QByteArray data;
    int position;
    bool ok;

    explicit Reader(const QByteArray &bytes)
        : data(bytes), position(0), ok(true)
    {
    }

    int remaining() const
    {
        return data.size() - position;
    }

    bool take(quint8 *value)
    {
        if (!ok || remaining() < 1) {
            ok = false;
            return false;
        }
        *value = static_cast<quint8>(data.at(position++));
        return true;
    }

    bool takeBytes(int count, QByteArray *value)
    {
        if (!ok || count < 0 || remaining() < count) {
            ok = false;
            return false;
        }
        *value = data.mid(position, count);
        position += count;
        return true;
    }

    bool takeU16(quint16 *value)
    {
        quint8 first = 0;
        quint8 second = 0;
        if (!take(&first) || !take(&second)) {
            return false;
        }
        *value = static_cast<quint16>((first << 8) | second);
        return true;
    }

    bool takeU32(quint32 *value)
    {
        quint8 a = 0;
        quint8 b = 0;
        quint8 c = 0;
        quint8 d = 0;
        if (!take(&a) || !take(&b) || !take(&c) || !take(&d)) {
            return false;
        }
        *value = (static_cast<quint32>(a) << 24)
            | (static_cast<quint32>(b) << 16)
            | (static_cast<quint32>(c) << 8)
            | static_cast<quint32>(d);
        return true;
    }
};

bool validUtf8(const QByteArray &value)
{
    for (int i = 0; i < value.size();) {
        const uchar first = static_cast<uchar>(value.at(i));
        int length = 0;
        quint32 codePoint = 0;
        if (first <= 0x7f) {
            length = 1;
            codePoint = first;
        } else if (first >= 0xc2 && first <= 0xdf) {
            length = 2;
            codePoint = first & 0x1f;
        } else if (first >= 0xe0 && first <= 0xef) {
            length = 3;
            codePoint = first & 0x0f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            length = 4;
            codePoint = first & 0x07;
        } else {
            return false;
        }
        if (i + length > value.size()) {
            return false;
        }
        for (int j = 1; j < length; ++j) {
            const uchar byte = static_cast<uchar>(value.at(i + j));
            if ((byte & 0xc0) != 0x80) {
                return false;
            }
            codePoint = (codePoint << 6) | (byte & 0x3f);
        }
        if ((length == 2 && codePoint < 0x80)
                || (length == 3 && codePoint < 0x800)
                || (length == 4 && codePoint < 0x10000)
                || codePoint > 0x10ffff
                || (codePoint >= 0xd800 && codePoint <= 0xdfff)
                || (codePoint & 0xffff) >= 0xfdd0
                || (codePoint & 0xffff) <= 0x001f
                || (codePoint & 0xffff) == 0x007f) {
            return false;
        }
        i += length;
    }
    return true;
}

bool boundedUtf8(const QByteArray &value, int maximum)
{
    return !value.isEmpty() && value.size() <= maximum && validUtf8(value);
}

bool writeUtf8(QByteArray *output, const QByteArray &value, int maximum)
{
    if (!boundedUtf8(value, maximum) || value.size() > 0xffff) {
        return false;
    }
    output->append(static_cast<char>((value.size() >> 8) & 0xff));
    output->append(static_cast<char>(value.size() & 0xff));
    output->append(value);
    return true;
}

bool writePropertyLength(QByteArray *output, int length)
{
    if (length < 0 || length > MaximumPropertyBytes) {
        return false;
    }
    int value = length;
    do {
        quint8 encoded = static_cast<quint8>(value % 128);
        value /= 128;
        if (value != 0) {
            encoded |= 0x80;
        }
        output->append(static_cast<char>(encoded));
    } while (value != 0);
    return true;
}

bool writeUserProperty(QByteArray *output,
                       const QByteArray &key,
                       const QByteArray &value)
{
    output->append(static_cast<char>(PropertyUser));
    return writeUtf8(output, key, MaximumTypeBytes)
        && writeUtf8(output, value, MaximumUserPropertyValueBytes);
}

QByteArray frame(quint8 header, const QByteArray &body)
{
    if (body.size() > MaximumRemainingLength) {
        return QByteArray();
    }
    QByteArray result;
    result.append(static_cast<char>(header));
    int value = body.size();
    do {
        quint8 encoded = static_cast<quint8>(value % 128);
        value /= 128;
        if (value != 0) {
            encoded |= 0x80;
        }
        result.append(static_cast<char>(encoded));
    } while (value != 0);
    result.append(body);
    return result;
}

bool readVariableInteger(Reader *reader, int maximum, quint32 *value)
{
    quint32 result = 0;
    quint32 multiplier = 1;
    int count = 0;
    while (true) {
        quint8 byte = 0;
        if (!reader->take(&byte) || ++count > 4) {
            reader->ok = false;
            return false;
        }
        result += static_cast<quint32>(byte & 0x7f) * multiplier;
        if (result > static_cast<quint32>(maximum)) {
            reader->ok = false;
            return false;
        }
        if ((byte & 0x80) == 0) {
            if (count > 1
                    && result < (static_cast<quint32>(1)
                                 << (7 * (count - 1)))) {
                reader->ok = false;
                return false;
            }
            *value = result;
            return true;
        }
        multiplier *= 128;
    }
}

bool propertyShape(quint8 id, PropertyShape *shape)
{
    switch (id) {
    case PropertyPayloadFormat:
    case PropertyRequestProblemInformation:
    case PropertyRequestResponseInformation:
    case PropertyMaximumQos:
    case PropertyRetainAvailable:
    case PropertyWildcardSubscription:
    case PropertySubscriptionIdentifierAvailable:
    case PropertySharedSubscriptionAvailable:
        *shape = ShapeByte;
        return true;
    case PropertyServerKeepAlive:
    case PropertyReceiveMaximum:
    case PropertyTopicAliasMaximum:
    case PropertyTopicAlias:
        *shape = ShapeTwoByte;
        return true;
    case PropertyMessageExpiry:
    case PropertySessionExpiry:
    case PropertyMaximumPacketSize:
        *shape = ShapeFourByte;
        return true;
    case PropertyContentType:
    case PropertyResponseTopic:
    case PropertyAssignedClientId:
    case PropertyAuthenticationMethod:
    case PropertyReasonString:
    case PropertyResponseInformation:
    case PropertyServerReference:
        *shape = ShapeUtf8;
        return true;
    case PropertyCorrelationData:
    case PropertyAuthenticationData:
        *shape = ShapeBinary;
        return true;
    case PropertySubscriptionIdentifier:
        *shape = ShapeVariable;
        return true;
    case PropertyUser:
        *shape = ShapePair;
        return true;
    default:
        return false;
    }
}

bool readProperties(Reader *reader,
                    NativeMqttProtocol::Packet *packet,
                    QByteArray *diagnosticCode)
{
    quint32 propertyLength = 0;
    if (!readVariableInteger(reader, MaximumPropertyBytes, &propertyLength)
            || propertyLength > static_cast<quint32>(reader->remaining())) {
        if (diagnosticCode) {
            *diagnosticCode = QByteArrayLiteral("property-length");
        }
        return false;
    }
    Reader properties(reader->data.mid(reader->position,
                                       static_cast<int>(propertyLength)));
    reader->position += static_cast<int>(propertyLength);
    while (properties.remaining() > 0) {
        quint8 id = 0;
        PropertyShape shape = ShapeByte;
        if (!properties.take(&id) || !propertyShape(id, &shape)) {
            if (diagnosticCode) {
                *diagnosticCode = QByteArrayLiteral("property-id");
            }
            return false;
        }
        QByteArray key;
        QByteArray value;
        switch (shape) {
        case ShapeByte: {
            quint8 ignored = 0;
            if (!properties.take(&ignored)) {
                return false;
            }
            break;
        }
        case ShapeTwoByte: {
            quint16 ignored = 0;
            if (!properties.takeU16(&ignored)) {
                return false;
            }
            if (id == PropertyServerKeepAlive) {
                if (packet->type != NativeMqttProtocol::ConnAck
                        || packet->serverKeepAliveSeconds >= 0) {
                    if (diagnosticCode)
                        *diagnosticCode = QByteArrayLiteral("server-keepalive");
                    return false;
                }
                packet->serverKeepAliveSeconds = ignored;
            }
            break;
        }
        case ShapeFourByte: {
            quint32 ignored = 0;
            if (!properties.takeU32(&ignored)) {
                return false;
            }
            break;
        }
        case ShapeUtf8:
        case ShapeBinary: {
            quint16 length = 0;
            if (!properties.takeU16(&length)
                    || !properties.takeBytes(length, &value)
                    || (shape == ShapeUtf8 && !validUtf8(value))) {
                if (diagnosticCode) {
                    *diagnosticCode = QByteArrayLiteral("property-value");
                }
                return false;
            }
            if (id == PropertyServerReference) {
                packet->serverReference = QString::fromUtf8(value);
            }
            break;
        }
        case ShapeVariable: {
            quint32 ignored = 0;
            if (!readVariableInteger(&properties, MaximumRemainingLength,
                                     &ignored)) {
                return false;
            }
            break;
        }
        case ShapePair: {
            quint16 keyLength = 0;
            quint16 valueLength = 0;
            if (!properties.takeU16(&keyLength)
                    || !properties.takeBytes(keyLength, &key)
                    || !validUtf8(key)
                    || !properties.takeU16(&valueLength)
                    || !properties.takeBytes(valueLength, &value)
                    || !validUtf8(value)
                    || key.size() > MaximumTypeBytes
                    || value.size() > MaximumUserPropertyValueBytes) {
                if (diagnosticCode) {
                    *diagnosticCode = QByteArrayLiteral("user-property");
                }
                return false;
            }
            /* User properties are the only property shape retained by the
             * bounded codec.  Other valid MQTT properties are consumed. */
            packet->userProperties.append(
                NativeMqttProtocol::UserProperty{key, value});
            break;
        }
        }
    }
    if (!properties.ok) {
        if (diagnosticCode) {
            *diagnosticCode = QByteArrayLiteral("property-truncated");
        }
        return false;
    }
    return true;
}

bool readReasonAndProperties(Reader *reader,
                             NativeMqttProtocol::Packet *packet,
                             QByteArray *diagnosticCode)
{
    if (reader->remaining() == 0) {
        return true;
    }
    if (!reader->take(&packet->reasonCode)) {
        return false;
    }
    return reader->remaining() == 0
        || readProperties(reader, packet, diagnosticCode);
}

bool fixedFlagsValid(NativeMqttProtocol::PacketType type, quint8 flags)
{
    switch (type) {
    case NativeMqttProtocol::ConnAck:
    case NativeMqttProtocol::SubAck:
    case NativeMqttProtocol::PingReq:
    case NativeMqttProtocol::PingResp:
    case NativeMqttProtocol::Disconnect:
        return flags == 0;
    case NativeMqttProtocol::Publish:
        return flags == 0;
    default:
        return false;
    }
}

void setError(QByteArray *diagnosticCode, const char *code)
{
    if (diagnosticCode) {
        *diagnosticCode = QByteArray(code);
    }
}

}

int NativeMqttProtocol::maximumPacketBytes()
{
    return MaximumPacketBytes;
}

int NativeMqttProtocol::maximumRemainingLength()
{
    return MaximumRemainingLength;
}

int NativeMqttProtocol::maximumPropertyBytes()
{
    return MaximumPropertyBytes;
}

QByteArray encodeConnectInternal(const QByteArray &clientId,
                                 const QByteArray &qrId)
{
    if (!boundedUtf8(clientId, MaximumClientIdBytes)) {
        return QByteArray();
    }
    QByteArray properties;
    properties.append(static_cast<char>(PropertyAuthenticationMethod));
    if (!writeUtf8(&properties, QByteArrayLiteral("pass"), MaximumTypeBytes)) {
        return QByteArray();
    }
    if (!qrId.isEmpty()) {
        if (!writeUserProperty(&properties,
                               QByteArrayLiteral("tmeAppID"),
                               QByteArrayLiteral("qqmusic"))
                || !writeUserProperty(&properties,
                                      QByteArrayLiteral("business"),
                                      QByteArrayLiteral("management"))
                || !writeUserProperty(&properties,
                                      QByteArrayLiteral("hashTag"), qrId)
                || !writeUserProperty(&properties,
                                      QByteArrayLiteral("clientTag"),
                                      QByteArrayLiteral("management.user"))
                || !writeUserProperty(&properties,
                                      QByteArrayLiteral("userID"), qrId)) {
            return QByteArray();
        }
    }
    QByteArray body;
    if (!writeUtf8(&body, QByteArrayLiteral("MQTT"), 4)) {
        return QByteArray();
    }
    body.append(static_cast<char>(5));
    body.append(static_cast<char>(0x02));
    body.append(static_cast<char>(0x00));
    body.append(static_cast<char>(0x2d));
    if (!writePropertyLength(&body, properties.size())) {
        return QByteArray();
    }
    body.append(properties);
    if (!writeUtf8(&body, clientId, MaximumClientIdBytes)) {
        return QByteArray();
    }
    return frame(0x10, body);
}

QByteArray NativeMqttProtocol::encodeConnect(const QByteArray &clientId)
{
    return encodeConnectInternal(clientId, QByteArray());
}

QByteArray NativeMqttProtocol::connectPacket(const QByteArray &qrId,
                                             const QByteArray &clientId)
{
    if (!boundedUtf8(qrId, MaximumQrIdBytes)) {
        return QByteArray();
    }
    return encodeConnectInternal(clientId, qrId);
}

QByteArray encodeSubscribeInternal(const QByteArray &topic, quint16 packetId)
{
    if (!boundedUtf8(topic, MaximumTopicBytes) || packetId == 0) {
        return QByteArray();
    }
    QByteArray properties;
    if (!writeUserProperty(&properties,
                           QByteArrayLiteral("authorization"),
                           QByteArrayLiteral("tmelogin"))
            || !writeUserProperty(&properties,
                                  QByteArrayLiteral("pubsub"),
                                  QByteArrayLiteral("unicast"))) {
        return QByteArray();
    }
    QByteArray body;
    body.append(static_cast<char>((packetId >> 8) & 0xff));
    body.append(static_cast<char>(packetId & 0xff));
    if (!writePropertyLength(&body, properties.size())) {
        return QByteArray();
    }
    body.append(properties);
    if (!writeUtf8(&body, topic, MaximumTopicBytes)) {
        return QByteArray();
    }
    body.append(static_cast<char>(0x00));
    return frame(0x82, body);
}

QByteArray NativeMqttProtocol::encodeSubscribe(const QByteArray &topic)
{
    return encodeSubscribeInternal(topic, 1);
}

QByteArray NativeMqttProtocol::subscribePacket(const QByteArray &qrId,
                                               quint16 packetId)
{
    if (!boundedUtf8(qrId, MaximumQrIdBytes) || packetId == 0) {
        return QByteArray();
    }
    return encodeSubscribeInternal(QByteArrayLiteral("management.qrcode_login/")
                                   + qrId, packetId);
}

QByteArray NativeMqttProtocol::pingRequestPacket()
{
    return QByteArray::fromHex("c000");
}

QByteArray NativeMqttProtocol::pingResponsePacket()
{
    return QByteArray::fromHex("d000");
}

bool NativeMqttProtocol::parse(const QByteArray &wire,
                               Packet *decoded,
                               QByteArray *diagnosticCode)
{
    if (decoded) {
        *decoded = Packet();
    }
    if (!decoded || wire.size() < 2 || wire.size() > MaximumPacketBytes) {
        setError(diagnosticCode, "packet-size");
        return false;
    }
    const quint8 header = static_cast<quint8>(wire.at(0));
    const PacketType type = static_cast<PacketType>(header >> 4);
    const quint8 flags = header & 0x0f;
    if (!fixedFlagsValid(type, flags)) {
        setError(diagnosticCode, "fixed-flags");
        return false;
    }
    Reader headerReader(wire.mid(1));
    quint32 remainingLength = 0;
    if (!readVariableInteger(&headerReader, MaximumRemainingLength,
                             &remainingLength)) {
        setError(diagnosticCode, "remaining-length");
        return false;
    }
    const int lengthBytes = headerReader.position;
    if (remainingLength != static_cast<quint32>(wire.size() - 1 - lengthBytes)) {
        setError(diagnosticCode, "packet-truncated");
        return false;
    }
    Reader reader(wire.mid(1 + lengthBytes,
                           static_cast<int>(remainingLength)));
    decoded->type = type;
    decoded->flags = flags;
    switch (type) {
    case ConnAck: {
        if (reader.remaining() < 2 || !reader.take(&decoded->flags)
                || decoded->flags > 1 || !reader.take(&decoded->reasonCode)
                || (decoded->reasonCode != 0
                    && decoded->reasonCode != 156
                    && decoded->reasonCode != 157)
                || !readProperties(&reader, decoded, diagnosticCode)) {
            setError(diagnosticCode, "connack");
            return false;
        }
        decoded->sessionPresent = decoded->flags != 0;
        decoded->flags = header & 0x0f;
        decoded->event = EventConnAck;
        break;
    }
    case SubAck:
        if (reader.remaining() < 3
                || !reader.takeU16(&decoded->packetId)
                || !readProperties(&reader, decoded, diagnosticCode)) {
            setError(diagnosticCode, "suback");
            return false;
        }
        while (reader.remaining() > 0) {
            quint8 reason = 0;
            if (!reader.take(&reason)) {
                setError(diagnosticCode, "suback-reason");
                return false;
            }
            decoded->reasonCodes.append(reason);
        }
        if (decoded->reasonCodes.isEmpty()) {
            setError(diagnosticCode, "suback-reason");
            return false;
        }
        decoded->event = EventSubAck;
        break;
    case Publish: {
        quint16 topicLength = 0;
        if (!reader.takeU16(&topicLength)
                || !reader.takeBytes(topicLength, &decoded->topic)
                || !boundedUtf8(decoded->topic, MaximumTopicBytes)
                || decoded->topic.contains('#')
                || decoded->topic.contains('+')) {
            setError(diagnosticCode, "publish-topic");
            return false;
        }
        const quint8 qos = (flags >> 1) & 0x03;
        if (qos != 0 && !reader.takeU16(&decoded->packetId)) {
            setError(diagnosticCode, "publish-packet-id");
            return false;
        }
        if (!readProperties(&reader, decoded, diagnosticCode)) {
            setError(diagnosticCode, "publish-properties");
            return false;
        }
        if (!reader.takeBytes(reader.remaining(), &decoded->payload)
                || decoded->payload.size() > MaximumPayloadBytes) {
            setError(diagnosticCode, "publish-payload");
            return false;
        }
        int typePropertyCount = 0;
        for (int i = 0; i < decoded->userProperties.size(); ++i) {
            if (decoded->userProperties.at(i).key == QByteArrayLiteral("type")) {
                if (++typePropertyCount != 1) {
                    setError(diagnosticCode, "publish-type");
                    return false;
                }
                decoded->publishType = decoded->userProperties.at(i).value;
                decoded->eventType = QString::fromUtf8(decoded->publishType);
            }
        }
        if (typePropertyCount != 1
                || !boundedUtf8(decoded->publishType, MaximumTypeBytes)) {
            setError(diagnosticCode, "publish-type");
            return false;
        }
        decoded->event = EventPublish;
        break;
    }
    case PingReq:
        if (reader.remaining() != 0) {
            setError(diagnosticCode, "pingreq");
            return false;
        }
        decoded->event = EventPingReq;
        break;
    case PingResp:
        if (reader.remaining() != 0) {
            setError(diagnosticCode, "pingresp");
            return false;
        }
        decoded->event = EventPingResp;
        break;
    case Disconnect:
        if (!readReasonAndProperties(&reader, decoded, diagnosticCode)
                || reader.remaining() != 0) {
            setError(diagnosticCode, "disconnect");
            return false;
        }
        decoded->event = EventDisconnect;
        break;
    default:
        setError(diagnosticCode, "packet-type");
        return false;
    }
    if (!reader.ok || reader.remaining() != 0) {
        setError(diagnosticCode, "packet-body");
        return false;
    }
    return true;
}

bool NativeMqttProtocol::parsePacket(const QByteArray &wire,
                                     NativeMqttPacket *decoded,
                                     QByteArray *diagnosticCode)
{
    if (!decoded) {
        setError(diagnosticCode, "packet-output");
        return false;
    }
    Packet packet;
    if (!parse(wire, &packet, diagnosticCode)) {
        *decoded = NativeMqttPacket();
        return false;
    }
    decoded->type = static_cast<NativeMqttPacket::Type>(packet.type);
    decoded->reasonCode = packet.reasonCode;
    decoded->packetId = packet.packetId;
    decoded->eventType = packet.eventType;
    decoded->payload = packet.payload;
    decoded->serverReference = packet.serverReference;
    return true;
}
