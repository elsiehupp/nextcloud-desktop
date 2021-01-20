#include <QTest>
#include <QSignalSpy>

#include "pushnotificationstestutils.h"

class TestAccount : public QObject
{
    Q_OBJECT

private slots:
    void testSetCredentials_correctCredentials_emitPushNotificationsReady()
    {
        FakeWebSocketServer fakeServer;
        auto account = createAccount();

        QSignalSpy pushNotificationsReady(account.data(), &OCC::Account::pushNotificationsReady);
        QVERIFY(pushNotificationsReady.isValid());

        fakeServer.waitForMessage(
            [](QWebSocket *socket, const QString &) {
                // Don't care about witch message was sent
                socket->sendTextMessage("authenticated");
            },
            [account]() {
                const QString user = "user";
                const QString password = "password";
                setCredentials(account.data(), user, password);
            });

        QVERIFY(pushNotificationsReady.wait());
        auto accountSent = pushNotificationsReady.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountSent, account.data());
    }

private:
};

QTEST_GUILESS_MAIN(TestAccount)
#include "testaccount.moc"
