#include <QTest>
#include <QVector>
#include <QWebSocketServer>

#include "account.h"
#include "common.h"
#include "pushnotifications.h"
#include "mockwebsocketserver.h"
#include "creds/abstractcredentials.h"

using namespace std::chrono_literals;

class CredentialsStub : public OCC::AbstractCredentials
{
    Q_OBJECT

public:
    CredentialsStub(const QString &user, const QString &password)
        : _user(user)
        , _password(password)
    {
    }
    virtual QString authType() const { return ""; }
    virtual QString user() const { return _user; }
    virtual QString password() const
    {
        return _password;
    }
    virtual QNetworkAccessManager *createQNAM() const { return nullptr; }
    virtual bool ready() const { return false; }
    virtual void fetchFromKeychain() { }
    virtual void askFromUser() { }

    virtual bool stillValid(QNetworkReply * /*reply*/) { return false; }
    virtual void persist() { }
    virtual void invalidateToken() { }
    virtual void forgetSensitiveData() { }

private:
    QString _user;
    QString _password;
};

QSharedPointer<OCC::Account> createAccount()
{
    auto account = OCC::Account::create();

    QStringList typeList;
    typeList.append("files");

    QString websocketUrl("ws://localhost:12345");

    QVariantMap endpointsMap;
    endpointsMap["websocket"] = websocketUrl;

    QVariantMap notifyPushMap;
    notifyPushMap["type"] = typeList;
    notifyPushMap["endpoints"] = endpointsMap;

    QVariantMap capabilitiesMap;
    capabilitiesMap["notify_push"] = notifyPushMap;

    account->setCapabilities(capabilitiesMap);

    return account;
}

QSharedPointer<OCC::PushNotifications> createPushNotifications(OCC::Account *account)
{
    return QSharedPointer<OCC::PushNotifications>(new OCC::PushNotifications(account));
}

class TestPushNotifications : public QObject
{
    Q_OBJECT

private slots:
    void testReconnect_correctCredentials_authenticatedOnWebSocket()
    {
        const QString user = "user";
        const QString password = "password";

        QEventLoop loop;
        QTimer::singleShot(5000, [&]() {
            loop.quit();
            QFAIL("No messages arrived");
        });

        bool userSended = false;
        MockWebSocketServer mockServer;
        connect(&mockServer, &MockWebSocketServer::processTextMessage, [&](QWebSocket *socket, QString message) {
            if (message == user) {
                userSended = true;
                return;
            } else if (message == password) {
                QCOMPARE(userSended, true);
                loop.quit();
                return;
            }
            QFAIL("Unexpected message");
        });


        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());
        pushNotifications->setup();

        loop.exec();
    }

    void testOnWebSocketTextMessageReceived_notifyFileMessage_emitFilesChanged()
    {
        const QString user = "user";
        const QString password = "password";
        MockWebSocketServer mockServer;

        connect(&mockServer, &MockWebSocketServer::processTextMessage, [&](QWebSocket *sender, QString message) {
            if (message == password) {
                sender->sendTextMessage("notify_file");
                return;
            }
        });

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());

        QEventLoop loop;
        QTimer::singleShot(5000, [&]() {
            loop.quit();
            QFAIL("No files changed event emitted");
        });

        connect(pushNotifications.data(), &OCC::PushNotifications::filesChanged, [&](OCC::Account * /*account*/) {
            loop.quit();
        });

        pushNotifications->setup();

        loop.exec();
    }

    void testOnWebSocketTextMessageReceived_invalidCredentialsMessage_reconnectWebSocket()
    {
        const QString user = "user";
        const QString password = "password";

        QEventLoop loop;
        QTimer::singleShot(5000, [&]() {
            loop.quit();
            QFAIL("No retry");
        });

        bool userSended = false;
        MockWebSocketServer mockServer;
        quint16 passwordSendedCount = 0;
        connect(&mockServer, &MockWebSocketServer::processTextMessage, [&](QWebSocket *socket, QString message) {
            if (message == password) {
                if (passwordSendedCount == 0) {
                    socket->sendTextMessage("err: Invalid credentials");
                }
                ++passwordSendedCount;
                if (passwordSendedCount == 2) {
                    loop.quit();
                }
            }
        });


        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());
        pushNotifications->setup();

        loop.exec();
    }

    void testOnWebSocketError_connectionLost_reconnectWebSocket()
    {
        const QString user = "user";
        const QString password = "password";

        QEventLoop loop;
        QTimer::singleShot(5000, [&]() {
            loop.quit();
            QFAIL("No reconnect");
        });

        bool userSended = false;
        MockWebSocketServer mockServer;
        quint16 passwordSendedCount = 0;


        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());
        connect(&mockServer, &MockWebSocketServer::processTextMessage, [&](QWebSocket *socket, QString message) {
            if (message == password) {
                if (passwordSendedCount == 0) {
                    // This a little bit ugly but I had no better idea how to trigger a error on the websocket client.
                    // The websocket that is retrived through the server is not connected to the error signal.
                    emit pushNotifications->_webSocket->error(QAbstractSocket::SocketError::NetworkError);
                }
                ++passwordSendedCount;
                if (passwordSendedCount == 2) {
                    loop.quit();
                }
            }
        });
        pushNotifications->setup();

        loop.exec();
    }
};

QTEST_GUILESS_MAIN(TestPushNotifications)
#include "testpushnotifications.moc"
