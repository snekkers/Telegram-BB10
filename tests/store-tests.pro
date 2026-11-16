TEMPLATE = app
TARGET = store-tests
QT = core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
INCLUDEPATH += ../src
SOURCES += store-tests.cpp ../src/TelegramSession.cpp ../src/TelegramStore.cpp ../src/JsonCodec.cpp
HEADERS += ../src/TelegramSession.hpp ../src/TelegramStore.hpp ../src/JsonCodec.hpp
SOURCES += ../src/PrivateFile.cpp
HEADERS += ../src/PrivateFile.hpp

SOURCES += ../src/LocalOutbox.cpp
HEADERS += ../src/LocalOutbox.hpp
