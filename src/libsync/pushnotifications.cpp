#include "pushnotifications.h"
#include "creds/abstractcredentials.h"
#include "websocket.h"
#include "account.h"

namespace OCC {

PushNotifications::PushNotifications(Account *account, QSharedPointer<AbstractWebSocket> webSocket)
    : _account(account)
    , _webSocket(webSocket)
{
}

bool PushNotifications::filesPushNotificationsAvailable() const
{
    return _account->capabilities().filesPushNotificationsAvailable();
}

void PushNotifications::reconnect()
{
    connectWebSocket();
}

void PushNotifications::connectWebSocket()
{
    disconnectWebSocket();

    setupFilesPushNotifications();

    openWebSocket();
}

void PushNotifications::disconnectWebSocket()
{
    _webSocket->close();
}

void PushNotifications::onWebSocketConnected()
{
    disconnect(_webSocket.data(), &AbstractWebSocket::textMessageReceived, this, &PushNotifications::onWebSocketTextMessageReceived);
    connect(_webSocket.data(), &AbstractWebSocket::textMessageReceived, this, &PushNotifications::onWebSocketTextMessageReceived);

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

void PushNotifications::onWebSocketDisconnected() { }

void PushNotifications::onWebSocketTextMessageReceived(const QString &message)
{
    if (message == "notify_file") {
        emit filesChanged(_account);
    } else if (message == "authenticated") {
        // TODO: Log
    } else if (message == "err: Invalid credentials") {
        connectWebSocket();
    }
}

void PushNotifications::onWebSocketError(QAbstractSocket::SocketError error)
{
    switch (error) {
        // TODO: Maybe few more cases go here that need an reconnect ?
    case QAbstractSocket::NetworkError:
        connectWebSocket();
        break;
    default:;
        // TODO: Log unhandled errors
        Q_ASSERT(false && "Unexpected network error not handled");
    }
}

void PushNotifications::onWebSocketSslErrors(const QList<QSslError> &errors)
{
    // TODO: What to do with them?
}

void PushNotifications::setupFilesPushNotifications()
{
    const auto capabilities = _account->capabilities();

    // Check for files push notifications
    if (!capabilities.filesPushNotificationsAvailable())
        return;

    // Disconnect signal handlers
    disconnect(_webSocket.data(), &AbstractWebSocket::connected, this, &PushNotifications::onWebSocketConnected);
    disconnect(_webSocket.data(), &AbstractWebSocket::connected, this, &PushNotifications::onWebSocketConnected);
    disconnect(_webSocket.data(), &AbstractWebSocket::error, this, &PushNotifications::onWebSocketError);

    // Reconnect them
    connect(_webSocket.data(), &AbstractWebSocket::error, this, &PushNotifications::onWebSocketError);
    connect(_webSocket.data(), &AbstractWebSocket::connected, this, &PushNotifications::onWebSocketConnected);
    connect(_webSocket.data(), &AbstractWebSocket::disconnected, this, &PushNotifications::onWebSocketDisconnected);
}

void PushNotifications::openWebSocket()
{
    // Open websocket
    const auto capabilities = _account->capabilities();
    const auto webSocketUrl = capabilities.pushNotificationWebSocketUrl();
    _webSocket->open(QUrl(webSocketUrl));
}
}
