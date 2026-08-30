TEMPLATE = app
CONFIG += testcase c++11
QT -= gui
QT += core testlib

isEmpty(TEST_SOURCE): error("TEST_SOURCE must name one tst_*.cpp")
TARGET = $$basename(TEST_SOURCE)
TEST_BASENAME = $$basename(TEST_SOURCE)
SOURCES += $$TEST_SOURCE

equals(TEST_BASENAME, tst_appmetadata.cpp) {
    SOURCES += ../../src/appmetadata.cpp
} else:equals(TEST_BASENAME, tst_core.cpp) {
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp
} else:equals(TEST_BASENAME, tst_qqpublicparser.cpp) {
    QT += gui network
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp \
        ../../src/service/qqrequestbuilder.cpp \
        ../../src/service/qqrequestpolicy.cpp \
        ../../src/service/qqpublicparser.cpp
} else:equals(TEST_BASENAME, tst_qqlibraryparser.cpp) {
    QT += gui network
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp \
        ../../src/service/qqrequestbuilder.cpp \
        ../../src/service/qqrequestpolicy.cpp \
        ../../src/service/qqlibraryparser.cpp
} else:equals(TEST_BASENAME, tst_qqlyricparser.cpp) {
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp \
        ../../src/service/qqrequestbuilder.cpp \
        ../../src/service/qqlyricparser.cpp
} else:equals(TEST_BASENAME, tst_qqplaybackparser.cpp) {
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp \
        ../../src/service/qqrequestbuilder.cpp \
        ../../src/service/qqplaybackparser.cpp
} else:equals(TEST_BASENAME, tst_qqrequestpolicy.cpp) {
    QT += network
    SOURCES += ../../src/service/qqrequestpolicy.cpp
} else:equals(TEST_BASENAME, tst_qqnetworktransport.cpp) {
    QT += network
    HEADERS += ../../src/service/qqnetworktransport.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/service/qqrequestpolicy.cpp \
        ../../src/service/qqnetworktransport.cpp
} else:equals(TEST_BASENAME, tst_nativeloginprotocol.cpp) {
    QT += gui network
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/session/cookiepolicy.cpp \
        ../../src/login/nativeloginprotocol.cpp
} else:equals(TEST_BASENAME, tst_nativeloginmanager.cpp) {
    QT += gui network
    HEADERS += \
        ../../src/login/nativeloginmanager.h \
        ../../src/login/nativelogintransport.h \
        ../../src/login/nativemqttprotocol.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/session/cookiepolicy.cpp \
        ../../src/login/nativemqttprotocol.cpp \
        ../../src/login/nativeloginprotocol.cpp \
        ../../src/login/nativelogintransport.cpp \
        ../../src/login/nativeloginmanager.cpp
} else:equals(TEST_BASENAME, tst_nativemqttprotocol.cpp) {
    HEADERS += ../../src/login/nativemqttprotocol.h
    SOURCES += ../../src/login/nativemqttprotocol.cpp
} else:equals(TEST_BASENAME, tst_qqmusicservice.cpp) {
    QT += network gui
    HEADERS += \
        ../../src/service/qqnetworktransport.h \
        ../../src/service/qqmusicservice.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp \
        ../../src/service/qqrequestbuilder.cpp \
        ../../src/service/qqrequestpolicy.cpp \
        ../../src/service/qqnetworktransport.cpp \
        ../../src/service/qqpublicparser.cpp \
        ../../src/service/qqlibraryparser.cpp \
        ../../src/service/qqlyricparser.cpp \
        ../../src/service/qqplaybackparser.cpp \
        ../../src/service/qqmusicservice.cpp
} else:equals(TEST_BASENAME, tst_models.cpp) {
    HEADERS += \
        ../../src/models/playlistlistmodel.h \
        ../../src/models/tracklistmodel.h \
        ../../src/models/lyriclistmodel.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/models/playlistlistmodel.cpp \
        ../../src/models/tracklistmodel.cpp \
        ../../src/models/lyriclistmodel.cpp
} else:equals(TEST_BASENAME, tst_applicationcontroller.cpp) {
    QT += network gui multimedia
    HEADERS += \
        ../../src/service/qqnetworktransport.h \
        ../../src/service/qqmusicservice.h \
        ../../src/models/playlistlistmodel.h \
        ../../src/models/tracklistmodel.h \
        ../../src/models/lyriclistmodel.h \
        ../../src/session/qqcookiejar.h \
        ../../src/session/sessionstore.h \
        ../../src/login/nativemqttprotocol.h \
        ../../src/login/nativelogintransport.h \
        ../../src/login/nativeloginmanager.h \
        ../../src/playback/audioengine.h \
        ../../src/playback/playbackcontroller.h \
        ../../src/storage/settingsstore.h \
        ../../src/storage/imagecache.h \
        ../../src/applicationcontroller.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp \
        ../../src/service/qqrequestbuilder.cpp \
        ../../src/service/qqrequestpolicy.cpp \
        ../../src/service/qqnetworktransport.cpp \
        ../../src/service/qqpublicparser.cpp \
        ../../src/service/qqlibraryparser.cpp \
        ../../src/service/qqlyricparser.cpp \
        ../../src/service/qqplaybackparser.cpp \
        ../../src/service/qqmusicservice.cpp \
        ../../src/models/playlistlistmodel.cpp \
        ../../src/models/tracklistmodel.cpp \
        ../../src/models/lyriclistmodel.cpp \
        ../../src/session/cookiepolicy.cpp \
        ../../src/session/qqcookiejar.cpp \
        ../../src/session/sessionstore.cpp \
        ../../src/login/nativemqttprotocol.cpp \
        ../../src/login/nativeloginprotocol.cpp \
        ../../src/login/nativelogintransport.cpp \
        ../../src/login/nativeloginmanager.cpp \
        ../../src/playback/audioengine.cpp \
        ../../src/playback/playbackcontroller.cpp \
        ../../src/storage/settingsstore.cpp \
        ../../src/storage/imagecache.cpp \
        ../../src/applicationcontroller.cpp
} else:equals(TEST_BASENAME, tst_cookiepolicy.cpp) {
    QT += network
    HEADERS += ../../src/session/qqcookiejar.h
    SOURCES += \
        ../../src/session/cookiepolicy.cpp \
        ../../src/session/qqcookiejar.cpp
} else:equals(TEST_BASENAME, tst_sessionstore.cpp) {
    QT += network gui
    HEADERS += \
        ../../src/service/qqnetworktransport.h \
        ../../src/service/qqmusicservice.h \
        ../../src/session/qqcookiejar.h \
        ../../src/session/sessionstore.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp \
        ../../src/service/qqrequestbuilder.cpp \
        ../../src/service/qqrequestpolicy.cpp \
        ../../src/service/qqnetworktransport.cpp \
        ../../src/service/qqpublicparser.cpp \
        ../../src/service/qqlibraryparser.cpp \
        ../../src/service/qqlyricparser.cpp \
        ../../src/service/qqplaybackparser.cpp \
        ../../src/service/qqmusicservice.cpp \
        ../../src/session/cookiepolicy.cpp \
        ../../src/session/qqcookiejar.cpp \
        ../../src/session/sessionstore.cpp
} else:equals(TEST_BASENAME, tst_secretidentifierfactory.cpp) {
    CONFIG += link_pkgconfig
    PKGCONFIG += sailfishsecrets
    HEADERS += ../../src/session/secretidentifierfactory.h
    SOURCES += ../../src/session/secretidentifierfactory.cpp
} else:equals(TEST_BASENAME, tst_audioengine.cpp) {
    QT += gui multimedia
    HEADERS += ../../src/playback/audioengine.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/playback/audioengine.cpp
} else:equals(TEST_BASENAME, tst_platformaudiomonitor.cpp) {
    QT += dbus
    HEADERS += ../../src/playback/platformaudiomonitor.h
    SOURCES += ../../src/playback/platformaudiomonitor.cpp
} else:equals(TEST_BASENAME, tst_playbackcontroller.cpp) {
    QT += network gui multimedia
    HEADERS += \
        ../../src/service/qqnetworktransport.h \
        ../../src/service/qqmusicservice.h \
        ../../src/playback/audioengine.h \
        ../../src/playback/playbackcontroller.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp \
        ../../src/service/qqrequestbuilder.cpp \
        ../../src/service/qqrequestpolicy.cpp \
        ../../src/service/qqnetworktransport.cpp \
        ../../src/service/qqpublicparser.cpp \
        ../../src/service/qqlibraryparser.cpp \
        ../../src/service/qqlyricparser.cpp \
        ../../src/service/qqplaybackparser.cpp \
        ../../src/service/qqmusicservice.cpp \
        ../../src/playback/audioengine.cpp \
        ../../src/playback/playbackcontroller.cpp
} else:equals(TEST_BASENAME, tst_mpriscontroller.cpp) {
    QT += network gui multimedia dbus
    HEADERS += \
        ../../src/service/qqnetworktransport.h \
        ../../src/service/qqmusicservice.h \
        ../../src/playback/audioengine.h \
        ../../src/playback/playbackcontroller.h \
        ../../src/playback/mpriscontroller.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/core/qqidentifier.cpp \
        ../../src/core/qqjsonreader.cpp \
        ../../src/service/qqrequestbuilder.cpp \
        ../../src/service/qqrequestpolicy.cpp \
        ../../src/service/qqnetworktransport.cpp \
        ../../src/service/qqpublicparser.cpp \
        ../../src/service/qqlibraryparser.cpp \
        ../../src/service/qqlyricparser.cpp \
        ../../src/service/qqplaybackparser.cpp \
        ../../src/service/qqmusicservice.cpp \
        ../../src/playback/audioengine.cpp \
        ../../src/playback/playbackcontroller.cpp \
        ../../src/playback/mpriscontroller.cpp
} else:equals(TEST_BASENAME, tst_settingsstore.cpp) {
    HEADERS += ../../src/storage/settingsstore.h
    SOURCES += \
        ../../src/core/types.cpp \
        ../../src/storage/settingsstore.cpp
} else:equals(TEST_BASENAME, tst_imagecache.cpp) {
    QT += gui network
    HEADERS += \
        ../../src/storage/settingsstore.h \
        ../../src/storage/imagecache.h
    SOURCES += \
        ../../src/service/qqrequestpolicy.cpp \
        ../../src/storage/settingsstore.cpp \
        ../../src/storage/imagecache.cpp
} else:equals(TEST_BASENAME, tst_translations.cpp) {
    TRANSLATION_SOURCE = $$PWD/../../translations/harbour-qqmusic-zh_CN.ts
    TRANSLATION_QM = $$OUT_PWD/harbour-qqmusic-zh_CN.qm
    qqmusic_translation.target = $$TRANSLATION_QM
    qqmusic_translation.depends = $$TRANSLATION_SOURCE
    qqmusic_translation.commands = $$[QT_INSTALL_BINS]/lrelease \
        $$shell_quote($$TRANSLATION_SOURCE) -qm \
        $$shell_quote($$TRANSLATION_QM)
    QMAKE_EXTRA_TARGETS += qqmusic_translation
    PRE_TARGETDEPS += $$TRANSLATION_QM
    DEFINES += QQMUSIC_TRANSLATION_QM=\\\"$$TRANSLATION_QM\\\"
} else {
    error("Unsupported TEST_SOURCE: $$TEST_BASENAME")
}

INCLUDEPATH += ../../src
