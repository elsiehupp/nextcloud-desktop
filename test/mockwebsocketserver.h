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

#ifndef MOCKWEBSOCKETSERVER_H
#define MOCKWEBSOCKETSERVER_H

#include <QWebSocketServer>
#include <QWebSocket>

class MockWebSocketServer : public QObject
{
    Q_OBJECT
public:
    explicit MockWebSocketServer(quint16 port = 12345, QObject *parent = nullptr);

    ~MockWebSocketServer();

signals:
    void closed();
    void processTextMessage(QWebSocket *sender, const QString &message);

private slots:
    void processTextMessageInternal(const QString &message);
    void onNewConnection();
    void socketDisconnected();

private:
    QWebSocketServer *_webSocketServer;
    QList<QWebSocket *> _clients;
};

#endif // MOCKWEBSOCKETSERVER_H
