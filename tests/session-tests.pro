TEMPLATE = app
TARGET = session-tests
QT = core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
INCLUDEPATH += ../src
SOURCES += session-tests.cpp ../src/TelegramSession.cpp ../src/JsonCodec.cpp
HEADERS += ../src/TelegramSession.hpp ../src/JsonCodec.hpp
