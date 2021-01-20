#include <QTest>
#include <QVector>
#include <QWebSocketServer>
#include <QSignalSpy>

#include "pushnotifications.h"
#include "pushnotificationstestutils.h"

class TestPushNotifications : public QObject
{
    Q_OBJECT

private slots:
    void testSetup_correctCredentials_authenticateAndEmitReady()
    {
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());
        const QString user = "user";
        const QString password = "password";
        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        QSignalSpy readySpy(account->pushNotifications(), &OCC::PushNotifications::ready);
        QVERIFY(readySpy.isValid());

        // Wait for authentication
        QVERIFY(processTextMessageSpy.wait());

        // Right authentication data should be sent
        QCOMPARE(processTextMessageSpy.count(), 2);

        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        const auto userSent = processTextMessageSpy.at(0).at(1).toString();
        const auto passwordSent = processTextMessageSpy.at(1).at(1).toString();

        QCOMPARE(userSent, user);
        QCOMPARE(passwordSent, password);

        // Sent authenticated
        socket->sendTextMessage("authenticated");

        // Wait for ready signal
        readySpy.wait();
        QCOMPARE(readySpy.count(), 1);
        QCOMPARE(account->pushNotifications()->isReady(), true);
    }

    void testOnWebSocketTextMessageReceived_notifyFileMessage_emitFilesChanged()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        QSignalSpy filesChangedSpy(account->pushNotifications(), &OCC::PushNotifications::filesChanged);
        QVERIFY(filesChangedSpy.isValid());

        // Wait for authentication and then send notify_file push notification
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("notify_file");

        // filesChanged signal should be emitted
        QVERIFY(filesChangedSpy.wait());
        QCOMPARE(filesChangedSpy.count(), 1);
        auto accountFilesChanged = filesChangedSpy.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountFilesChanged, account.data());
    }

    void testOnWebSocketTextMessageReceived_notifyActivityMessage_emitNotification()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        QSignalSpy filesChangedSpy(account->pushNotifications(), &OCC::PushNotifications::notification);
        QVERIFY(filesChangedSpy.isValid());

        // Wait for authentication and then send notify_file push notification
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("notify_activity");

        // notification signal should be emitted
        QVERIFY(filesChangedSpy.wait());
        QCOMPARE(filesChangedSpy.count(), 1);
        auto accountFilesChanged = filesChangedSpy.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountFilesChanged, account.data());
    }

    void testOnWebSocketTextMessageReceived_notifyNotificationMessage_emitNotification()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        QSignalSpy filesChangedSpy(account->pushNotifications(), &OCC::PushNotifications::notification);
        QVERIFY(filesChangedSpy.isValid());

        // Wait for authentication and then send notify_file push notification
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("notify_notification");

        // notification signal should be emitted
        QVERIFY(filesChangedSpy.wait());
        QCOMPARE(filesChangedSpy.count(), 1);
        auto accountFilesChanged = filesChangedSpy.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountFilesChanged, account.data());
    }

    void testOnWebSocketTextMessageReceived_invalidCredentialsMessage_reconnectWebSocket()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        // Need to set reconnect timer interval to zero for tests
        account->pushNotifications()->setReconnectTimerInterval(0);

        // Wait for authentication attempt and then sent invalid credentials
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        const auto firstPasswordSent = processTextMessageSpy.at(1).at(1).toString();
        QCOMPARE(firstPasswordSent, password);
        processTextMessageSpy.clear();
        socket->sendTextMessage("err: Invalid credentials");

        // Wait for a new authentication attempt
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto secondPasswordSent = processTextMessageSpy.at(1).at(1).toString();
        QCOMPARE(secondPasswordSent, password);
    }

    void testOnWebSocketError_connectionLost_emitConnectionLost()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        // Need to set reconnect timer interval to zero for tests
        account->pushNotifications()->setReconnectTimerInterval(0);

        QSignalSpy connectionLostSpy(account->pushNotifications(), &OCC::PushNotifications::connectionLost);
        QVERIFY(connectionLostSpy.isValid());

        // Wait for authentication and then sent a network error
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->abort();

        // Wait for connectionLost signal
        connectionLostSpy.wait();
        QCOMPARE(connectionLostSpy.count(), 1);
        QCOMPARE(account->pushNotifications()->isReady(), false);
    }

    void testSetup_credentialsThreeTimeRejected_emitAuthenticationFailed()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());
        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        account->pushNotifications()->setReconnectTimerInterval(0);

        QSignalSpy authenticationFailedSpy(account->pushNotifications(), &OCC::PushNotifications::authenticationFailed);
        QVERIFY(authenticationFailedSpy.isValid());

        // Let three authentication attempts fail
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();

        processTextMessageSpy.clear();
        socket->sendTextMessage("err: Invalid credentials");

        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        processTextMessageSpy.clear();
        socket->sendTextMessage("err: Invalid credentials");

        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        // Now the canNotAuthenticate Signal should be emitted
        authenticationFailedSpy.wait();
        QCOMPARE(authenticationFailedSpy.count(), 1);
        QCOMPARE(account->pushNotifications()->isReady(), false);
    }

    void testSetup_maxConnectionAttemptsReached_canConnectAgain()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        account->pushNotifications()->setReconnectTimerInterval(0);
        QSignalSpy canNotAuthenticateSpy(account->pushNotifications(), &OCC::PushNotifications::authenticationFailed);
        QVERIFY(canNotAuthenticateSpy.isValid());

        // Let three authentication attempts fail
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        processTextMessageSpy.clear();
        socket->sendTextMessage("err: Invalid credentials");

        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        processTextMessageSpy.clear();
        socket->sendTextMessage("err: Invalid credentials");

        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        processTextMessageSpy.clear();
        socket->sendTextMessage("err: Invalid credentials");

        // Now the canNotAuthenticate Signal should be emitted
        canNotAuthenticateSpy.wait();
        QCOMPARE(canNotAuthenticateSpy.count(), 1);

        // Now call setup again and it should do another connection attempt
        account->pushNotifications()->setup();
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);

        auto userSent = processTextMessageSpy.at(0).at(1).toString();
        auto passwordSent = processTextMessageSpy.at(1).at(1).toString();
        QCOMPARE(userSent, user);
        QCOMPARE(passwordSent, password);
    }

    void testOnWebSocketSslError_sslError_emitConnectionLost()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);

        QSignalSpy connectionLostSpy(account->pushNotifications(), &OCC::PushNotifications::connectionLost);
        QVERIFY(connectionLostSpy.isValid());

        processTextMessageSpy.wait();

        // FIXME: This a little bit ugly but I had no better idea how to trigger a error on the websocket client.
        // The websocket that is retrived through the server is not connected to the error signal.
        emit account->pushNotifications()->_webSocket->sslErrors(QList<QSslError>());

        // Wait for connectionLost signal
        // FIXME: For some reason it takes a few seconds until this signal arrives. Why does this happen?
        connectionLostSpy.wait();
        QCOMPARE(connectionLostSpy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestPushNotifications)
#include "testpushnotifications.moc"
