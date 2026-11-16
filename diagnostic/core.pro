TEMPLATE = app
TARGET = berrygram-diagnostic-core
QT = core
CONFIG += console c++11
CONFIG -= app_bundle
INCLUDEPATH += ../src
SOURCES += core.cpp ../src/TelegramSession.cpp ../src/TelegramStore.cpp ../src/JsonCodec.cpp ../src/PrivateFile.cpp
HEADERS += ../src/TelegramSession.hpp ../src/TelegramStore.hpp ../src/JsonCodec.hpp ../src/PrivateFile.hpp

SOURCES += ../src/LocalOutbox.cpp
HEADERS += ../src/LocalOutbox.hpp

SOURCES += ../src/TelegramStoreActions.cpp
