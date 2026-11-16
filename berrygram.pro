TEMPLATE = app
APP_NAME = berrygram
TARGET = berrygram

CONFIG += qt warn_on cascades10
CONFIG -= debug_and_release
QT += core declarative

INCLUDEPATH += $$PWD/src
SOURCES += $$PWD/src/main.cpp \
           $$PWD/src/ChatController.cpp
HEADERS += $$PWD/src/ChatController.hpp
OTHER_FILES += $$PWD/bar-descriptor.xml \
               $$PWD/assets/main.qml \
               $$PWD/assets/ChatPage.qml

# Keep packaging paths independent of Momentics-generated makefiles.
DESTDIR = $$OUT_PWD/bin
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR = $$OUT_PWD/moc
