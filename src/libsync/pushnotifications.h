/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

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
    explicit PushNotifications(Account *account, QObject *parent = nullptr);

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

signals:
    /**
     * Will be emitted if files on the server changed
     */
    void filesChanged(Account *account);

private:
    void openWebSocket();
    void reconnectToWebSocket();
    void closeWebSocket();
    void authenticateOnWebSocket();

    Account *_account = nullptr;
    QWebSocket *_webSocket = nullptr;

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
