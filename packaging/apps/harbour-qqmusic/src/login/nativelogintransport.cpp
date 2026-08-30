#include "nativelogintransport.h"

#include <QCryptographicHash>
#include <QSslError>
#include <QSslSocket>
#include <QUuid>

namespace {

const int MaximumHandshakeBytes = 16 * 1024;
const int MaximumWebSocketMessageBytes = 256 * 1024;
const QByteArray WebSocketGuid =
    QByteArrayLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

bool hasControlBytes(const QByteArray &value)
{
    for (int index = 0; index < value.size(); ++index) {
        const uchar byte = static_cast<uchar>(value.at(index));
        if (byte < 0x20 || byte == 0x7f) {
            return true;
        }
    }
    return false;
}

bool allowedHeader(const QByteArray &name)
{
    return name == QByteArrayLiteral("Origin")
        || name == QByteArrayLiteral("Referer")
        || name == QByteArrayLiteral("User-Agent")
        || name == QByteArrayLiteral("Sec-WebSocket-Protocol");
}

bool hasHeaderToken(const QByteArray &value, const QByteArray &expected)
{
    const QList<QByteArray> tokens = value.split(',');
    for (int index = 0; index < tokens.size(); ++index) {
        if (tokens.at(index).trimmed().toLower() == expected) {
            return true;
        }
    }
    return false;
}

QByteArray maskedFrame(quint8 opcode, const QByteArray &payload)
{
    if (payload.size() > MaximumWebSocketMessageBytes) {
        return QByteArray();
    }
    QByteArray frame;
    frame.append(static_cast<char>(0x80 | opcode));
    const quint64 length = static_cast<quint64>(payload.size());
    if (length < 126) {
        frame.append(static_cast<char>(0x80 | quint8(length)));
    } else if (length <= 0xffff) {
        frame.append(static_cast<char>(0x80 | 126));
        frame.append(static_cast<char>((length >> 8) & 0xff));
        frame.append(static_cast<char>(length & 0xff));
    } else {
        frame.append(static_cast<char>(0x80 | 127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.append(static_cast<char>((length >> shift) & 0xff));
        }
    }
    const QByteArray mask = QUuid::createUuid().toRfc4122().left(4);
    if (mask.size() != 4) {
        return QByteArray();
    }
    frame.append(mask);
    for (int index = 0; index < payload.size(); ++index) {
        frame.append(payload.at(index) ^ mask.at(index % mask.size()));
    }
    return frame;
}

}

NativeWebSocketTransport::NativeWebSocketTransport(QObject *parent)
    : NativeLoginTransport(parent)
    , m_socket(new QSslSocket(this))
    , m_phase(Idle)
    , m_failed(false)
    , m_fragmentOpcode(0)
{
    m_socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_socket->setReadBufferSize(MaximumWebSocketMessageBytes
                                + MaximumHandshakeBytes);
    connect(m_socket, &QSslSocket::encrypted,
            this, &NativeWebSocketTransport::socketEncrypted);
    connect(m_socket, &QSslSocket::readyRead,
            this, &NativeWebSocketTransport::socketReadyRead);
    connect(m_socket, &QSslSocket::disconnected,
            this, &NativeWebSocketTransport::socketDisconnected);
    connect(m_socket,
            static_cast<void (QAbstractSocket::*)(
                QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &NativeWebSocketTransport::socketError);
    connect(m_socket,
            static_cast<void (QSslSocket::*)(const QList<QSslError> &)>(
                &QSslSocket::sslErrors),
            this, &NativeWebSocketTransport::socketSslErrors);
}

bool NativeWebSocketTransport::open(
    const QUrl &url,
    const QMap<QByteArray, QByteArray> &headers)
{
    if (!url.isValid() || url.scheme() != QStringLiteral("wss")
            || url.host() != QStringLiteral("mu.y.qq.com")
            || (url.path() != QStringLiteral("/ws/handshake")
                && !url.path().startsWith(
                    QStringLiteral("/ws/handshake/")))
            || !url.userInfo().isEmpty() || url.port(-1) != -1
            || !url.query().isEmpty() || !url.fragment().isEmpty()) {
        return false;
    }
    if (headers.size() != 4
            || headers.value(QByteArrayLiteral("Origin"))
                != QByteArrayLiteral("https://y.qq.com")
            || headers.value(QByteArrayLiteral("Referer"))
                != QByteArrayLiteral("https://y.qq.com/")
            || headers.value(QByteArrayLiteral("Sec-WebSocket-Protocol"))
                != QByteArrayLiteral("mqtt")) {
        return false;
    }
    for (QMap<QByteArray, QByteArray>::const_iterator it = headers.constBegin();
         it != headers.constEnd(); ++it) {
        if (!allowedHeader(it.key()) || it.value().isEmpty()
                || it.value().size() > 512 || hasControlBytes(it.value())) {
            return false;
        }
    }
    close();
    m_failed = false;
    m_phase = Encrypting;
    m_url = url;
    m_headers = headers;
    m_socket->setPeerVerifyName(url.host());
    m_socket->connectToHostEncrypted(url.host(), 443);
    return true;
}

bool NativeWebSocketTransport::sendBinaryMessage(const QByteArray &message)
{
    return !m_failed && m_phase == Open && !message.isEmpty()
        && sendFrame(0x02, message);
}

void NativeWebSocketTransport::close()
{
    m_phase = Idle;
    m_failed = false;
    resetBuffers();
    m_socket->abort();
}

void NativeWebSocketTransport::socketEncrypted()
{
    if (m_phase != Encrypting) {
        return;
    }
    m_handshakeKey = QUuid::createUuid().toRfc4122().toBase64();
    if (m_handshakeKey.isEmpty()) {
        failClosed(QStringLiteral("qq.login.websocket.handshake.key"));
        return;
    }
    QByteArray request = QByteArrayLiteral("GET ")
        + m_url.path(QUrl::FullyEncoded).toLatin1()
        + QByteArrayLiteral(" HTTP/1.1\r\nHost: mu.y.qq.com\r\n")
        + QByteArrayLiteral("Upgrade: websocket\r\nConnection: Upgrade\r\n")
        + QByteArrayLiteral("Sec-WebSocket-Key: ") + m_handshakeKey
        + QByteArrayLiteral("\r\nSec-WebSocket-Version: 13\r\n")
        + QByteArrayLiteral("Sec-WebSocket-Protocol: mqtt\r\nOrigin: ")
        + m_headers.value(QByteArrayLiteral("Origin"))
        + QByteArrayLiteral("\r\nReferer: ")
        + m_headers.value(QByteArrayLiteral("Referer"))
        + QByteArrayLiteral("\r\nUser-Agent: ")
        + m_headers.value(QByteArrayLiteral("User-Agent"))
        + QByteArrayLiteral("\r\n\r\n");
    if (request.size() > MaximumHandshakeBytes
            || m_socket->write(request) != request.size()) {
        failClosed(QStringLiteral("qq.login.websocket.handshake.send"));
        return;
    }
    m_phase = Handshaking;
}

void NativeWebSocketTransport::socketReadyRead()
{
    if (m_phase == Idle || m_failed) {
        return;
    }
    m_input.append(m_socket->readAll());
    if ((m_phase == Handshaking && m_input.size() > MaximumHandshakeBytes)
            || (m_phase == Open
                && m_input.size() > MaximumWebSocketMessageBytes
                    + MaximumHandshakeBytes)) {
        failClosed(QStringLiteral("qq.login.websocket.receive.size"));
        return;
    }
    if (m_phase == Handshaking && !parseHandshake()) {
        return;
    }
    if (m_phase == Open) {
        parseFrames();
    }
}

void NativeWebSocketTransport::socketDisconnected()
{
    const bool notify = m_phase != Idle;
    m_phase = Idle;
    resetBuffers();
    if (notify) {
        emit disconnected();
    }
}

void NativeWebSocketTransport::socketError(QAbstractSocket::SocketError)
{
    if (m_phase != Idle) {
        failClosed(QStringLiteral("qq.login.websocket.network"));
    }
}

void NativeWebSocketTransport::socketSslErrors(const QList<QSslError> &)
{
    if (m_phase != Idle) {
        failClosed(QStringLiteral("qq.login.websocket.tls"));
    }
}

void NativeWebSocketTransport::failClosed(const QString &diagnosticCode)
{
    if (m_failed) {
        return;
    }
    m_failed = true;
    emit failed(diagnosticCode);
    m_socket->abort();
}

bool NativeWebSocketTransport::sendFrame(quint8 opcode,
                                         const QByteArray &payload)
{
    if (payload.size() > MaximumWebSocketMessageBytes
            || ((opcode & 0x08) != 0 && payload.size() > 125)) {
        return false;
    }
    const QByteArray frame = maskedFrame(opcode, payload);
    return !frame.isEmpty() && m_socket->write(frame) == frame.size();
}

bool NativeWebSocketTransport::parseHandshake()
{
    const int headerEnd = m_input.indexOf(QByteArrayLiteral("\r\n\r\n"));
    if (headerEnd < 0) {
        return false;
    }
    const QByteArray headerBytes = m_input.left(headerEnd);
    m_input.remove(0, headerEnd + 4);
    const QList<QByteArray> lines = headerBytes.split('\n');
    if (lines.isEmpty()
            || lines.first().trimmed() != QByteArrayLiteral("HTTP/1.1 101 Switching Protocols")) {
        failClosed(QStringLiteral("qq.login.websocket.handshake.status"));
        return false;
    }
    QMap<QByteArray, QByteArray> responseHeaders;
    for (int index = 1; index < lines.size(); ++index) {
        const QByteArray line = lines.at(index).trimmed();
        const int separator = line.indexOf(':');
        if (line.isEmpty() || separator < 1
                || line.at(0) == ' ' || line.at(0) == '\t') {
            failClosed(QStringLiteral("qq.login.websocket.handshake.header"));
            return false;
        }
        const QByteArray name = line.left(separator).trimmed().toLower();
        const QByteArray value = line.mid(separator + 1).trimmed();
        if (name.isEmpty() || value.isEmpty() || hasControlBytes(value)
                || responseHeaders.contains(name)) {
            failClosed(QStringLiteral("qq.login.websocket.handshake.header"));
            return false;
        }
        responseHeaders.insert(name, value);
    }
    const QByteArray expectedAccept = QCryptographicHash::hash(
        m_handshakeKey + WebSocketGuid,
        QCryptographicHash::Sha1).toBase64();
    if (responseHeaders.value(QByteArrayLiteral("upgrade")).toLower()
                != QByteArrayLiteral("websocket")
            || !hasHeaderToken(
                responseHeaders.value(QByteArrayLiteral("connection")),
                QByteArrayLiteral("upgrade"))
            || responseHeaders.value(
                QByteArrayLiteral("sec-websocket-accept")) != expectedAccept
            || responseHeaders.value(
                QByteArrayLiteral("sec-websocket-protocol"))
                != QByteArrayLiteral("mqtt")
            || responseHeaders.contains(
                QByteArrayLiteral("sec-websocket-extensions"))) {
        failClosed(QStringLiteral("qq.login.websocket.handshake.contract"));
        return false;
    }
    m_phase = Open;
    emit connected();
    return true;
}

bool NativeWebSocketTransport::parseFrames()
{
    while (m_input.size() >= 2) {
        const quint8 first = static_cast<quint8>(m_input.at(0));
        const quint8 second = static_cast<quint8>(m_input.at(1));
        const bool finalFrame = (first & 0x80) != 0;
        const quint8 opcode = first & 0x0f;
        if ((first & 0x70) != 0 || (second & 0x80) != 0) {
            failClosed(QStringLiteral("qq.login.websocket.frame.flags"));
            return false;
        }
        int headerSize = 2;
        quint64 payloadSize = second & 0x7f;
        if (payloadSize == 126) {
            if (m_input.size() < 4) {
                return true;
            }
            payloadSize = (static_cast<quint8>(m_input.at(2)) << 8)
                | static_cast<quint8>(m_input.at(3));
            headerSize = 4;
            if (payloadSize < 126) {
                failClosed(QStringLiteral("qq.login.websocket.frame.length"));
                return false;
            }
        } else if (payloadSize == 127) {
            if (m_input.size() < 10) {
                return true;
            }
            payloadSize = 0;
            for (int index = 2; index < 10; ++index) {
                payloadSize = (payloadSize << 8)
                    | static_cast<quint8>(m_input.at(index));
            }
            headerSize = 10;
            if (payloadSize <= 0xffff) {
                failClosed(QStringLiteral("qq.login.websocket.frame.length"));
                return false;
            }
        }
        if (payloadSize > quint64(MaximumWebSocketMessageBytes)
                || payloadSize > quint64(m_input.size() - headerSize)) {
            if (payloadSize > quint64(MaximumWebSocketMessageBytes)) {
                failClosed(QStringLiteral("qq.login.websocket.frame.size"));
                return false;
            }
            return true;
        }
        const QByteArray payload = m_input.mid(
            headerSize, static_cast<int>(payloadSize));
        m_input.remove(0, headerSize + static_cast<int>(payloadSize));
        const bool control = (opcode & 0x08) != 0;
        if (control && (!finalFrame || payload.size() > 125)) {
            failClosed(QStringLiteral("qq.login.websocket.frame.control"));
            return false;
        }
        if (opcode == 0x02) {
            if (m_fragmentOpcode != 0) {
                failClosed(QStringLiteral("qq.login.websocket.frame.order"));
                return false;
            }
            if (finalFrame) {
                if (payload.isEmpty()) {
                    failClosed(QStringLiteral("qq.login.websocket.message.size"));
                    return false;
                }
                emit binaryMessageReceived(payload);
            } else {
                m_fragmentOpcode = opcode;
                m_fragment = payload;
            }
        } else if (opcode == 0x00) {
            if (m_fragmentOpcode != 0x02
                    || m_fragment.size() + payload.size()
                        > MaximumWebSocketMessageBytes) {
                failClosed(QStringLiteral("qq.login.websocket.frame.order"));
                return false;
            }
            m_fragment.append(payload);
            if (finalFrame) {
                if (m_fragment.isEmpty()) {
                    failClosed(QStringLiteral("qq.login.websocket.message.size"));
                    return false;
                }
                const QByteArray message = m_fragment;
                m_fragment.clear();
                m_fragmentOpcode = 0;
                emit binaryMessageReceived(message);
            }
        } else if (opcode == 0x09) {
            if (!sendFrame(0x0a, payload)) {
                failClosed(QStringLiteral("qq.login.websocket.pong"));
                return false;
            }
        } else if (opcode == 0x0a) {
            continue;
        } else if (opcode == 0x08) {
            if (payload.size() == 1) {
                failClosed(QStringLiteral("qq.login.websocket.close.frame"));
                return false;
            }
            sendFrame(0x08, payload);
            m_socket->disconnectFromHost();
            return true;
        } else {
            failClosed(opcode == 0x01
                           ? QStringLiteral("qq.login.websocket.message.text")
                           : QStringLiteral("qq.login.websocket.frame.opcode"));
            return false;
        }
    }
    return true;
}

void NativeWebSocketTransport::resetBuffers()
{
    m_url.clear();
    m_headers.clear();
    m_handshakeKey.clear();
    m_input.clear();
    m_fragment.clear();
    m_fragmentOpcode = 0;
}
