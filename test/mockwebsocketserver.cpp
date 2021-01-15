#include <QLoggingCategory>

#include "mockwebsocketserver.h"

Q_LOGGING_CATEGORY(lcMockWebSocketServer, "nextcloud.test.mockwebserver", QtInfoMsg)

MockWebSocketServer::MockWebSocketServer(quint16 port, QObject *parent)
    : QObject(parent)
    , _webSocketServer(new QWebSocketServer(QStringLiteral("Mock Server"), QWebSocketServer::NonSecureMode, this))
{
    if (_webSocketServer->listen(QHostAddress::Any, port)) {
        connect(_webSocketServer, &QWebSocketServer::newConnection, this, &MockWebSocketServer::onNewConnection);
        connect(_webSocketServer, &QWebSocketServer::closed, this, &MockWebSocketServer::closed);
        qCInfo(lcMockWebSocketServer) << "Open mock websocket server on port:" << port;
        return;
    }
    Q_UNREACHABLE();
}

MockWebSocketServer::~MockWebSocketServer()
{
    qCInfo(lcMockWebSocketServer) << "Close mock websocket server";

    _webSocketServer->close();
    qDeleteAll(_clients.begin(), _clients.end());

    delete _webSocketServer;
}

void MockWebSocketServer::processTextMessageInternal(const QString &message)
{
    auto client = qobject_cast<QWebSocket *>(sender());
    emit processTextMessage(client, message);
}

void MockWebSocketServer::onNewConnection()
{
    qCInfo(lcMockWebSocketServer) << "New connection on mock websocket server";

    auto socket = _webSocketServer->nextPendingConnection();

    connect(socket, &QWebSocket::textMessageReceived, this, &MockWebSocketServer::processTextMessageInternal);
    connect(socket, &QWebSocket::disconnected, this, &MockWebSocketServer::socketDisconnected);

    _clients << socket;
}

void MockWebSocketServer::socketDisconnected()
{
    qCInfo(lcMockWebSocketServer) << "Socket disconnected";

    auto client = qobject_cast<QWebSocket *>(sender());

    if (client) {
        _clients.removeAll(client);
        client->deleteLater();
    }
}

#include "mockwebsocketserver.moc"
