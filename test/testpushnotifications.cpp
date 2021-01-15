#include <QTest>
#include <QVector>
#include <QWebSocketServer>
#include <QSignalSpy>

#include "account.h"
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

OCC::AccountPtr createAccount()
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

std::unique_ptr<OCC::PushNotifications> createPushNotifications(OCC::Account *account)
{
    return std::make_unique<OCC::PushNotifications>(account);
}

class TestPushNotifications : public QObject
{
    Q_OBJECT

private slots:
    void testSetup_correctCredentials_authenticatedOnWebSocket()
    {
        const QString user = "user";
        const QString password = "password";
        MockWebSocketServer mockServer;
        QSignalSpy spy(&mockServer, &MockWebSocketServer::processTextMessage);
        QVERIFY(spy.isValid());
        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());

        // Setup push notifications
        pushNotifications->setup();

        // Wait for authentication
        spy.wait();

        // Right authentication data should be sent
        QCOMPARE(spy.count(), 2);

        const auto userSent = spy.at(0).at(1).toString();
        const auto passwordSent = spy.at(1).at(1).toString();

        QCOMPARE(userSent, user);
        QCOMPARE(passwordSent, password);
    }

    void testOnWebSocketTextMessageReceived_notifyFileMessage_emitFilesChanged()
    {
        const QString user = "user";
        const QString password = "password";
        MockWebSocketServer mockServer;
        QSignalSpy processTextMessageSpy(&mockServer, &MockWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount().data();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account);
        QSignalSpy filesChangedSpy(pushNotifications.get(), &OCC::PushNotifications::filesChanged);
        QVERIFY(filesChangedSpy.isValid());

        // Setup push notifications
        pushNotifications->setup();

        // Wait for authentication and then send notify_file push notification
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("notify_file");

        // filesChanged signal should be emitted
        QVERIFY(filesChangedSpy.wait());
        QCOMPARE(filesChangedSpy.count(), 1);
        // FIXME: This fails because accountFilesChanged is always nullptr. What do I wrong?
        // I have checked that in a normal signal handler I get the correct account pointer.
        // auto accountFilesChanged = filesChangedSpy.at(0).at(0).value<OCC::Account *>();
        // QCOMPARE(accountFilesChanged, account);
    }

    void testOnWebSocketTextMessageReceived_invalidCredentialsMessage_reconnectWebSocket()
    {
        const QString user = "user";
        const QString password = "password";
        MockWebSocketServer mockServer;
        QSignalSpy spy(&mockServer, &MockWebSocketServer::processTextMessage);
        QVERIFY(spy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());

        // Setup push notifications
        pushNotifications->setup();

        // Wait for authentication attempt and then sent invalid credentials
        spy.wait();
        QCOMPARE(spy.count(), 2);
        const auto socket = spy.at(0).at(0).value<QWebSocket *>();
        const auto firstPasswordSent = spy.at(1).at(1).toString();
        QCOMPARE(firstPasswordSent, password);
        socket->sendTextMessage("err: Invalid credentials");

        // Wait for a new authentication attempt
        spy.wait();
        QCOMPARE(spy.count(), 4);
        const auto secondPasswordSent = spy.at(3).at(1).toString();
        QCOMPARE(secondPasswordSent, password);
    }

    void testOnWebSocketError_connectionLost_reconnectWebSocket()
    {
        const QString user = "user";
        const QString password = "password";
        MockWebSocketServer mockServer;
        QSignalSpy processTextMessageSpy(&mockServer, &MockWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());
        QSignalSpy connectionLostSpy(pushNotifications.get(), &OCC::PushNotifications::connectionLost);
        QVERIFY(connectionLostSpy.isValid());

        // Setup push notifications
        pushNotifications->setup();

        // Wait for authentication and then sent a network error
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        // FIXME: This a little bit ugly but I had no better idea how to trigger a error on the websocket client.
        // The websocket that is retrived through the server is not connected to the error signal.
        emit pushNotifications->_webSocket->error(QAbstractSocket::SocketError::NetworkError);

        // Wait for connectionLost signal
        connectionLostSpy.wait();
        QCOMPARE(connectionLostSpy.count(), 1);
    }

    void testSetup_credentialsThreeTimeRejected_emitCanNotAuthenticate()
    {
        const QString user = "user";
        const QString password = "password";
        MockWebSocketServer mockServer;
        QSignalSpy processTextMessageSpy(&mockServer, &MockWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());
        QSignalSpy canNotAuthenticateSpy(pushNotifications.get(), &OCC::PushNotifications::canNotAuthenticate);
        QVERIFY(canNotAuthenticateSpy.isValid());

        // Setup push notifications
        pushNotifications->setup();

        // Let three authentication attempts fail
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 4);
        socket = processTextMessageSpy.at(2).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 6);
        socket = processTextMessageSpy.at(4).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        // Now the canNotAuthenticate Signal should be emitted
        canNotAuthenticateSpy.wait();
        QCOMPARE(canNotAuthenticateSpy.count(), 1);
    }

    void testSetup_maxConnectionAttemptsReached_canConnectAgain()
    {
        const QString user = "user";
        const QString password = "password";
        MockWebSocketServer mockServer;
        QSignalSpy processTextMessageSpy(&mockServer, &MockWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());
        QSignalSpy canNotAuthenticateSpy(pushNotifications.get(), &OCC::PushNotifications::canNotAuthenticate);
        QVERIFY(canNotAuthenticateSpy.isValid());

        // Setup push notifications
        pushNotifications->setup();

        // Let three authentication attempts fail
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 4);
        socket = processTextMessageSpy.at(2).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 6);
        socket = processTextMessageSpy.at(4).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        // Now the canNotAuthenticate Signal should be emitted
        canNotAuthenticateSpy.wait();
        QCOMPARE(canNotAuthenticateSpy.count(), 1);

        // Now call setup again and it should do another connection attempt
        pushNotifications->setup();
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 8);

        auto userSent = processTextMessageSpy.at(6).at(1).toString();
        auto passwordSent = processTextMessageSpy.at(7).at(1).toString();
        QCOMPARE(userSent, user);
        QCOMPARE(passwordSent, password);
    }

    void testAuthentication_correctCredentials_emitReady()
    {
        const QString user = "user";
        const QString password = "password";
        MockWebSocketServer mockServer;
        QSignalSpy processTextMessageSpy(&mockServer, &MockWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());
        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        auto pushNotifications = createPushNotifications(account.data());
        QSignalSpy readySpy(pushNotifications.get(), &OCC::PushNotifications::ready);
        QVERIFY(readySpy.isValid());

        // Setup push notifications
        pushNotifications->setup();

        // Wait for authentication and send authenticated response
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("authenticated");

        // Wait for ready signal
        readySpy.wait();
        QCOMPARE(readySpy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestPushNotifications)
#include "testpushnotifications.moc"
