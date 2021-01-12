#include <QTest>
#include <QVector>

#include "account.h"
#include "common.h"
#include "creds/abstractcredentials.h"

using namespace std::chrono_literals;

class WebSocketStub : public OCC::AbstractWebSocket
{
    Q_OBJECT
public:
    void open(const QUrl &url) override
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

    virtual bool stillValid(QNetworkReply *reply) { return false; }
    virtual void persist() { }
    virtual void invalidateToken() { }
    virtual void forgetSensitiveData() { }

private:
    QString _user;
    QString _password;
};

QSharedPointer<OCC::Account> createAccount(WebSocketStub *webSocket)
{
    QSharedPointer<WebSocketStub> ws(webSocket);
    auto account = OCC::Account::create(ws);

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

class TestAccount : public QObject
{
    Q_OBJECT

private slots:
    void testSetCredentials_correctCredentials_AuthenticatedOnWebSocket()
    {
        const QString user = "user";
        const QString password = "password";
        auto credentials = new CredentialsStub(user, password);
        auto webSocket = new WebSocketStub();
        auto account = createAccount(webSocket);

        account->setCredentials(credentials);
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
        auto credentials = new CredentialsStub(user, password);
        auto webSocket = new WebSocketStub();
        auto account = createAccount(webSocket);
        account->setCredentials(credentials);

        connect(account.data(), &OCC::Account::filesChanged, [&](OCC::Account *account) {
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
        auto credentials = new CredentialsStub(user, password);
        auto webSocket = new WebSocketStub();
        auto account = createAccount(webSocket);
        account->setCredentials(credentials);

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
        auto credentials = new CredentialsStub(user, password);
        auto webSocket = new WebSocketStub();
        auto account = createAccount(webSocket);
        account->setCredentials(credentials);

        webSocket->sendTextMessageCalledCount = 0;
        webSocket->sendTextMessageCalledArguments.clear();

        emit webSocket->error(QAbstractSocket::SocketError::NetworkError);
        wait(1s);

        QCOMPARE(webSocket->sendTextMessageCalledCount, 2);
        QCOMPARE(webSocket->sendTextMessageCalledArguments[0], user);
        QCOMPARE(webSocket->sendTextMessageCalledArguments[1], password);
    }
};

QTEST_GUILESS_MAIN(TestAccount)
#include "testaccount.moc"
