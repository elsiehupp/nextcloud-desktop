#ifndef PUSHNOTIFICATIONS_H
#define PUSHNOTIFICATIONS_H

#include <QWebSocket>

#include "capabilities.h"

class TestPushNotifications;

namespace OCC {

class Account;
class AbstractCredentials;

class PushNotifications : public QObject
{
    Q_OBJECT

public:
    PushNotifications(Account *account);

    /**
     * Setup push notifications
     *
     * This method needs to be called before push notifications can be used.
     */
    void setup();

    /**
     * Reset push notifications
     *
     * After a call to this function the object will be in a state like after a call to setup.
     */
    void reset();

    /**
     * Get the types of push notifications available
     */
    PushNotificationTypes pushNotificationsAvailable() const;

signals:
    /**
     * Will be emitted if files on the server changed
     */
    void filesChanged(Account *account);

private:
    void openWebSocket();
    void connectWebSocket();
    void disconnectWebSocket();
    void authenticateOnWebSocket();

    Account *_account = nullptr;
    QSharedPointer<QWebSocket> _webSocket = nullptr;

    friend class ::TestPushNotifications;

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketTextMessageReceived(const QString &message);
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketSslErrors(const QList<QSslError> &errors);
};

}

#endif // PUSHNOTIFICATIONS_H
