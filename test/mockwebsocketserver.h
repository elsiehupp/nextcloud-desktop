#ifndef MOCKWEBSOCKETSERVER_H
#define MOCKWEBSOCKETSERVER_H

#include <QWebSocketServer>
#include <QWebSocket>

class MockWebSocketServer : public QObject
{
    Q_OBJECT
public:
    struct RequestWithResponse
    {
        QString request;
        QString response;
    };

    explicit MockWebSocketServer(quint16 port = 12345);

    ~MockWebSocketServer();

    void sendTextMessage(const QString &message);

    QVector<QString> messagesReceived;

signals:
    void closed();
    void processTextMessage(QWebSocket *sender, QString message);

private slots:
    void processTextMessageInternal(QString message);
    void onNewConnection();
    void socketDisconnected();
    void setRequestsWithResponses(const QVector<RequestWithResponse> &messages);

private:
    QWebSocketServer
        *_webSocketServer;
    QList<QWebSocket *> _clients;
    QVector<RequestWithResponse> _requestsWithResponses;
};

#include "mockwebsocketserver.moc"

#endif // MOCKWEBSOCKETSERVER_H
