#ifndef NATIVELOGINTRANSPORT_H
#define NATIVELOGINTRANSPORT_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QUrl>

class QSslError;
class QSslSocket;

class NativeLoginTransport : public QObject
{
    Q_OBJECT

public:
    explicit NativeLoginTransport(QObject *parent = 0) : QObject(parent) {}
    ~NativeLoginTransport() override = default;

    virtual bool open(const QUrl &url,
                      const QMap<QByteArray, QByteArray> &headers) = 0;
    virtual bool sendBinaryMessage(const QByteArray &message) = 0;
    virtual void close() = 0;

signals:
    void connected();
    void binaryMessageReceived(const QByteArray &message);
    void disconnected();
    void failed(const QString &diagnosticCode);
};

class NativeWebSocketTransport final : public NativeLoginTransport
{
    Q_OBJECT

public:
    explicit NativeWebSocketTransport(QObject *parent = 0);

    bool open(const QUrl &url,
              const QMap<QByteArray, QByteArray> &headers) override;
    bool sendBinaryMessage(const QByteArray &message) override;
    void close() override;

private slots:
    void socketEncrypted();
    void socketReadyRead();
    void socketDisconnected();
    void socketError(QAbstractSocket::SocketError error);
    void socketSslErrors(const QList<QSslError> &errors);

private:
    enum Phase {
        Idle,
        Encrypting,
        Handshaking,
        Open
    };

    void failClosed(const QString &diagnosticCode);
    bool sendFrame(quint8 opcode, const QByteArray &payload);
    bool parseHandshake();
    bool parseFrames();
    void resetBuffers();

    QSslSocket *m_socket;
    Phase m_phase;
    bool m_failed;
    QUrl m_url;
    QMap<QByteArray, QByteArray> m_headers;
    QByteArray m_handshakeKey;
    QByteArray m_input;
    QByteArray m_fragment;
    quint8 m_fragmentOpcode;
};

#endif
