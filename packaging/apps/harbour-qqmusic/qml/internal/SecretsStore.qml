import QtQuick 2.0
import Sailfish.Secrets 1.0 as Secrets

QtObject {
    id: root
    objectName: "qqSecretsStore"

    signal loadFinished(bool succeeded, int errorCode, string payloadBase64)
    signal storeFinished(bool succeeded, int errorCode)
    signal deleteFinished(bool succeeded, int errorCode)

    // Keep the locked v1 collection untouched.  The fixed v2 item is read as
    // a compatibility fallback only: Sailfish Secrets deliberately rejects
    // overwriting an existing item, so every validated candidate is committed
    // into a new immutable v3 slot before it can replace the active session.
    readonly property string legacySecretName: "qq-session-v1"
    readonly property string legacySessionCollectionName:
        "org.bonito.qqmusic.sessions.v1"
    readonly property string fallbackSecretName: "qq-session-v2"
    readonly property string fallbackSessionCollectionName:
        "org.bonito.qqmusic.sessions.v2"
    readonly property string secretNamePrefix: "qq-session-v3-"
    readonly property string sessionSchema: "3"
    readonly property string sessionCollectionName:
        "org.bonito.zzmusic.sessions.v3"
    property var identifierFactory: null
    property string stagedStorePayloadBase64: ""
    property string pendingStorePayloadBase64: ""
    property string pendingSecretName: ""
    property string activeSecretName: ""
    property string activeCollectionName: ""
    property var loadCandidateNames: []
    property int loadCandidateIndex: 0

    onStagedStorePayloadBase64Changed: {
        if (stagedStorePayloadBase64.length > 0
                && !storeDispatchTimer.running)
            storeDispatchTimer.start()
    }

    function fallbackSecretIdentifier() {
        return identifierFactory
                ? identifierFactory.sessionIdentifier(
                      fallbackSessionCollectionName,
                      manager.defaultStoragePluginName)
                : null
    }

    function namedSecretIdentifier(secretName, collectionName) {
        return identifierFactory
                ? identifierFactory.namedSessionIdentifier(
                      secretName, collectionName,
                      manager.defaultStoragePluginName)
                : null
    }

    function isMissingSecretError(errorCode) {
        return errorCode === Secrets.Result.InvalidSecretError
                || errorCode === Secrets.Result.InvalidCollectionError
    }

    function isMissingCollectionError(errorCode) {
        return errorCode === Secrets.Result.InvalidCollectionError
    }

    function latestSessionNames(identifiers) {
        var names = []
        for (var index = 0; index < identifiers.length; ++index) {
            var name = identifiers[index].name
            if (name && name.indexOf(secretNamePrefix) === 0)
                names.push(name)
        }
        names.sort()
        names.reverse()
        return names
    }

    function loadSecret() {
        if (!identifierFactory) {
            root.loadFinished(false, -1, "")
            return
        }
        activeSecretName = ""
        activeCollectionName = ""
        loadCandidateNames = []
        loadCandidateIndex = 0
        findRequest.filter = manager.constructFilterData({
            "zzmusic-session-schema": sessionSchema
        })
        findRequest.startRequest()
    }

    function retrieveNextVersionedSecret() {
        if (loadCandidateIndex >= loadCandidateNames.length) {
            retrieveFallbackSecret()
            return
        }
        activeSecretName = loadCandidateNames[loadCandidateIndex]
        activeCollectionName = sessionCollectionName
        ++loadCandidateIndex
        var identifier = namedSecretIdentifier(activeSecretName,
                                               activeCollectionName)
        if (!identifier) {
            root.loadFinished(false, -1, "")
            return
        }
        retrieveRequest.identifier = identifier
        retrieveRequest.startRequest()
    }

    function retrieveFallbackSecret() {
        var identifier = fallbackSecretIdentifier()
        if (!identifier) {
            root.loadFinished(false, -1, "")
            return
        }
        activeSecretName = fallbackSecretName
        activeCollectionName = fallbackSessionCollectionName
        retrieveRequest.identifier = identifier
        retrieveRequest.startRequest()
    }

    function storeSecret() {
        console.info("qqmusic-login secrets store_dispatch_entered")
        if (!identifierFactory || stagedStorePayloadBase64.length === 0
                || pendingStorePayloadBase64.length > 0
                || createCollectionRequest.status === Secrets.Request.Active
                || storeRequest.status === Secrets.Request.Active) {
            stagedStorePayloadBase64 = ""
            root.storeFinished(false, -1)
            return
        }
        pendingStorePayloadBase64 = stagedStorePayloadBase64
        stagedStorePayloadBase64 = ""
        pendingSecretName = identifierFactory.newSessionSecretName()
        if (!pendingSecretName
                || pendingSecretName.indexOf(secretNamePrefix) !== 0) {
            pendingStorePayloadBase64 = ""
            pendingSecretName = ""
            root.storeFinished(false, -1)
            return
        }
        console.info("qqmusic-login secrets create_collection_start")
        createCollectionRequest.startRequest()
    }

    function startPendingStore() {
        if (pendingStorePayloadBase64.length === 0) {
            root.storeFinished(false, -1)
            return
        }
        var secret = manager.constructSecret()
        secret.name = pendingSecretName
        secret.collectionName = sessionCollectionName
        secret.storagePluginName = manager.defaultStoragePluginName
        secret.type = "Blob"
        secret.data = manager.fromBase64(pendingStorePayloadBase64)
        secret.setFilterData("zzmusic-session-schema", sessionSchema)
        storeRequest.secret = secret
        console.info("qqmusic-login secrets collection_store_start")
        storeRequest.startRequest()
    }

    function deleteSecret() {
        if (!identifierFactory) {
            root.deleteFinished(false, -1)
            return
        }
        // Logout removes the complete app-owned v3 collection first, then the
        // fixed v2 compatibility collection.  Otherwise an older immutable
        // slot could become active again after a later restart.
        deleteCurrentCollectionRequest.startRequest()
    }

    property QtObject secretManagerObject: Secrets.SecretManager {
        id: manager
    }

    property QtObject storeDispatchTimerObject: Timer {
        id: storeDispatchTimer
        interval: 1
        repeat: false
        onTriggered: root.storeSecret()
    }

    property QtObject retrieveRequestObject: Secrets.StoredSecretRequest {
        id: retrieveRequest
        manager: manager
        // A device-lock protected collection can remain locked after a cold
        // start or secrets-daemon restart.  Permit the system verification
        // flow so the existing session can be retrieved without deleting or
        // replacing its secret.
        userInteractionMode: Secrets.SecretManager.SystemInteraction
        onStatusChanged: {
            if (status !== Secrets.Request.Finished)
                return
            var ok = result.code === Secrets.Result.Succeeded
            var missing = !ok && root.isMissingSecretError(result.errorCode)
            console.info("qqmusic-login secrets load_finished"
                         + " result=" + result.code
                         + " error=" + result.errorCode)
            if (ok) {
                root.loadFinished(true, result.errorCode,
                                  manager.toBase64(secret.data))
            } else if (missing
                       && root.activeCollectionName
                          === root.sessionCollectionName) {
                root.retrieveNextVersionedSecret()
            } else {
                root.activeSecretName = ""
                root.activeCollectionName = ""
                root.loadFinished(missing, result.errorCode, "")
            }
        }
    }

    property QtObject findRequestObject: Secrets.FindSecretsRequest {
        id: findRequest
        manager: manager
        collectionName: root.sessionCollectionName
        storagePluginName: manager.defaultStoragePluginName
        filterOperator: Secrets.SecretManager.OperatorAnd
        userInteractionMode: Secrets.SecretManager.SystemInteraction
        onStatusChanged: {
            if (status !== Secrets.Request.Finished)
                return
            var ok = result.code === Secrets.Result.Succeeded
            console.info("qqmusic-login secrets find_finished"
                         + " result=" + result.code
                         + " error=" + result.errorCode
                         + " identifiers=" + (ok ? identifiers.length : 0))
            if (ok) {
                root.loadCandidateNames = root.latestSessionNames(identifiers)
                root.loadCandidateIndex = 0
                root.retrieveNextVersionedSecret()
            } else if (root.isMissingCollectionError(result.errorCode)) {
                root.retrieveFallbackSecret()
            } else {
                root.loadFinished(false, result.errorCode, "")
            }
        }
    }

    property QtObject createCollectionRequestObject:
            Secrets.CreateCollectionRequest {
        id: createCollectionRequest
        manager: manager
        collectionName: root.sessionCollectionName
        deviceLockUnlockSemantic: Secrets.SecretManager.DeviceLockKeepUnlocked
        accessControlMode: Secrets.SecretManager.OwnerOnlyMode
        storagePluginName: manager.defaultStoragePluginName
        encryptionPluginName: manager.defaultEncryptionPluginName
        // A cold start or daemon restart can relock the collection. Let the
        // system unlock it rather than discard a successfully scanned login.
        userInteractionMode: Secrets.SecretManager.SystemInteraction
        onStatusChanged: {
            if (status !== Secrets.Request.Finished)
                return
            console.info("qqmusic-login secrets create_collection_finished"
                         + " result=" + result.code
                         + " error=" + result.errorCode)
            if (result.code !== Secrets.Result.Succeeded
                    && result.errorCode
                       !== Secrets.Result.CollectionAlreadyExistsError) {
                var errorCode = result.errorCode
                root.pendingStorePayloadBase64 = ""
                root.storeFinished(false, errorCode)
                return
            }
            root.startPendingStore()
        }
    }

    property QtObject storeRequestObject: Secrets.StoreSecretRequest {
        id: storeRequest
        manager: manager
        userInteractionMode: Secrets.SecretManager.SystemInteraction
        onStatusChanged: {
            if (status === Secrets.Request.Finished) {
                console.info("qqmusic-login secrets collection_store_finished"
                             + " result=" + result.code
                             + " error=" + result.errorCode)
                var ok = result.code === Secrets.Result.Succeeded
                if (ok) {
                    root.activeSecretName = root.pendingSecretName
                    root.activeCollectionName = root.sessionCollectionName
                }
                root.pendingStorePayloadBase64 = ""
                root.pendingSecretName = ""
                root.storeFinished(ok, result.errorCode)
            }
        }
    }

    property QtObject deleteCurrentCollectionRequestObject:
            Secrets.DeleteCollectionRequest {
        id: deleteCurrentCollectionRequest
        manager: manager
        collectionName: root.sessionCollectionName
        storagePluginName: manager.defaultStoragePluginName
        userInteractionMode: Secrets.SecretManager.PreventInteraction
        onStatusChanged: {
            if (status !== Secrets.Request.Finished)
                return
            var ok = result.code === Secrets.Result.Succeeded
                    || root.isMissingCollectionError(result.errorCode)
            if (!ok) {
                root.deleteFinished(false, result.errorCode)
                return
            }
            deleteFallbackCollectionRequest.startRequest()
        }
    }

    property QtObject deleteFallbackCollectionRequestObject:
            Secrets.DeleteCollectionRequest {
        id: deleteFallbackCollectionRequest
        manager: manager
        collectionName: root.fallbackSessionCollectionName
        storagePluginName: manager.defaultStoragePluginName
        userInteractionMode: Secrets.SecretManager.PreventInteraction
        onStatusChanged: {
            if (status !== Secrets.Request.Finished)
                return
            var ok = result.code === Secrets.Result.Succeeded
                    || root.isMissingCollectionError(result.errorCode)
            if (ok) {
                root.activeSecretName = ""
                root.activeCollectionName = ""
                root.loadCandidateNames = []
                root.loadCandidateIndex = 0
            }
            root.deleteFinished(ok, result.errorCode)
        }
    }
}
