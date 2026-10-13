#include <QtTest>
#include <QCryptographicHash>
#include "login/nativelogintransport.h"

class NativeLoginTransportTest : public QObject
{
    Q_OBJECT
private slots:
    void acceptsUpgradeReasonPhrases_data()
    {
        QTest::addColumn<QByteArray>("status");
        QTest::newRow("standard") << QByteArray("HTTP/1.1 101 Switching Protocols");
        QTest::newRow("alternate") << QByteArray("HTTP/1.1 101 WebSocket Protocol Handshake");
        QTest::newRow("empty") << QByteArray("HTTP/1.1 101");
    }
    void acceptsUpgradeReasonPhrases()
    {
        QFETCH(QByteArray, status);
        NativeWebSocketTransport transport;
        QSignalSpy connected(&transport, SIGNAL(connected()));
        const QByteArray key("test-handshake-key");
        const QByteArray accept = QCryptographicHash::hash(
            key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
            QCryptographicHash::Sha1).toBase64();
        transport.m_handshakeKey = key;
        transport.m_phase = NativeWebSocketTransport::Handshaking;
        transport.m_input = status + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Protocol: mqtt\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n";
        QVERIFY(transport.parseHandshake());
        QCOMPARE(connected.count(), 1);
    }
    void rejectsInvalidUpgrade_data()
    {
        QTest::addColumn<QByteArray>("status");
        QTest::newRow("redirect") << QByteArray("HTTP/1.1 302 Found");
        QTest::newRow("wrong-code") << QByteArray("HTTP/1.1 200 OK");
        QTest::newRow("code-suffix") << QByteArray("HTTP/1.1 1010");
        QTest::newRow("invalid-headers") << QByteArray("HTTP/1.1 101");
    }
    void rejectsInvalidUpgrade()
    {
        QFETCH(QByteArray, status);
        NativeWebSocketTransport transport;
        QSignalSpy connected(&transport, SIGNAL(connected()));
        QSignalSpy failed(&transport, SIGNAL(failed(QString)));
        transport.m_phase = NativeWebSocketTransport::Handshaking;
        transport.m_input = status + "\r\nUpgrade: websocket\r\n\r\n";
        QVERIFY(!transport.parseHandshake());
        QCOMPARE(connected.count(), 0);
        QCOMPARE(failed.count(), 1);
    }
    void reassemblesFragmentedFramesAndStopsOnClose()
    {
        NativeWebSocketTransport transport;
        QSignalSpy messages(&transport, SIGNAL(binaryMessageReceived(QByteArray)));
        transport.m_phase = NativeWebSocketTransport::Open;
        transport.m_input = QByteArray::fromHex("0202c0");
        QVERIFY(transport.parseFrames());
        QCOMPARE(messages.count(), 0);
        transport.m_input.append(QByteArray::fromHex("0080008202d000"));
        QVERIFY(transport.parseFrames());
        QCOMPARE(messages.count(), 2);
        QCOMPARE(messages.at(0).at(0).toByteArray(), QByteArray::fromHex("c000"));
        QCOMPARE(messages.at(1).at(0).toByteArray(), QByteArray::fromHex("d000"));
        connect(&transport, &NativeLoginTransport::binaryMessageReceived,
                &transport, &NativeLoginTransport::close);
        transport.m_input = QByteArray::fromHex("8202c0008202d000");
        QVERIFY(transport.parseFrames());
        QCOMPARE(messages.count(), 3);
        QVERIFY(transport.m_input.isEmpty());
    }
    void rejectsMaskedServerFramesAndOversizedPayloads()
    {
        const QList<QByteArray> invalid = QList<QByteArray>()
            << QByteArray::fromHex("8280")
            << QByteArray::fromHex("827f0000000000040001")
            << QByteArray::fromHex("8100");
        for (const QByteArray &bytes : invalid) {
            NativeWebSocketTransport transport;
            QSignalSpy failed(&transport, SIGNAL(failed(QString)));
            transport.m_phase = NativeWebSocketTransport::Open;
            transport.m_input = bytes;
            QVERIFY(!transport.parseFrames());
            QCOMPARE(failed.count(), 1);
        }
    }
};

QTEST_GUILESS_MAIN(NativeLoginTransportTest)
#include "tst_nativelogintransport.moc"
