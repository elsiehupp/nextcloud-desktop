#include <QTest>
#include <chrono>
#include <thread>

#include "websocket.h"
#include "common.h"

using namespace std::chrono_literals;

QSharedPointer<OCC::WebSocket>
createWebSocket()
{
    return QSharedPointer<OCC::WebSocket>(new OCC::WebSocket);
}

class TestWebSocket : public QObject
{
    Q_OBJECT

private slots:
    void testConnect()
    {
        auto connected = false;

        auto webSocket = createWebSocket();
        connect(webSocket.data(), &OCC::WebSocket::connected, [&]() {
            connected = true;
        });

        emit webSocket->_webSocket.connected();
        wait(4ms);

        QCOMPARE(connected, true);
    }

    void testDisconnect()
    {
        auto disconnected = false;

        auto webSocket = createWebSocket();
        connect(webSocket.data(), &OCC::WebSocket::disconnected, [&]() {
            disconnected = true;
        });

        emit webSocket->_webSocket.disconnected();
        wait(4ms);

        QCOMPARE(disconnected, true);
    }

    void testTextMessageReceived()
    {
        QString messageToSent = "Hello World!";
        QString messageReceived = "";

        auto webSocket = createWebSocket();
        connect(webSocket.data(), &OCC::WebSocket::textMessageReceived, [&](const QString &message) {
            messageReceived = message;
        });

        emit webSocket->_webSocket.textMessageReceived(messageToSent);
        wait(4ms);

        QCOMPARE(messageReceived, messageToSent);
    }

    void testError()
    {
        QAbstractSocket::SocketError socketErrorReceived = QAbstractSocket::SocketError::UnknownSocketError;

        auto webSocket = createWebSocket();
        connect(webSocket.data(), &OCC::WebSocket::error, [&](QAbstractSocket::SocketError error) {
            socketErrorReceived = error;
        });

        emit webSocket->_webSocket.error(QAbstractSocket::SocketError::NetworkError);
        wait(4ms);

        QCOMPARE(socketErrorReceived, QAbstractSocket::SocketError::NetworkError);
    }

    void testSslErrors()
    {
        QList<QSslError> sslErrors;
        sslErrors.append(QSslError());
        QList<QSslError> sslErrorsReceived;

        auto webSocket = createWebSocket();
        connect(webSocket.data(), &OCC::WebSocket::sslErrors, [&](QList<QSslError> errors) {
            sslErrorsReceived = errors;
        });

        emit webSocket->_webSocket.sslErrors(sslErrors);
        wait(4ms);

        QCOMPARE(sslErrorsReceived.size(), 1);
        QCOMPARE(sslErrorsReceived[0], sslErrors[0]);
    }
};

QTEST_GUILESS_MAIN(TestWebSocket)
#include "testwebsocket.moc"
