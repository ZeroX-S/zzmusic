import QtQuick 2.0
import QtTest 1.1
import Sailfish.Secrets 1.0 as Secrets
import "../../qml/internal" as Internal

TestCase {
    name: "SecretsBridge"

    Internal.SecretsStore {
        id: secretsStore
    }

    QtObject {
        id: validIdentifierFactory
        property int sequence: 0
        function newSessionSecretName() {
            ++sequence
            return "qq-session-v3-test-" + sequence
        }
    }

    Internal.SecretsStore {
        id: dispatchStore
        identifierFactory: validIdentifierFactory
    }

    Internal.SecretsStore {
        id: pendingStore
        identifierFactory: validIdentifierFactory
    }

    SignalSpy {
        id: loadSpy
        target: secretsStore
        signalName: "loadFinished"
    }

    SignalSpy {
        id: deleteSpy
        target: secretsStore
        signalName: "deleteFinished"
    }

    SignalSpy {
        id: storeSpy
        target: secretsStore
        signalName: "storeFinished"
    }

    function test_storeUsesDeviceLockCollectionWithDefaultPlugins() {
        var manager = secretsStore.secretManagerObject
        var createRequest = secretsStore.createCollectionRequestObject
        var retrieveRequest = secretsStore.retrieveRequestObject
        var storeRequest = secretsStore.storeRequestObject
        var findRequest = secretsStore.findRequestObject
        var deleteCurrentRequest =
                secretsStore.deleteCurrentCollectionRequestObject
        var deleteFallbackRequest =
                secretsStore.deleteFallbackCollectionRequestObject
        compare(secretsStore.fallbackSecretName, "qq-session-v2")
        compare(secretsStore.fallbackSessionCollectionName,
                "org.bonito.qqmusic.sessions.v2")
        compare(secretsStore.secretNamePrefix, "qq-session-v3-")
        compare(secretsStore.sessionSchema, "3")
        compare(secretsStore.sessionCollectionName,
                "org.bonito.zzmusic.sessions.v3")
        compare(secretsStore.legacySecretName, "qq-session-v1")
        compare(secretsStore.legacySessionCollectionName,
                "org.bonito.qqmusic.sessions.v1")
        verify(secretsStore.fallbackSecretName
               !== secretsStore.legacySecretName)
        verify(secretsStore.sessionCollectionName
               !== secretsStore.legacySessionCollectionName)
        compare(createRequest.collectionName,
                secretsStore.sessionCollectionName)
        compare(findRequest.collectionName,
                secretsStore.sessionCollectionName)
        verify(createRequest.collectionName.length > 0)
        compare(createRequest.storagePluginName,
                manager.defaultStoragePluginName)
        compare(createRequest.encryptionPluginName,
                manager.defaultEncryptionPluginName)
        verify(createRequest.storagePluginName.length > 0)
        compare(createRequest.deviceLockUnlockSemantic,
                Secrets.SecretManager.DeviceLockKeepUnlocked)
        compare(createRequest.accessControlMode,
                Secrets.SecretManager.OwnerOnlyMode)
        compare(retrieveRequest.userInteractionMode,
                Secrets.SecretManager.SystemInteraction)
        compare(findRequest.userInteractionMode,
                Secrets.SecretManager.SystemInteraction)
        compare(findRequest.filterOperator,
                Secrets.SecretManager.OperatorAnd)
        compare(createRequest.userInteractionMode,
                Secrets.SecretManager.PreventInteraction)
        compare(storeRequest.userInteractionMode,
                Secrets.SecretManager.PreventInteraction)
        compare(deleteCurrentRequest.collectionName,
                secretsStore.sessionCollectionName)
        compare(deleteFallbackRequest.collectionName,
                secretsStore.fallbackSessionCollectionName)
        compare(deleteCurrentRequest.userInteractionMode,
                Secrets.SecretManager.PreventInteraction)
        compare(deleteFallbackRequest.userInteractionMode,
                Secrets.SecretManager.PreventInteraction)
    }

    function test_latestVersionedSlotWinsWithoutUsingForeignNames() {
        var names = secretsStore.latestSessionNames([
            { "name": "foreign-secret" },
            { "name": "qq-session-v3-0000000000100-a" },
            { "name": "qq-session-v3-0000000000300-c" },
            { "name": "qq-session-v3-0000000000200-b" }
        ])
        compare(names.length, 3)
        compare(names[0], "qq-session-v3-0000000000300-c")
        compare(names[1], "qq-session-v3-0000000000200-b")
        compare(names[2], "qq-session-v3-0000000000100-a")
    }

    function test_missingSecretErrorsAreNormalSignedOutState() {
        verify(secretsStore.isMissingSecretError(
                   Secrets.Result.InvalidSecretError))
        verify(secretsStore.isMissingSecretError(
                   Secrets.Result.InvalidCollectionError))
        verify(!secretsStore.isMissingSecretError(
                   Secrets.Result.CollectionIsLockedError))
        verify(!secretsStore.isMissingSecretError(
                   Secrets.Result.UnknownError))
    }

    function test_missingIdentifierFactoryFailsClosed() {
        secretsStore.loadSecret()
        compare(loadSpy.count, 1)
        compare(loadSpy.signalArguments[0][0], false)

        secretsStore.deleteSecret()
        compare(deleteSpy.count, 1)
        compare(deleteSpy.signalArguments[0][0], false)

        secretsStore.stagedStorePayloadBase64 = "e30="
        tryCompare(storeSpy, "count", 1)
        compare(storeSpy.signalArguments[0][0], false)
        compare(secretsStore.stagedStorePayloadBase64, "")
    }

    function test_validFactoryDispatchesCollectionCreate() {
        var createRequest = dispatchStore.createCollectionRequestObject
        compare(createRequest.status, Secrets.Request.Inactive)

        dispatchStore.stagedStorePayloadBase64 = "e30="

        tryCompare(dispatchStore, "stagedStorePayloadBase64", "")
        verify(createRequest.status === Secrets.Request.Active
               || createRequest.status === Secrets.Request.Finished)
    }

    function test_validFactoryDispatchesCollectionStore() {
        var storeRequest = pendingStore.storeRequestObject
        compare(storeRequest.status, Secrets.Request.Inactive)
        pendingStore.pendingStorePayloadBase64 = "e30="
        pendingStore.pendingSecretName = "qq-session-v3-test"

        pendingStore.startPendingStore()
        wait(50)

        verify(storeRequest.status === Secrets.Request.Active
               || storeRequest.status === Secrets.Request.Finished)
        compare(storeRequest.secret.name, "qq-session-v3-test")
        compare(storeRequest.secret.filterData("zzmusic-session-schema"), "3")
    }
}
