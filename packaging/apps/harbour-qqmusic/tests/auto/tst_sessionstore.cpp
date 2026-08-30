#include <QtTest>

#include "service/qqmusicservice.h"
#include "session/cookiepolicy.h"
#include "session/qqcookiejar.h"
#include "session/sessionstore.h"

#include <QNetworkCookie>
#include <QTimer>
#include <QVariant>

class FakeSecretsBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString stagedStorePayloadBase64
               READ stagedPayload WRITE setStagedPayload)

public:
    int loadCalls = 0;
    int storeCalls = 0;
    int deleteCalls = 0;
    QString stagedStorePayloadBase64;
    QString storedPayload;

    QString stagedPayload() const { return stagedStorePayloadBase64; }
    void setStagedPayload(const QString &payload)
    {
        stagedStorePayloadBase64 = payload;
        if (!payload.isEmpty()) {
            QTimer::singleShot(0, this, SLOT(storeSecret()));
        }
    }

public slots:
    void loadSecret() { ++loadCalls; }
    void storeSecret()
    {
        ++storeCalls;
        storedPayload = stagedStorePayloadBase64;
        stagedStorePayloadBase64.clear();
    }
    void deleteSecret() { ++deleteCalls; }

signals:
    void loadFinished(bool succeeded, int errorCode, const QString &payloadBase64);
    void storeFinished(bool succeeded, int errorCode);
    void deleteFinished(bool succeeded, int errorCode);
};

class FakeValidationService : public QqMusicService
{
    Q_OBJECT

public:
    QList<quint64> operations;
    QList<quint64> authenticatedOperations;
    QList<quint64> userOperations;
    QList<quint64> cancellations;
    QString lastUin;

    bool requestCreatedLibrary(quint64 operationId,
                               const QString &uin) override
    {
        operations.append(operationId);
        authenticatedOperations.append(operationId);
        lastUin = uin;
        return true;
    }

    bool requestUser(quint64 operationId, const QString &uin) override
    {
        operations.append(operationId);
        userOperations.append(operationId);
        lastUin = uin;
        return true;
    }

    bool cancel(quint64 operationId) override
    {
        cancellations.append(operationId);
        ServiceError error;
        error.kind = ErrorKind::Cancelled;
        error.operationId = operationId;
        emit failed(error);
        return true;
    }

    void authenticate()
    {
        emit createdLibraryReady(
            authenticatedOperations.last(),
            QVector<PlaylistSummary>(), QVector<PlaylistSummary>());
    }

    void succeedProfile(const QString &uin, const QString &nickname)
    {
        UserProfile profile;
        profile.uin = uin;
        profile.nickname = nickname;
        profile.avatarUrl = QUrl("https://thirdqq.qlogo.cn/avatar.jpg");
        emit userReady(userOperations.last(), profile);
    }

    void succeed(const QString &uin, const QString &nickname)
    {
        const int userRequestCount = userOperations.size();
        authenticate();
        if (userOperations.size() != userRequestCount) {
            succeedProfile(uin, nickname);
        }
    }

    void reject()
    {
        ServiceError error;
        error.kind = ErrorKind::AuthenticationExpired;
        error.operationId = authenticatedOperations.last();
        emit failed(error);
    }
};

namespace {

CookieRecord cookie(const QByteArray &name, const QByteArray &value)
{
    CookieRecord record;
    record.name = name;
    record.value = value;
    record.domain = QStringLiteral(".qq.com");
    record.path = QStringLiteral("/");
    record.secure = true;
    record.expiresAt = QDateTime::currentDateTimeUtc().addDays(30);
    return record;
}

QVector<CookieRecord> session(const QByteArray &uin,
                              const QByteArray &key)
{
    return QVector<CookieRecord>()
        << cookie("uin", uin)
        << cookie("qm_keyst", key);
}

QByteArray activeUin(QqCookieJar *jar)
{
    const QList<QNetworkCookie> cookies =
        jar->cookiesForUrl(QUrl("https://y.qq.com/"));
    for (int index = 0; index < cookies.size(); ++index) {
        if (cookies.at(index).name() == "uin") {
            return cookies.at(index).value();
        }
    }
    return QByteArray();
}

}

class SessionStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void replacesOnlyAfterValidationAndSecretWrite()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(bridge.metaObject()->indexOfProperty(
                    "stagedStorePayloadBase64") >= 0);
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        store.submitCandidate(session("o10001", "key-a"));
        QCOMPARE(service.lastUin, QStringLiteral("10001"));
        QCOMPARE(service.authenticatedOperations.size(), 1);
        QCOMPARE(service.userOperations.size(), 0);
        QVERIFY(active.isEmpty());
        service.authenticate();
        QCOMPARE(service.userOperations.size(), 1);
        QCOMPARE(bridge.storeCalls, 0);
        service.succeedProfile("10001", "Account A");
        QTRY_COMPARE(bridge.storeCalls, 1);
        QVERIFY(active.isEmpty());
        emit bridge.storeFinished(true, 0);
        QVERIFY(store.loggedIn());
        QCOMPARE(store.nickname(), QStringLiteral("Account A"));
        QCOMPARE(activeUin(&active), QByteArray("o10001"));

        store.submitCandidate(session("o20002", "key-b"));
        service.reject();
        QCOMPARE(candidateFailures.count(), 1);
        QCOMPARE(service.userOperations.size(), 1);
        QCOMPARE(activeUin(&active), QByteArray("o10001"));
        QCOMPARE(bridge.storeCalls, 1);

        store.submitCandidate(session("o20002", "key-b"));
        service.succeed("20002", "Account B");
        QTRY_COMPARE(bridge.storeCalls, 2);
        QCOMPARE(activeUin(&active), QByteArray("o10001"));
        emit bridge.storeFinished(true, 0);
        QCOMPARE(activeUin(&active), QByteArray("o20002"));
        QCOMPARE(store.nickname(), QStringLiteral("Account B"));
    }

    void authenticationFailureNeverRequestsProfileOrWritesSecret()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        store.submitCandidate(session("o10001", "key-a"));
        QCOMPARE(service.authenticatedOperations.size(), 1);
        QCOMPARE(service.userOperations.size(), 0);

        service.reject();

        QCOMPARE(candidateFailures.count(), 1);
        QCOMPARE(service.userOperations.size(), 0);
        QCOMPARE(bridge.storeCalls, 0);
        QVERIFY(bridge.storedPayload.isEmpty());
        QVERIFY(!store.loggedIn());
        QVERIFY(active.isEmpty());
        QVERIFY(candidate.isEmpty());
    }

    void busyRequestDoesNotCancelCandidateValidation()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy failures(&store, SIGNAL(sessionFailed(ServiceError)));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        store.submitCandidate(session("o10001", "key-a"));
        QCOMPARE(service.operations.size(), 1);
        store.submitCandidate(session("o20002", "key-b"));
        QCOMPARE(failures.count(), 1);
        QCOMPARE(candidateFailures.count(), 1);
        QCOMPARE(service.operations.size(), 1);

        service.succeed("10001", "Account A");
        QTRY_COMPARE(bridge.storeCalls, 1);
        store.logout();
        QCOMPARE(failures.count(), 2);
        QCOMPARE(candidateFailures.count(), 1);
        QCOMPARE(bridge.deleteCalls, 0);

        emit bridge.storeFinished(true, 0);
        QVERIFY(store.loggedIn());
        QCOMPARE(activeUin(&active), QByteArray("o10001"));
    }

    void interactiveCandidateSupersedesInFlightRestore()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy failures(&store, SIGNAL(sessionFailed(ServiceError)));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        const QString payload = QString::fromLatin1(
            CookiePolicy::serialize(session("o10001", "key-a")).toBase64());
        store.load();
        emit bridge.loadFinished(true, 0, payload);
        QCOMPARE(service.authenticatedOperations.size(), 1);
        const quint64 restoreOperation = service.authenticatedOperations.last();

        store.submitCandidate(session("o20002", "key-b"));

        QCOMPARE(service.cancellations,
                 QList<quint64>() << restoreOperation);
        QCOMPARE(service.authenticatedOperations.size(), 2);
        QCOMPARE(service.lastUin, QStringLiteral("20002"));
        QCOMPARE(failures.count(), 0);
        QCOMPARE(candidateFailures.count(), 0);
        QVERIFY(active.isEmpty());

        service.succeed("20002", "Account B");
        QTRY_COMPARE(bridge.storeCalls, 1);
        QVERIFY(active.isEmpty());
        emit bridge.storeFinished(true, 0);

        QVERIFY(store.loggedIn());
        QCOMPARE(store.nickname(), QStringLiteral("Account B"));
        QCOMPARE(activeUin(&active), QByteArray("o20002"));
    }

    void interactiveCandidateSupersedesPendingSecretRead()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy failures(&store, SIGNAL(sessionFailed(ServiceError)));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        store.load();
        store.submitCandidate(session("o20002", "key-b"));
        QCOMPARE(service.authenticatedOperations.size(), 1);
        QCOMPARE(service.lastUin, QStringLiteral("20002"));
        QCOMPARE(failures.count(), 0);
        QCOMPARE(candidateFailures.count(), 0);

        const QString latePayload = QString::fromLatin1(
            CookiePolicy::serialize(session("o10001", "key-a")).toBase64());
        emit bridge.loadFinished(true, 0, latePayload);
        QCOMPARE(service.authenticatedOperations.size(), 1);
        QCOMPARE(service.lastUin, QStringLiteral("20002"));

        service.succeed("20002", "Account B");
        QTRY_COMPARE(bridge.storeCalls, 1);
        emit bridge.storeFinished(true, 0);
        QVERIFY(store.loggedIn());
        QCOMPARE(activeUin(&active), QByteArray("o20002"));
    }

    void keepsStoredSecretUntilExpiredRemovalIsConfirmed()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        store.submitCandidate(session("o10001", "key-a"));
        service.succeed("10001", "Account A");
        QTRY_COMPARE(bridge.storeCalls, 1);
        emit bridge.storeFinished(true, 0);
        store.markAuthenticationExpired();
        QVERIFY(!store.loggedIn());
        QVERIFY(store.removalPending());
        QVERIFY(active.isEmpty());
        QCOMPARE(bridge.deleteCalls, 0);

        store.confirmExpiredSessionRemoval();
        QCOMPARE(bridge.deleteCalls, 1);
        QVERIFY(store.removalPending());
        emit bridge.deleteFinished(false, 7);
        QCOMPARE(candidateFailures.count(), 0);
        QVERIFY(store.removalPending());
        store.confirmExpiredSessionRemoval();
        QCOMPARE(bridge.deleteCalls, 2);
        emit bridge.deleteFinished(true, 0);
        QVERIFY(active.isEmpty());
        QVERIFY(!store.removalPending());
    }

    void clearsOnlyAfterSuccessfulLogoutDeletion()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));
        store.submitCandidate(session("o10001", "key-a"));
        service.succeed("10001", "Account A");
        QTRY_COMPARE(bridge.storeCalls, 1);
        emit bridge.storeFinished(true, 0);

        store.logout();
        QCOMPARE(bridge.deleteCalls, 1);
        QVERIFY(store.loggedIn());
        emit bridge.deleteFinished(false, 7);
        QCOMPARE(candidateFailures.count(), 0);
        QVERIFY(store.loggedIn());
        QCOMPARE(activeUin(&active), QByteArray("o10001"));

        store.logout();
        emit bridge.deleteFinished(true, 0);
        QVERIFY(!store.loggedIn());
        QVERIFY(active.isEmpty());
        QVERIFY(candidate.isEmpty());
    }

    void rejectsLoadFailureCorruptionAndExpiredSecrets()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy failures(&store, SIGNAL(sessionFailed(ServiceError)));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        store.load();
        emit bridge.loadFinished(false, 1, QString());
        QCOMPARE(failures.count(), 1);
        QVERIFY(active.isEmpty());

        store.load();
        emit bridge.loadFinished(true, 0, QStringLiteral("not-base64"));
        QCOMPARE(failures.count(), 2);

        QVector<CookieRecord> expired = session("o10001", "key-a");
        expired[0].expiresAt = QDateTime::currentDateTimeUtc().addSecs(-1);
        const QString payload = QString::fromLatin1(
            CookiePolicy::serialize(expired).toBase64());
        store.load();
        emit bridge.loadFinished(true, 0, payload);
        QCOMPARE(failures.count(), 3);
        QCOMPARE(candidateFailures.count(), 0);
        QCOMPARE(service.operations.size(), 0);
        QVERIFY(store.removalPending());
        store.confirmExpiredSessionRemoval();
        QCOMPARE(bridge.deleteCalls, 1);
        emit bridge.deleteFinished(true, 0);
        QVERIFY(!store.removalPending());
    }

    void acceptsMissingStoredSecretAsSignedOut()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy failures(&store, SIGNAL(sessionFailed(ServiceError)));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        store.load();
        QCOMPARE(bridge.loadCalls, 1);
        emit bridge.loadFinished(true, 43, QString());

        QCOMPARE(failures.count(), 0);
        QCOMPARE(candidateFailures.count(), 0);
        QCOMPARE(service.operations.size(), 0);
        QVERIFY(!store.loggedIn());
        QVERIFY(active.isEmpty());
        QVERIFY(candidate.isEmpty());

        store.submitCandidate(session("o10001", "key-a"));
        QCOMPARE(service.authenticatedOperations.size(), 1);
        QCOMPARE(service.lastUin, QStringLiteral("10001"));
    }

    void restoresRemovalPendingWhenLoadedSessionValidationExpires()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        const QString payload = QString::fromLatin1(
            CookiePolicy::serialize(session("o10001", "key-a")).toBase64());
        store.load();
        emit bridge.loadFinished(true, 0, payload);
        QCOMPARE(service.operations.size(), 1);
        QVERIFY(!store.removalPending());
        service.reject();
        QCOMPARE(candidateFailures.count(), 0);
        QVERIFY(store.removalPending());
        QVERIFY(!store.loggedIn());
    }

    void authenticationExpiryCancelsLoadAndIgnoresLateSuccess()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy activated(&store, SIGNAL(sessionActivated(UserProfile)));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));
        QSignalSpy failures(&store, SIGNAL(sessionFailed(ServiceError)));

        const QString payload = QString::fromLatin1(
            CookiePolicy::serialize(session("o10001", "key-a")).toBase64());
        store.load();
        emit bridge.loadFinished(true, 0, payload);
        QCOMPARE(service.operations.size(), 1);
        const quint64 validationId = service.operations.last();

        store.markAuthenticationExpired();
        QCOMPARE(service.cancellations, QList<quint64>() << validationId);
        QVERIFY(store.removalPending());
        QVERIFY(candidate.isEmpty());
        QCOMPARE(candidateFailures.count(), 0);
        QCOMPARE(failures.count(), 0);

        service.succeed("10001", "Late Account");
        QCOMPARE(activated.count(), 0);
        QVERIFY(!store.loggedIn());
        QVERIFY(active.isEmpty());
        QVERIFY(store.removalPending());
    }

    void authenticationExpiryCancelsInteractiveCandidateOnce()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));
        QSignalSpy failures(&store, SIGNAL(sessionFailed(ServiceError)));

        store.submitCandidate(session("o10001", "key-a"));
        QCOMPARE(service.operations.size(), 1);
        const quint64 validationId = service.operations.last();
        store.markAuthenticationExpired();

        QCOMPARE(service.cancellations, QList<quint64>() << validationId);
        QCOMPARE(candidateFailures.count(), 1);
        QCOMPARE(failures.count(), 0);
        QVERIFY(candidate.isEmpty());
        QVERIFY(store.removalPending());

        service.succeed("10001", "Late Candidate");
        QCOMPARE(candidateFailures.count(), 1);
        QVERIFY(!store.loggedIn());
        QVERIFY(active.isEmpty());
    }

    void reportsInvalidAndStoreRejectedCandidatesOnly()
    {
        QqCookieJar active;
        QqCookieJar candidate;
        FakeValidationService service;
        SessionStore store(&active, &service, &candidate);
        FakeSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy failures(&store, SIGNAL(sessionFailed(ServiceError)));
        QSignalSpy candidateFailures(&store, SIGNAL(candidateFailed()));

        store.submitCandidate(QVector<CookieRecord>());
        QCOMPARE(failures.count(), 1);
        QCOMPARE(candidateFailures.count(), 1);
        QCOMPARE(service.operations.size(), 0);

        store.submitCandidate(session("o10001", "key-a"));
        service.succeed("10001", "Account A");
        QTRY_COMPARE(bridge.storeCalls, 1);
        emit bridge.storeFinished(false, 7);
        QCOMPARE(failures.count(), 2);
        QCOMPARE(candidateFailures.count(), 2);
        QVERIFY(!store.loggedIn());
        QVERIFY(active.isEmpty());
    }
};

QTEST_GUILESS_MAIN(SessionStoreTest)
#include "tst_sessionstore.moc"
