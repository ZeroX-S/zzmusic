TEMPLATE = app
TARGET = harbour-qqmusic

CONFIG += c++11 link_pkgconfig
QT += core gui qml quick network multimedia dbus
PKGCONFIG += sailfishapp sailfishsecrets

SOURCES += \
    main.cpp \
    appmetadata.cpp \
    applicationcontroller.cpp \
    core/types.cpp \
    core/qqidentifier.cpp \
    core/qqjsonreader.cpp \
    login/nativemqttprotocol.cpp \
    login/nativeloginprotocol.cpp \
    login/nativelogintransport.cpp \
    login/nativeloginmanager.cpp \
    playback/audioengine.cpp \
    playback/platformaudiomonitor.cpp \
    playback/playbackcontroller.cpp \
    playback/mpriscontroller.cpp \
    service/qqrequestbuilder.cpp \
    service/qqrequestpolicy.cpp \
    service/qqnetworktransport.cpp \
    service/qqmusicservice.cpp \
    service/qqpublicparser.cpp \
    service/qqlibraryparser.cpp \
    service/qqlyricparser.cpp \
    service/qqplaybackparser.cpp \
    session/cookiepolicy.cpp \
    session/qqcookiejar.cpp \
    session/secretidentifierfactory.cpp \
    session/sessionstore.cpp \
    storage/settingsstore.cpp \
    storage/imagecache.cpp \
    models/playlistlistmodel.cpp \
    models/tracklistmodel.cpp \
    models/lyriclistmodel.cpp

HEADERS += \
    appmetadata.h \
    applicationcontroller.h \
    core/types.h \
    core/qqidentifier.h \
    core/qqjsonreader.h \
    login/nativemqttprotocol.h \
    login/nativeloginprotocol.h \
    login/nativelogintransport.h \
    login/nativeloginmanager.h \
    playback/audioengine.h \
    playback/platformaudiomonitor.h \
    playback/playbackcontroller.h \
    playback/mpriscontroller.h \
    service/qqrequestbuilder.h \
    service/qqrequestpolicy.h \
    service/qqnetworktransport.h \
    service/qqmusicservice.h \
    service/qqpublicparser.h \
    service/qqlibraryparser.h \
    service/qqlyricparser.h \
    service/qqplaybackparser.h \
    session/cookiepolicy.h \
    session/qqcookiejar.h \
    session/secretidentifierfactory.h \
    session/sessionstore.h \
    storage/settingsstore.h \
    storage/imagecache.h \
    models/playlistlistmodel.h \
    models/tracklistmodel.h \
    models/lyriclistmodel.h

OTHER_FILES += \
    ../qml/internal/SecretsStore.qml \
    ../qml/cover/AppCover.qml \
    ../qml/pages/DiscoverPage.qml \
    ../qml/pages/HomePage.qml \
    ../qml/pages/TopChartsPage.qml

TRANSLATIONS += ../translations/harbour-qqmusic-zh_CN.ts
