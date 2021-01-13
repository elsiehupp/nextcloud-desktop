#ifndef PUSHNOTIFICATIONS_H
#define PUSHNOTIFICATIONS_H

#include "websocket.h"
#include "capabilities.h"

namespace OCC {

class Account;
class AbstractCredentials;

class PushNotifications : public QObject
{
    Q_OBJECT

public:
    // Question: Is it okay to pass a raw account pointer? Because PushNotifications don't really own the account. The PushNotifications
    // object is owned by the account state. Why not passing AccountState? Maybe would be better but this makes testing more difficult.
    PushNotifications(Account *account, QSharedPointer<AbstractWebSocket> webSocket = QSharedPointer<AbstractWebSocket>(new WebSocket));
    void reconnect();
    PushNotificationTypes pushNotificationsAvailable() const;

signals:
    void filesChanged(Account *account);

private:
    void openWebSocket();
    void connectWebSocket();
    void disconnectWebSocket();
    void authenticateOnWebSocket();
    void setupFilesPushNotifications();

    Account *_account = nullptr;
    QSharedPointer<AbstractWebSocket> _webSocket = nullptr;

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketTextMessageReceived(const QString &message);
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketSslErrors(const QList<QSslError> &errors);
};

}

#endif // PUSHNOTIFICATIONS_H
