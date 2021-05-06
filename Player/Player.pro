TEMPLATE = app

QT += core gui widgets opengl concurrent

SOURCES += $$PWD/main.cpp

INCLUDEPATH += ../AVQt



win32 {
    LIBS += -L$$OUT_PWD/../AVQt/
    LIBS += -lOpenAL32 -lopengl32 -lbluray -lxml2 -lharfbuzz -lcairo -lgobject-2.0 -lfontconfig -lfreetype -lusp10 -lmsimg32 -lpixman-1 -lffi -lexpat -lbz2 -lpng16 -lharfbuzz_too -lfreetype_too -lglib-2.0 -lwinmm -lshlwapi -lpcre -lintl -lgdi32 -lgnutls -lhogweed -lnettle -ltasn1 -lidn2 -latomic -lcrypt32 -lgmp -lunistring -lcharset -lws2_32 -ldl -lavcodec -lvpx -lpthread -liconv -llzma -lopencore-amrwb -lz -lmp3lame -lopencore-amrnb -lopus -lspeex -ltheoraenc -ltheoradec -lvo-amrwbenc -lvorbisenc -lvorbis -logg -lx264 /data/Silas/MXE/usr/x86_64-w64-mingw32.static/lib/xvidcore.a -lole32 -lavutil -lm -luser32 -lbcrypt
    TARGET = ../Player
}

linux {
    LIBS += -L$$OUT_PWD/../
    LIBS += -lopenal -lGL
}

LIBS += -lAVQt -lavformat -lavfilter -lavutil -lavcodec -lavdevice -lswscale -lswresample
