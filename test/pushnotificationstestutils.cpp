#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTest>

#include "pushnotificationstestutils.h"

Q_LOGGING_CATEGORY(lcMockWebSocketServer, "nextcloud.test.mockwebserver", QtInfoMsg)

FakeWebSocketServer::FakeWebSocketServer(quint16 port, QObject *parent)
    : QObject(parent)
    , _webSocketServer(new QWebSocketServer(QStringLiteral("Mock Server"), QWebSocketServer::NonSecureMode, this))
{
    if (_webSocketServer->listen(QHostAddress::Any, port)) {
        connect(_webSocketServer, &QWebSocketServer::newConnection, this, &FakeWebSocketServer::onNewConnection);
        connect(_webSocketServer, &QWebSocketServer::closed, this, &FakeWebSocketServer::closed);
        qCInfo(lcMockWebSocketServer) << "Open mock websocket server on port:" << port;
        return;
    }
    Q_UNREACHABLE();
}

FakeWebSocketServer::~FakeWebSocketServer()
{
    close();
}

void FakeWebSocketServer::waitForMessage(std::function<void(QWebSocket *, const QString &)> processMessage, std::function<void()> triggerMessage)
{
    QSignalSpy spy(this, &FakeWebSocketServer::processTextMessage);
    triggerMessage();
    QVERIFY(spy.wait());
    auto socket = spy.at(0).at(0).value<QWebSocket *>();
    const auto message = spy.at(0).at(1).toString();
    processMessage(socket, message);
}

void FakeWebSocketServer::close()
{
    if (_webSocketServer->isListening()) {
        qCInfo(lcMockWebSocketServer) << "Close mock websocket server";

        _webSocketServer->close();
        qDeleteAll(_clients.begin(), _clients.end());
    }
}

void FakeWebSocketServer::processTextMessageInternal(const QString &message)
{
    auto client = qobject_cast<QWebSocket *>(sender());
    emit processTextMessage(client, message);
}

void FakeWebSocketServer::onNewConnection()
{
    qCInfo(lcMockWebSocketServer) << "New connection on mock websocket server";

    auto socket = _webSocketServer->nextPendingConnection();

    connect(socket, &QWebSocket::textMessageReceived, this, &FakeWebSocketServer::processTextMessageInternal);
    connect(socket, &QWebSocket::disconnected, this, &FakeWebSocketServer::socketDisconnected);

    _clients << socket;
}

void FakeWebSocketServer::socketDisconnected()
{
    qCInfo(lcMockWebSocketServer) << "Socket disconnected";

    auto client = qobject_cast<QWebSocket *>(sender());

    if (client) {
        _clients.removeAll(client);
        client->deleteLater();
    }
}

CredentialsStub::CredentialsStub(const QString &user, const QString &password)
    : _user(user)
    , _password(password)
{
}

QString CredentialsStub::authType() const
{
    return "";
}

QString CredentialsStub::user() const
{
    return _user;
}

QString CredentialsStub::password() const
{
    return _password;
}

QNetworkAccessManager *CredentialsStub::createQNAM() const
{
    return nullptr;
}

bool CredentialsStub::ready() const
{
    return false;
}

void CredentialsStub::fetchFromKeychain() { }

void CredentialsStub::askFromUser() { }

bool CredentialsStub::stillValid(QNetworkReply * /*reply*/)
{
    return false;
}

void CredentialsStub::persist() { }

void CredentialsStub::invalidateToken() { }

void CredentialsStub::forgetSensitiveData() { }

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

void setCredentials(OCC::Account *account, const QString &user, const QString &password)
{
    auto credentials = new CredentialsStub(user, password);
    account->setCredentials(credentials);
}
