TEMPLATE = app
APP_NAME = berrygram
TARGET = berrygram

CONFIG += qt warn_on cascades10
CONFIG -= debug_and_release
QT += core declarative

INCLUDEPATH += $$PWD/src
SOURCES += $$PWD/src/main.cpp \
           $$PWD/src/ChatController.cpp \
           $$PWD/src/TelegramSession.cpp \
           $$PWD/src/TelegramStore.cpp \
           $$PWD/src/TelegramController.cpp \
           $$PWD/src/JsonCodec.cpp
HEADERS += $$PWD/src/ChatController.hpp \
           $$PWD/src/TelegramSession.hpp \
           $$PWD/src/TelegramStore.hpp \
           $$PWD/src/TelegramController.hpp \
           $$PWD/src/JsonCodec.hpp
LIBS += -lbbdata
OTHER_FILES += $$PWD/bar-descriptor.xml \
               $$PWD/assets/main.qml \
               $$PWD/assets/ChatPage.qml \
               $$PWD/assets/LoginPage.qml

# The Cascades SDK chooses architecture-specific output directories for debug builds.
# Build scripts stage the finished executable into bin/ for BAR packaging.
