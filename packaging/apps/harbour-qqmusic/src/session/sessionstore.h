#ifndef SESSIONSTORE_H
#define SESSIONSTORE_H

#include "cookiepolicy.h"
#include "core/types.h"

#include <QObject>
#include <QPointer>

class QNetworkAccessManager;
class QqCookieJar;
class QqMusicService;

class SessionStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool removalPending READ removalPending
               NOTIFY removalPendingChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY sessionChanged)
    Q_PROPERTY(QString nickname READ nickname NOTIFY sessionChanged)
    Q_PROPERTY(QUrl avatar READ avatar NOTIFY sessionChanged)

public:
    explicit SessionStore(QqCookieJar *activeJar, QObject *parent = 0);
    SessionStore(QqCookieJar *activeJar,
                 QqMusicService *validationService,
                 QqCookieJar *candidateJar,
                 QObject *parent = 0);

    bool bindBridge(QObject *bridge);
    bool loggedIn() const;
    QString nickname() const;
    QUrl avatar() const;
    UserProfile profile() const;
    bool removalPending() const;

    Q_INVOKABLE void load();
    void submitCandidate(const QVector<CookieRecord> &records);
    Q_INVOKABLE void logout();
    void markAuthenticationExpired();
    Q_INVOKABLE void confirmExpiredSessionRemoval();

signals:
    void sessionChanged();
    void sessionActivated(const UserProfile &profile);
    void sessionCleared();
    void sessionFailed(const ServiceError &error);
    void candidateFailed();
    void sessionActionFailed();
    void removalPendingChanged();

private slots:
    void bridgeLoadFinished(bool succeeded,
                            int errorCode,
                            const QString &payloadBase64);
    void bridgeStoreFinished(bool succeeded, int errorCode);
    void bridgeDeleteFinished(bool succeeded, int errorCode);
    void validationLibraryReady(
        quint64 operationId,
        const QVector<PlaylistSummary> &liked,
        const QVector<PlaylistSummary> &created);
    void validationReady(quint64 operationId, const UserProfile &profile);
    void validationFailed(const ServiceError &error);

private:
    enum class Action {
        None,
        Load,
        ValidateCandidate,
        StoreCandidate,
        Logout,
        RemoveExpired
    };

    enum class ValidationStage {
        None,
        AuthenticatedLibrary,
        UserProfile
    };

    void initializeValidationObjects();
    void beginValidation(const QVector<CookieRecord> &records, Action action);
    void reportFailure(const QString &diagnosticCode);
    void finishFailure(const QString &diagnosticCode);
    void clearCandidate();
    void setRemovalPending(bool pending);
    void activate(const QVector<CookieRecord> &records,
                  const UserProfile &profile);
    QString candidateUin(const QVector<CookieRecord> &records) const;
    bool hasUsableKey(const QVector<CookieRecord> &records) const;

    QPointer<QqCookieJar> m_activeJar;
    QPointer<QqCookieJar> m_candidateJar;
    QPointer<QqMusicService> m_validationService;
    QPointer<QObject> m_bridge;
    QNetworkAccessManager *m_ownedCandidateManager;
    QVector<CookieRecord> m_candidateRecords;
    UserProfile m_candidateProfile;
    UserProfile m_profile;
    quint64 m_nextOperationId;
    quint64 m_validationOperationId;
    Action m_action;
    ValidationStage m_validationStage;
    bool m_loggedIn;
    bool m_removalPending;
};

#endif
