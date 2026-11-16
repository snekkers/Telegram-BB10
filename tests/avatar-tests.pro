TEMPLATE = app
TARGET = avatar-tests
QT = core gui testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
INCLUDEPATH += ../src
SOURCES += avatar-tests.cpp ../src/AvatarRenderer.cpp
HEADERS += ../src/AvatarRenderer.hpp

SOURCES += ../src/EmojiText.cpp
HEADERS += ../src/EmojiText.hpp

SOURCES += ../src/PhotoCache.cpp
HEADERS += ../src/PhotoCache.hpp
