#include "pushnotifications.h"
#include "creds/abstractcredentials.h"
#include "account.h"

namespace OCC {

PushNotifications::PushNotifications(Account *account)
    : _account(account)
{
}

PushNotificationTypes PushNotifications::pushNotificationsAvailable() const
{
    return _account->capabilities().pushNotificationsAvailable();
}

void PushNotifications::setup()
{
    connectWebSocket();
}

void PushNotifications::reset()
{
    setup();
}

void PushNotifications::connectWebSocket()
{
    disconnectWebSocket();
    openWebSocket();
}

void PushNotifications::disconnectWebSocket()
{
    if (_webSocket) {
        qInfo() << "Disconnect from websocket";
        _webSocket->close();

        // Disconnect signal handlers
        disconnect(_webSocket.data(), &QWebSocket::connected, this, &PushNotifications::onWebSocketConnected);
        disconnect(_webSocket.data(), &QWebSocket::disconnected, this, &PushNotifications::onWebSocketDisconnected);
        disconnect(_webSocket.data(), &QWebSocket::sslErrors, this, &PushNotifications::onWebSocketSslErrors);
        disconnect(_webSocket.data(), QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &PushNotifications::onWebSocketError);
    }
}

void PushNotifications::onWebSocketConnected()
{
    disconnect(_webSocket.data(), &QWebSocket::textMessageReceived, this, &PushNotifications::onWebSocketTextMessageReceived);
    connect(_webSocket.data(), &QWebSocket::textMessageReceived, this, &PushNotifications::onWebSocketTextMessageReceived);

    qInfo() << "Connected to websocket";

    authenticateOnWebSocket();
}

void PushNotifications::authenticateOnWebSocket()
{
    const auto credentials = _account->credentials();
    const auto username = credentials->user();
    const auto password = credentials->password();

    // Authenticate
    _webSocket->sendTextMessage(username);
    _webSocket->sendTextMessage(password);
}

void PushNotifications::onWebSocketDisconnected()
{
    qInfo() << "Disconnected from websocket";
}

void PushNotifications::onWebSocketTextMessageReceived(const QString &message)
{
    qInfo() << "Received push notification:" << message;

    if (message == "notify_file") {
        qInfo() << "Files push notifications arrived";
        emit filesChanged(_account);
    } else if (message == "authenticated") {
        qInfo() << "Authenticated successful on websocket";
    } else if (message == "err: Invalid credentials") {
        qInfo() << "Invalid credentials submitted to websocket";
        connectWebSocket();
    }
}

void PushNotifications::onWebSocketError(QAbstractSocket::SocketError error)
{
    qWarning() << "Received web socket error: " << error;

    switch (error) {
        // TODO: Maybe few more cases go here that need an reconnect ?
    case QAbstractSocket::NetworkError:
        connectWebSocket();
        break;
    default:;
        // TODO: How to handle such a case?
        Q_ASSERT(false && "Unexpected network error not handled");
    }
}

void PushNotifications::onWebSocketSslErrors(const QList<QSslError> &errors)
{
    qWarning() << "Received web socket ssl errors: " << errors;
    // TODO: What to do with them?
}

void PushNotifications::openWebSocket()
{
    // Open websocket
    const auto capabilities = _account->capabilities();
    const auto webSocketUrl = capabilities.pushNotificationWebSocketUrl();

    if (!_webSocket) {
        qInfo() << "Create websocket";
        _webSocket = QSharedPointer<QWebSocket>(new QWebSocket);
    }

    if (_webSocket) {
        connect(_webSocket.data(), QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &PushNotifications::onWebSocketError);
        connect(_webSocket.data(), &QWebSocket::sslErrors, this, &PushNotifications::onWebSocketSslErrors);
        connect(_webSocket.data(), &QWebSocket::connected, this, &PushNotifications::onWebSocketConnected);
        connect(_webSocket.data(), &QWebSocket::disconnected, this, &PushNotifications::onWebSocketDisconnected);

        qInfo() << "Open connection to websocket on:" << webSocketUrl;
        _webSocket->open(webSocketUrl);
    }
}
}
