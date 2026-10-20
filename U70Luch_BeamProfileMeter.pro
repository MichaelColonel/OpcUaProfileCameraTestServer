TEMPLATE = app
CONFIG += c++11 console
QT += opcua opcua-private
QT -= gui

# Installed example package
MODULE_SOURCES=/home/michel/devel/git/qtopcua/
!exists($$MODULE_SOURCES):{
  # Build from source tree
  MODULE_SOURCES=$$PWD/../../../../
}

INCLUDEPATH += $$MODULE_SOURCES/src/plugins/opcua/open62541
DEPENDPATH += INCLUDEPATH

qtConfig(open62541):!qtConfig(system-open62541) {
    include($$MODULE_SOURCES/src/3rdparty/open62541.pri)
} else {
    QMAKE_USE_PRIVATE += open62541
}

SOURCES += main.cpp \
    U70Luch_BeamProfileMeter.cpp \
    $$MODULE_SOURCES/src/plugins/opcua/open62541/qopen62541utils.cpp \
    $$MODULE_SOURCES/src/plugins/opcua/open62541/qopen62541valueconverter.cpp

HEADERS += \
    U70Luch_BeamProfileMeter.h

#install
target.path = $$[QT_INSTALL_EXAMPLES]/opcua/u70luch_beamprofilemeter/server
INSTALLS += target
