TEMPLATE = app
TARGET = DraganflyerAPIExample
target.path = $$PREFIX/bin
DESTDIR = bin/
INSTALLS += target
QT += network

SOURCES += main.cpp \
    com/remotecontroller.cpp \
    com/serial/qextserialport.cpp \
    com/vehicle.cpp \
    gui/configwidget.cpp \
    gui/controlwidget.cpp \
    gui/monitorwidget.cpp \
    gui/remoteclient.cpp \
    gui/telemetrywidget.cpp \
    joystick/joystick.cpp

HEADERS += \
    com/remotecontroller.h \
    com/serial/qextserialenumerator.h \
    com/serial/qextserialport.h \
    com/serial/qextserialport_global.h \
    com/vehicle.h \
    gui/configwidget.h \
    gui/controlwidget.h \
    gui/monitorwidget.h \
    gui/remoteclient.h \
    gui/telemetrywidget.h \
    joystick/joystick.h

LIBS += -lSDL

macx:LIBS += -framework IOKit -framework CoreFoundation
macx:SOURCES += com/serial/qextserialenumerator_osx.cpp

unix:DEFINES += _TTY_POSIX_
unix:SOURCES += com/serial/posix_qextserialport.cpp
unix:!macx:SOURCES += com/serial/qextserialenumerator_unix.cpp

win32:DEFINES          += WINVER=0x0501
win32:DESTWIN           = $${DESTDIR}
win32:DESTWIN          ~= s,/,\\,g
win32:INCLUDEPATH      += C:/QT/$$[QT_VERSION]/src
win32:INCLUDEPATH      += C:/SDL/SDL-1.2.15/include
win32:LIBS             += -lsetupapi -L C:/SDL/SDL-1.2.15/lib -lSDL
win32:SDLDLL            = C:/SDL/SDL-1.2.15/bin/SDL.dll
win32:SDLDLL           ~= s,/,\\,g
win32:SOURCES          += com/serial/win_qextserialport.cpp com/serial/qextserialenumerator_win.cpp
win32:QMAKE_PRE_LINK   += $$quote(cmd /C copy /V /Y $${SDLDLL} /B $${DESTWIN}$$escape_expand(\\n\\t))

QMAKE_CXXFLAGS += -pedantic -Werror -Wextra -Wno-long-long
