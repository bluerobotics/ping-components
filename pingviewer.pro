TARGET = pingviewer

CONFIG += \
    c++14 \
    static

QT += core charts gui qml quick widgets quickcontrols2 concurrent svg xml

HEADERS += \
    src/waterfallgradient.h \
    src/waterfall.h \
    src/logger.h \
    src/util.h

SOURCES += \
    src/waterfallgradient.cpp \
    src/waterfall.cpp \
    src/logger.cpp \
    src/util.cpp \
    src/main.cpp

RESOURCES += \
    resources.qrc

*-g++ {
    QMAKE_CXXFLAGS += -fopenmp
    QMAKE_LFLAGS += -fopenmp
}

include(lib/ping-protocol-cpp/ping.pri)

win32 {
    CONFIG(debug,debug|release) {
        # Terminal output
        CONFIG += console
    }
}

# https://git-scm.com/docs/git-log placeholders
# Get git info
exists ($$_PRO_FILE_PWD_/.git) {
    GIT_VERSION = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$PWD log -1 --format=%h)
    GIT_VERSION_DATE = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$PWD log -1 --format=%aI)
    GIT_TAG = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$PWD describe --abbrev=0)
    GIT_URL = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$PWD remote get-url origin)
    DEFINES += 'GIT_VERSION=\\"$$GIT_VERSION\\"'
    DEFINES += 'GIT_VERSION_DATE=\\"$$GIT_VERSION_DATE\\"'
    DEFINES += 'GIT_TAG=\\"$$GIT_TAG\\"'
    DEFINES += 'GIT_URL=\\"$$GIT_URL\\"'
} else {
    DEFINES += 'GIT_VERSION=\\"-\\"'
    DEFINES += 'GIT_VERSION_DATE=\\"-\\"'
    DEFINES += 'GIT_TAG=\\"-\\"'
    DEFINES += 'GIT_URL=\\"-\\"'
}
