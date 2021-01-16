#include "pushnotifications.h"
#include "creds/abstractcredentials.h"
#include "account.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcPushNotifications, "nextcloud.sync.pushnotifications", QtInfoMsg)

PushNotifications::PushNotifications(Account *account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
}

void PushNotifications::setup()
{
    _failedAuthenticationAttemptsCount = 0;
    reconnectToWebSocket();
}

void PushNotifications::reset()
{
    setup();
}

void PushNotifications::reconnectToWebSocket()
{
    closeWebSocket();
    openWebSocket();
}

void PushNotifications::closeWebSocket()
{
    if (_webSocket) {
        qCInfo(lcPushNotifications) << "Close websocket";

        _webSocket->close();
    }
}

void PushNotifications::onWebSocketConnected()
{
    qCInfo(lcPushNotifications) << "Connected to websocket";

    connect(_webSocket, &QWebSocket::textMessageReceived, this, &PushNotifications::onWebSocketTextMessageReceived, Qt::UniqueConnection);

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
    qCInfo(lcPushNotifications) << "Disconnected from websocket";
}

void PushNotifications::onWebSocketTextMessageReceived(const QString &message)
{
    qCInfo(lcPushNotifications) << "Received push notification:" << message;

    if (message == "notify_file") {
        qCInfo(lcPushNotifications) << "Files push notifications arrived";
        emit filesChanged(_account);
    } else if (message == "authenticated") {
        qCInfo(lcPushNotifications) << "Authenticated successful on websocket";
        _failedAuthenticationAttemptsCount = 0;
        emit ready();
    } else if (message == "err: Invalid credentials") {
        qCInfo(lcPushNotifications) << "Invalid credentials submitted to websocket";
        if (!tryReconnectToWebSocket()) {
            emit canNotAuthenticate();
        }
    }
}

void PushNotifications::onWebSocketError(QAbstractSocket::SocketError error)
{
    qCWarning(lcPushNotifications) << "Websocket error: " << error;

    emit connectionLost();
}

bool PushNotifications::tryReconnectToWebSocket()
{
    ++_failedAuthenticationAttemptsCount;
    if (_failedAuthenticationAttemptsCount >= _maxAllowedFailedAuthenticationAttempts) {
        return false;
    }

    reconnectToWebSocket();

    return true;
}

void PushNotifications::onWebSocketSslErrors(const QList<QSslError> &errors)
{
    qCWarning(lcPushNotifications) << "Received websocket ssl errors:" << errors;
    emit connectionLost();
}

void PushNotifications::openWebSocket()
{
    // Open websocket
    const auto capabilities = _account->capabilities();
    const auto webSocketUrl = capabilities.pushNotificationsWebSocketUrl();

    if (!_webSocket) {
        qCInfo(lcPushNotifications) << "Create websocket";
        _webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    }

    if (_webSocket) {
        connect(_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &PushNotifications::onWebSocketError, Qt::UniqueConnection);
        connect(_webSocket, &QWebSocket::sslErrors, this, &PushNotifications::onWebSocketSslErrors, Qt::UniqueConnection);
        connect(_webSocket, &QWebSocket::connected, this, &PushNotifications::onWebSocketConnected, Qt::UniqueConnection);
        connect(_webSocket, &QWebSocket::disconnected, this, &PushNotifications::onWebSocketDisconnected, Qt::UniqueConnection);

        qCInfo(lcPushNotifications) << "Open connection to websocket on:" << webSocketUrl;
        _webSocket->open(webSocketUrl);
    }
}
}
