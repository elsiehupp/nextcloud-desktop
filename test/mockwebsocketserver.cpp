#include "mockwebsocketserver.h"

MockWebSocketServer::MockWebSocketServer(quint16 port)
    : _webSocketServer(new QWebSocketServer(QStringLiteral("Mock Server"), QWebSocketServer::NonSecureMode, this))
{
    if (_webSocketServer->listen(QHostAddress::Any, port)) {
        connect(_webSocketServer, &QWebSocketServer::newConnection, this, &MockWebSocketServer::onNewConnection);
        connect(_webSocketServer, &QWebSocketServer::closed, this, &MockWebSocketServer::closed);
        qInfo() << "Open mock websocket server on port:" << port;
        return;
    }
    Q_UNREACHABLE();
}

MockWebSocketServer::~MockWebSocketServer()
{
    qInfo() << "Close mock websocket server";
    _webSocketServer->close();
    qDeleteAll(_clients.begin(), _clients.end());

    delete _webSocketServer;
}

void MockWebSocketServer::sendTextMessage(const QString &message)
{
    Q_ASSERT(_clients.size() > 0);
    _clients[0]->sendTextMessage(message);
}

void MockWebSocketServer::processTextMessageInternal(QString message)
{
    auto client = qobject_cast<QWebSocket *>(sender());
    emit processTextMessage(client, message);
}

void MockWebSocketServer::onNewConnection()
{
    qInfo() << "New connection on mock websocket server";
    auto socket = _webSocketServer->nextPendingConnection();

    connect(socket, &QWebSocket::textMessageReceived, this, &MockWebSocketServer::processTextMessageInternal);
    connect(socket, &QWebSocket::disconnected, this, &MockWebSocketServer::socketDisconnected);

    _clients << socket;
}

void MockWebSocketServer::socketDisconnected()
{
    qInfo() << "Socket disconnected";
    auto client = qobject_cast<QWebSocket *>(sender());

    if (client) {
        _clients.removeAll(client);
        client->deleteLater();
    }
}

void MockWebSocketServer::setRequestsWithResponses(const QVector<RequestWithResponse> &messages)
{
    _requestsWithResponses = messages;
}

#include "mockwebsocketserver.moc"
