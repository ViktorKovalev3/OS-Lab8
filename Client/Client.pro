TEMPLATE = app
CONFIG += console c
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c
QMAKE_CXXFLAGS += -pthread -std=c
LIBS += -pthread -lrt
