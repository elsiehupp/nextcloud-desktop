#include <QTest>
#include <QVector>

#include "account.h"
#include "common.h"
#include "pushnotifications.h"
#include "creds/abstractcredentials.h"

using namespace std::chrono_literals;

class WebSocketStub : public OCC::AbstractWebSocket
{
    Q_OBJECT
public:
    void open(const QUrl & /*url*/) override
    {
        emit connected();
    }
    void close() override { }
    qint64 sendTextMessage(const QString &message) override
    {
        ++sendTextMessageCalledCount;
        sendTextMessageCalledArguments.append(message);
        return 0;
    }

    uint32_t sendTextMessageCalledCount = 0;
    QVector<QString> sendTextMessageCalledArguments;
};

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

QSharedPointer<WebSocketStub> createWebSocket()
{
    return QSharedPointer<WebSocketStub>(new WebSocketStub);
}

QSharedPointer<OCC::Account> createAccount()
{
    auto account = OCC::Account::create();

    QStringList typeList;
    typeList.append("files");

    QString websocketUrl("testurl");

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

QSharedPointer<OCC::PushNotifications> createPushNotifications(QSharedPointer<WebSocketStub> webSocket, OCC::Account *account)
{
    return QSharedPointer<OCC::PushNotifications>(new OCC::PushNotifications(account, webSocket));
}

class TestPushNotifications : public QObject
{
    Q_OBJECT

private slots:
    void testReconnect_correctCredentials_authenticatedOnWebSocket()
    {
        const QString user = "user";
        const QString password = "password";
        auto webSocket = createWebSocket();
        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(webSocket, account.data());
        pushNotifications->reconnect();

        wait(4ms);

        QCOMPARE(webSocket->sendTextMessageCalledCount, 2);
        QCOMPARE(webSocket->sendTextMessageCalledArguments[0], user);
        QCOMPARE(webSocket->sendTextMessageCalledArguments[1], password);
    }

    void testOnWebSocketTextMessageReceived_notifyFileMessage_emitFilesChanged()
    {
        bool wasFilesChangedEmitted = false;
        const QString user = "user";
        const QString password = "password";
        auto webSocket = createWebSocket();
        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(webSocket, account.data());
        pushNotifications->reconnect();

        connect(pushNotifications.data(), &OCC::PushNotifications::filesChanged, [&](OCC::Account * /*account*/) {
            wasFilesChangedEmitted = true;
        });

        emit webSocket->textMessageReceived("notify_file");
        wait(1s);

        QCOMPARE(wasFilesChangedEmitted, true);
    }

    void testOnWebSocketTextMessageReceived_invalidCredentialsMessage_reconnectWebSocket()
    {
        const QString user = "user";
        const QString password = "password";
        auto webSocket = createWebSocket();
        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(webSocket, account.data());
        pushNotifications->reconnect();

        webSocket->sendTextMessageCalledCount = 0;
        webSocket->sendTextMessageCalledArguments.clear();

        emit webSocket->textMessageReceived("err: Invalid credentials");
        wait(1s);

        QCOMPARE(webSocket->sendTextMessageCalledCount, 2);
        QCOMPARE(webSocket->sendTextMessageCalledArguments[0], user);
        QCOMPARE(webSocket->sendTextMessageCalledArguments[1], password);
    }

    void testOnWebSocketError_connectionLost_reconnectWebSocket()
    {
        const QString user = "user";
        const QString password = "password";
        auto webSocket = createWebSocket();
        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(webSocket, account.data());
        pushNotifications->reconnect();

        webSocket->sendTextMessageCalledCount = 0;
        webSocket->sendTextMessageCalledArguments.clear();

        emit webSocket->error(QAbstractSocket::SocketError::NetworkError);
        wait(1s);

        QCOMPARE(webSocket->sendTextMessageCalledCount, 2);
        QCOMPARE(webSocket->sendTextMessageCalledArguments[0], user);
        QCOMPARE(webSocket->sendTextMessageCalledArguments[1], password);
    }
};

QTEST_GUILESS_MAIN(TestPushNotifications)
#include "testpushnotifications.moc"
