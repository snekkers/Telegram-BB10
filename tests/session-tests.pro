TEMPLATE = app
TARGET = session-tests
QT = core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
!contains(CONFIG, release_api_check): DEFINES += BERRYGRAM_DEVELOPMENT_TEST_API
INCLUDEPATH += ../src
SOURCES += session-tests.cpp ../src/TelegramSession.cpp ../src/JsonCodec.cpp ../src/TelegramStore.cpp
HEADERS += ../src/TelegramSession.hpp ../src/JsonCodec.hpp ../src/TelegramStore.hpp
SOURCES += ../src/PrivateFile.cpp
HEADERS += ../src/PrivateFile.hpp

SOURCES += ../src/LocalOutbox.cpp
HEADERS += ../src/LocalOutbox.hpp

SOURCES += ../src/TelegramStoreActions.cpp
SOURCES += ../src/TelegramPeople.cpp
SOURCES += ../src/TelegramReactions.cpp

SOURCES += ../src/TelegramTools.cpp
