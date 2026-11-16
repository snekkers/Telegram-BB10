TEMPLATE = app
APP_NAME = berrygram
TARGET = berrygram

CONFIG += qt warn_on cascades10
CONFIG -= debug_and_release
CONFIG(debug, debug|release): DEFINES += BERRYGRAM_DEVELOPMENT_TEST_API
CONFIG(debug, debug|release): DEFINES += BERRYGRAM_UI_CHECKS
QT += core declarative gui

INCLUDEPATH += $$PWD/src
SOURCES += $$PWD/src/main.cpp \
           $$PWD/src/ChatController.cpp \
           $$PWD/src/TelegramSession.cpp \
           $$PWD/src/TelegramStore.cpp \
           $$PWD/src/TelegramController.cpp \
           $$PWD/src/JsonCodec.cpp
SOURCES += $$PWD/src/PrivateFile.cpp
HEADERS += $$PWD/src/ChatController.hpp \
           $$PWD/src/TelegramSession.hpp \
           $$PWD/src/TelegramStore.hpp \
           $$PWD/src/TelegramController.hpp \
           $$PWD/src/JsonCodec.hpp
HEADERS += $$PWD/src/PrivateFile.hpp
HEADERS += $$PWD/src/MessagePresentation.hpp
SOURCES += $$PWD/src/UiTools.cpp
HEADERS += $$PWD/src/UiTools.hpp
LIBS += -lscreen
SOURCES += $$PWD/src/TelegramSearch.cpp $$PWD/src/SearchController.cpp
HEADERS += $$PWD/src/TelegramSearch.hpp $$PWD/src/SearchController.hpp
LIBS += -lbbdata -lbbsystem -lbbcascadespickers -lbbmultimedia
OTHER_FILES += $$PWD/bar-descriptor.xml \
               $$PWD/assets/main.qml \
               $$PWD/assets/ChatPage.qml \
               $$PWD/assets/LoginPage.qml
OTHER_FILES += $$files($$PWD/assets/*.qml) $$PWD/assets/Ui.js

# The Cascades SDK chooses architecture-specific output directories for debug builds.
# Build scripts stage the finished executable into bin/ for BAR packaging.

SOURCES += $$PWD/src/LocalOutbox.cpp
HEADERS += $$PWD/src/LocalOutbox.hpp

SOURCES += $$PWD/src/AutoPager.cpp
HEADERS += $$PWD/src/AutoPager.hpp
SOURCES += $$PWD/src/TelegramStoreActions.cpp
SOURCES += $$PWD/src/DemoActions.cpp
SOURCES += $$PWD/src/TelegramPeople.cpp
HEADERS += $$PWD/src/Presence.hpp
SOURCES += $$PWD/src/AvatarImages.cpp
HEADERS += $$PWD/src/AvatarImages.hpp

SOURCES += $$PWD/src/AvatarRenderer.cpp
HEADERS += $$PWD/src/AvatarRenderer.hpp
SOURCES += $$PWD/src/DemoPeople.cpp

SOURCES += $$PWD/src/EmojiText.cpp
HEADERS += $$PWD/src/EmojiText.hpp

SOURCES += $$PWD/src/EmojiView.cpp
HEADERS += $$PWD/src/EmojiView.hpp

SOURCES += $$PWD/src/NotificationCenter.cpp
HEADERS += $$PWD/src/NotificationCenter.hpp $$PWD/src/NotificationPolicy.hpp
LIBS += -lbbplatform

SOURCES += $$PWD/src/MediaController.cpp $$PWD/src/PhotoCache.cpp
HEADERS += $$PWD/src/MediaController.hpp $$PWD/src/PhotoCache.hpp

SOURCES += $$PWD/src/EmojiCatalog.cpp
HEADERS += $$PWD/src/EmojiCatalog.hpp

SOURCES += $$PWD/src/Preferences.cpp
HEADERS += $$PWD/src/Preferences.hpp

HEADERS += $$PWD/src/FormattedText.hpp

SOURCES += $$PWD/src/TelegramReactions.cpp

SOURCES += $$PWD/src/TelegramTools.cpp
QT += network
SOURCES += $$PWD/src/VoiceDecoder.cpp $$PWD/src/VoicePlayer.cpp
HEADERS += $$PWD/src/VoiceDecoder.hpp $$PWD/src/VoicePlayer.hpp
device: VOICE_PREFIX = $$PWD/build/device/voice-codecs
simulator: VOICE_PREFIX = $$PWD/build/simulator/voice-codecs
INCLUDEPATH += $$VOICE_PREFIX/include $$VOICE_PREFIX/include/opus
LIBS += -L$$VOICE_PREFIX/lib -lopusfile -lopus -logg -lm

SOURCES += $$PWD/src/TelegramThreads.cpp
