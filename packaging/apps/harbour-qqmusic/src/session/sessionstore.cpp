#include "sessionstore.h"

#include "core/qqidentifier.h"
#include "qqcookiejar.h"
#include "service/qqmusicservice.h"

#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QVariant>

SessionStore::SessionStore(QqCookieJar *activeJar, QObject *parent)
    : QObject(parent)
    , m_activeJar(activeJar)
    , m_ownedCandidateManager(0)
    , m_nextOperationId(1000000)
    , m_validationOperationId(0)
    , m_action(Action::None)
    , m_validationStage(ValidationStage::None)
    , m_loggedIn(false)
    , m_removalPending(false)
{
    initializeValidationObjects();
}

SessionStore::SessionStore(QqCookieJar *activeJar,
                           QqMusicService *validationService,
                           QqCookieJar *candidateJar,
                           QObject *parent)
    : QObject(parent)
    , m_activeJar(activeJar)
    , m_candidateJar(candidateJar)
    , m_validationService(validationService)
    , m_ownedCandidateManager(0)
    , m_nextOperationId(1000000)
    , m_validationOperationId(0)
    , m_action(Action::None)
    , m_validationStage(ValidationStage::None)
    , m_loggedIn(false)
    , m_removalPending(false)
{
    if (m_validationService) {
        connect(m_validationService, &QqMusicService::createdLibraryReady,
                this, &SessionStore::validationLibraryReady);
        connect(m_validationService, &QqMusicService::userReady,
                this, &SessionStore::validationReady);
        connect(m_validationService, &QqMusicService::failed,
                this, &SessionStore::validationFailed);
    }
}

void SessionStore::initializeValidationObjects()
{
    m_ownedCandidateManager = new QNetworkAccessManager(this);
    QqCookieJar *jar = new QqCookieJar(m_ownedCandidateManager);
    m_ownedCandidateManager->setCookieJar(jar);
    m_candidateJar = jar;
    QqMusicService *service = new QqMusicService(m_ownedCandidateManager, this);
    m_validationService = service;
    connect(service, &QqMusicService::createdLibraryReady,
            this, &SessionStore::validationLibraryReady);
    connect(service, &QqMusicService::userReady,
            this, &SessionStore::validationReady);
    connect(service, &QqMusicService::failed,
            this, &SessionStore::validationFailed);
}

bool SessionStore::bindBridge(QObject *bridge)
{
    if (!bridge || m_bridge) {
        return false;
    }
    const bool loadConnected = connect(
        bridge, SIGNAL(loadFinished(bool,int,QString)),
        this, SLOT(bridgeLoadFinished(bool,int,QString)));
    const bool storeConnected = connect(
        bridge, SIGNAL(storeFinished(bool,int)),
        this, SLOT(bridgeStoreFinished(bool,int)));
    const bool deleteConnected = connect(
        bridge, SIGNAL(deleteFinished(bool,int)),
        this, SLOT(bridgeDeleteFinished(bool,int)));
    if (!loadConnected || !storeConnected || !deleteConnected) {
        bridge->disconnect(this);
        return false;
    }
    m_bridge = bridge;
    return true;
}

bool SessionStore::loggedIn() const
{
    return m_loggedIn;
}

QString SessionStore::nickname() const
{
    return m_profile.nickname;
}

QUrl SessionStore::avatar() const
{
    return m_profile.avatarUrl;
}

UserProfile SessionStore::profile() const
{
    return m_profile;
}

bool SessionStore::removalPending() const
{
    return m_removalPending;
}

void SessionStore::load()
{
    if (!m_bridge || m_action != Action::None) {
        return;
    }
    m_action = Action::Load;
    if (!QMetaObject::invokeMethod(m_bridge, "loadSecret")) {
        finishFailure(QStringLiteral("qq.session.bridge.load.invoke"));
    }
}

void SessionStore::submitCandidate(const QVector<CookieRecord> &records)
{
    qInfo().noquote().nospace()
        << "qqmusic-login session candidate_received cookies="
        << records.size();
    if (m_action == Action::Load) {
        const quint64 restoreOperationId = m_validationOperationId;
        clearCandidate();
        m_action = Action::None;
        if (m_validationService && restoreOperationId != 0) {
            m_validationService->cancel(restoreOperationId);
        }
        qInfo().noquote()
            << "qqmusic-login session restore_superseded_by_candidate";
    }
    if (m_action != Action::None) {
        reportFailure(QStringLiteral("qq.session.candidate.busy"));
        emit candidateFailed();
        return;
    }
    beginValidation(records, Action::ValidateCandidate);
}

void SessionStore::logout()
{
    if (m_action != Action::None) {
        reportFailure(QStringLiteral("qq.session.logout.busy"));
        emit sessionActionFailed();
        return;
    }
    if (!m_bridge) {
        reportFailure(QStringLiteral("qq.session.logout.unavailable"));
        emit sessionActionFailed();
        return;
    }
    m_action = Action::Logout;
    if (!QMetaObject::invokeMethod(m_bridge, "deleteSecret")) {
        finishFailure(QStringLiteral("qq.session.bridge.delete.invoke"));
    }
}

void SessionStore::markAuthenticationExpired()
{
    setRemovalPending(true);
    const Action interruptedAction = m_action;
    if (interruptedAction == Action::Load
            || interruptedAction == Action::ValidateCandidate
            || interruptedAction == Action::StoreCandidate) {
        const quint64 validationOperationId = m_validationOperationId;
        clearCandidate();
        m_action = Action::None;
        if (m_validationService && validationOperationId != 0) {
            m_validationService->cancel(validationOperationId);
        }
        if (interruptedAction == Action::ValidateCandidate
                || interruptedAction == Action::StoreCandidate) {
            emit candidateFailed();
        }
    }
    if (m_activeJar) {
        m_activeJar->clearSession();
    }
    const bool changed = m_loggedIn || !m_profile.uin.isEmpty();
    m_loggedIn = false;
    m_profile = UserProfile();
    if (changed) {
        emit sessionChanged();
        emit sessionCleared();
    }
}

void SessionStore::confirmExpiredSessionRemoval()
{
    if (!m_removalPending || m_loggedIn) {
        return;
    }
    if (!m_bridge || m_action != Action::None) {
        reportFailure(QStringLiteral("qq.session.expired-delete.unavailable"));
        emit sessionActionFailed();
        return;
    }
    m_action = Action::RemoveExpired;
    if (!QMetaObject::invokeMethod(m_bridge, "deleteSecret")) {
        finishFailure(QStringLiteral("qq.session.bridge.expired-delete.invoke"));
    }
}

void SessionStore::bridgeLoadFinished(bool succeeded,
                                      int,
                                      const QString &payloadBase64)
{
    if (m_action != Action::Load) {
        return;
    }
    if (!succeeded) {
        finishFailure(QStringLiteral("qq.session.secret.load"));
        return;
    }
    if (payloadBase64.isEmpty()) {
        clearCandidate();
        m_action = Action::None;
        qInfo().noquote()
            << "qqmusic-login session no_stored_secret";
        return;
    }
    const QByteArray encoded = payloadBase64.toLatin1();
    const QByteArray payload = QByteArray::fromBase64(encoded);
    if (payload.toBase64() != encoded) {
        finishFailure(QStringLiteral("qq.session.secret.base64"));
        return;
    }
    const ServiceResult<QVector<CookieRecord> > decoded =
        CookiePolicy::deserialize(payload, QDateTime::currentDateTimeUtc());
    if (!decoded.ok()) {
        if (decoded.error.kind == ErrorKind::AuthenticationExpired) {
            setRemovalPending(true);
        }
        finishFailure(QStringLiteral("qq.session.secret.corrupt"));
        return;
    }
    beginValidation(decoded.value, Action::Load);
}

void SessionStore::bridgeStoreFinished(bool succeeded, int)
{
    if (m_action != Action::StoreCandidate) {
        return;
    }
    if (!succeeded) {
        finishFailure(QStringLiteral("qq.session.secret.store"));
        return;
    }
    qInfo().noquote()
        << "qqmusic-login session secret_store_ok";
    const QVector<CookieRecord> records = m_candidateRecords;
    const UserProfile profile = m_candidateProfile;
    clearCandidate();
    m_action = Action::None;
    activate(records, profile);
}

void SessionStore::bridgeDeleteFinished(bool succeeded, int)
{
    if (m_action != Action::Logout && m_action != Action::RemoveExpired) {
        return;
    }
    if (!succeeded) {
        finishFailure(QStringLiteral("qq.session.secret.delete"));
        return;
    }
    const Action completedAction = m_action;
    m_action = Action::None;
    if (m_activeJar) {
        m_activeJar->clearSession();
    }
    clearCandidate();
    const bool changed = m_loggedIn || !m_profile.uin.isEmpty();
    m_loggedIn = false;
    m_profile = UserProfile();
    if (changed) {
        emit sessionChanged();
    }
    if (completedAction == Action::RemoveExpired
            || completedAction == Action::Logout) {
        setRemovalPending(false);
    }
    emit sessionCleared();
}

void SessionStore::validationLibraryReady(
    quint64 operationId,
    const QVector<PlaylistSummary> &,
    const QVector<PlaylistSummary> &)
{
    if (operationId != m_validationOperationId
            || m_validationStage != ValidationStage::AuthenticatedLibrary
            || (m_action != Action::Load
                && m_action != Action::ValidateCandidate)) {
        return;
    }
    m_validationStage = ValidationStage::UserProfile;
    if (!m_validationService->requestUser(
            operationId, candidateUin(m_candidateRecords))) {
        finishFailure(QStringLiteral("qq.session.validation.profile.start"));
    }
}

void SessionStore::validationReady(quint64 operationId,
                                   const UserProfile &profile)
{
    if (operationId != m_validationOperationId
            || m_validationStage != ValidationStage::UserProfile
            || (m_action != Action::Load
                && m_action != Action::ValidateCandidate)) {
        return;
    }
    if (profile.uin != candidateUin(m_candidateRecords)) {
        finishFailure(QStringLiteral("qq.session.validation.identity"));
        return;
    }
    m_candidateProfile = profile;
    if (m_action == Action::Load) {
        qInfo().noquote()
            << "qqmusic-login session load_validation_ok";
        const QVector<CookieRecord> records = m_candidateRecords;
        clearCandidate();
        m_action = Action::None;
        activate(records, profile);
        return;
    }
    if (!m_bridge) {
        finishFailure(QStringLiteral("qq.session.bridge.store.unavailable"));
        return;
    }
    const QByteArray payload = CookiePolicy::serialize(m_candidateRecords);
    const QString encoded = QString::fromLatin1(payload.toBase64());
    m_action = Action::StoreCandidate;
    qInfo().noquote().nospace()
        << "qqmusic-login session validation_ok store_bytes="
        << payload.size();
    if (!m_bridge->setProperty(
            "stagedStorePayloadBase64", QVariant(encoded))) {
        finishFailure(QStringLiteral("qq.session.bridge.store.property"));
        return;
    }
    qInfo().noquote()
        << "qqmusic-login session secret_store_dispatch mode=property-timer";
}

void SessionStore::validationFailed(const ServiceError &error)
{
    if (error.operationId != m_validationOperationId
            || m_validationStage == ValidationStage::None
            || (m_action != Action::Load
                && m_action != Action::ValidateCandidate)) {
        return;
    }
    if (m_action == Action::Load
            && error.kind == ErrorKind::AuthenticationExpired) {
        setRemovalPending(true);
    }
    qInfo().noquote().nospace()
        << "qqmusic-login session validation_failed diagnostic="
        << error.diagnosticCode;
    const QString diagnosticCode = error.diagnosticCode.startsWith(
        QStringLiteral("qq."))
        ? error.diagnosticCode
        : QStringLiteral("qq.session.validation.failed");
    finishFailure(diagnosticCode);
}

void SessionStore::beginValidation(const QVector<CookieRecord> &records,
                                   Action action)
{
    m_action = action;
    const ServiceResult<QVector<CookieRecord> > validated =
        CookiePolicy::validateAll(records, QDateTime::currentDateTimeUtc());
    const bool identityReady = validated.ok()
        && !candidateUin(validated.value).isEmpty();
    const bool keyReady = validated.ok()
        && hasUsableKey(validated.value);
    qInfo().noquote().nospace()
        << "qqmusic-login session validation_begin cookies="
        << records.size()
        << " policy_ok=" << int(validated.ok())
        << " identity_ready=" << int(identityReady)
        << " music_key_ready=" << int(keyReady);
    if (!validated.ok()
            || !identityReady
            || !keyReady
            || !m_candidateJar
            || !m_validationService) {
        finishFailure(QStringLiteral("qq.session.candidate.invalid"));
        return;
    }
    m_candidateRecords = validated.value;
    m_candidateJar->replaceRecords(m_candidateRecords);
    ++m_nextOperationId;
    if (m_nextOperationId == 0) {
        ++m_nextOperationId;
    }
    m_validationOperationId = m_nextOperationId;
    m_validationStage = ValidationStage::AuthenticatedLibrary;
    qInfo().noquote().nospace()
        << "qqmusic-login session validation_request operation="
        << m_validationOperationId;
    if (!m_validationService->requestCreatedLibrary(
            m_validationOperationId, candidateUin(m_candidateRecords))) {
        finishFailure(QStringLiteral("qq.session.validation.start"));
    }
}

void SessionStore::finishFailure(const QString &diagnosticCode)
{
    const Action failedAction = m_action;
    clearCandidate();
    m_action = Action::None;
    qInfo().noquote().nospace()
        << "qqmusic-login session failed diagnostic="
        << diagnosticCode;
    reportFailure(diagnosticCode);
    if (failedAction == Action::ValidateCandidate
            || failedAction == Action::StoreCandidate) {
        emit candidateFailed();
    } else if (failedAction == Action::Logout
               || failedAction == Action::RemoveExpired) {
        emit sessionActionFailed();
    }
}

void SessionStore::reportFailure(const QString &diagnosticCode)
{
    ServiceError error;
    error.kind = ErrorKind::Internal;
    error.diagnosticCode = diagnosticCode;
    emit sessionFailed(error);
}

void SessionStore::clearCandidate()
{
    if (m_candidateJar) {
        m_candidateJar->clearSession();
    }
    m_candidateRecords.clear();
    m_candidateProfile = UserProfile();
    m_validationOperationId = 0;
    m_validationStage = ValidationStage::None;
}

void SessionStore::setRemovalPending(bool pending)
{
    if (m_removalPending != pending) {
        m_removalPending = pending;
        emit removalPendingChanged();
    }
}

void SessionStore::activate(const QVector<CookieRecord> &records,
                            const UserProfile &profile)
{
    if (m_activeJar) {
        m_activeJar->replaceRecords(records);
    }
    m_profile = profile;
    m_loggedIn = true;
    setRemovalPending(false);
    qInfo().noquote()
        << "qqmusic-login session activated";
    emit sessionChanged();
    emit sessionActivated(profile);
}

QString SessionStore::candidateUin(
    const QVector<CookieRecord> &records) const
{
    static const char *const names[] = { "uin", "wxuin", "pt2gguin" };
    for (uint nameIndex = 0;
         nameIndex < sizeof(names) / sizeof(names[0]); ++nameIndex) {
        for (int index = 0; index < records.size(); ++index) {
            if (records.at(index).name != names[nameIndex]) {
                continue;
            }
            QString value = QString::fromLatin1(records.at(index).value);
            while (value.startsWith(QLatin1Char('o'))) {
                value.remove(0, 1);
            }
            if (QqIdentifier::isNumericPlaylistId(value)) {
                return value;
            }
        }
    }
    return QString();
}

bool SessionStore::hasUsableKey(
    const QVector<CookieRecord> &records) const
{
    for (int index = 0; index < records.size(); ++index) {
        const QByteArray name = records.at(index).name;
        if (name == "qm_keyst" || name == "qqmusic_key"
                || name == "p_skey" || name == "skey") {
            return true;
        }
    }
    return false;
}
