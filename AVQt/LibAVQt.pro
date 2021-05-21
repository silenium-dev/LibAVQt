# Created by and for Qt Creator This file was created for editing the project sources only.
# You may attempt to use it for building too, by modifying this file here.

#TARGET = LibAVQt

TEMPLATE = lib

CONFIG += staticlib

QT = core gui widgets opengl concurrent
CONFIG += c++latest

HEADERS = \
   $$PWD/filter/private/AudioDecoder_p.h \
   $$PWD/filter/private/DecoderDXVA2_p.h \
   $$PWD/filter/private/DecoderVAAPI_p.h \
   $$PWD/filter/private/EncoderVAAPI_p.h \
   $$PWD/filter/AudioDecoder.h \
   $$PWD/filter/DecoderDXVA2.h \
   $$PWD/filter/DecoderVAAPI.h \
   $$PWD/filter/EncoderVAAPI.h \
   $$PWD/filter/IDecoder.h \
   $$PWD/input/private/Demuxer_p.h \
   $$PWD/input/Demuxer.h \
   $$PWD/input/IAudioSource.h \
   $$PWD/input/IFrameSource.h \
   $$PWD/input/IPacketSource.h \
   $$PWD/output/private/OpenALAudioOutput_p.h \
   $$PWD/output/private/OpenALErrorHandler.h \
   $$PWD/output/private/OpenGLRenderer_p.h \
   $$PWD/output/private/RenderClock_p.h \
   $$PWD/output/IAudioSink.h \
   $$PWD/output/IFrameSink.h \
   $$PWD/output/IPacketSink.h \
   $$PWD/output/OpenALAudioOutput.h \
   $$PWD/output/OpenGLRenderer.h \
   $$PWD/output/RenderClock.h \
   $$PWD/AVQt

SOURCES = \
   $$PWD/filter/AudioDecoder.cpp \
   $$PWD/filter/DecoderDXVA2.cpp \
   $$PWD/filter/DecoderVAAPI.cpp \
   $$PWD/filter/EncoderVAAPI.cpp \
   $$PWD/input/Demuxer.cpp \
   $$PWD/output/OpenALAudioOutput.cpp \
   $$PWD/output/OpenGLRenderer.cpp \
   $$PWD/output/RenderClock.cpp

INCLUDEPATH = \
    $$PWD/filter \
    $$PWD/filter/private \
    $$PWD/input \
    $$PWD/input/private \
    $$PWD/output \
    $$PWD/output/private

RESOURCES += $$PWD/resources.qrc

LIBS += -lOpenAL32 -lopengl32 -lavformat -lavfilter -lavutil -lavcodec -lavdevice -lswscale -lswresample -lbluray -lxml2 -lharfbuzz -lcairo -lgobject-2.0 -lfontconfig -lfreetype -lusp10 -lmsimg32 -lpixman-1 -lffi -lexpat -lbz2 -lpng16 -lharfbuzz_too -lfreetype_too -lglib-2.0 -lwinmm -lshlwapi -lpcre -lintl -lgdi32 -lgnutls -lhogweed -lnettle -ltasn1 -lidn2 -latomic -lcrypt32 -lgmp -lunistring -lcharset -lws2_32 -ldl -lavcodec -lvpx -lpthread -liconv -llzma -lopencore-amrwb -lz -lmp3lame -lopencore-amrnb -lopus -lspeex -ltheoraenc -ltheoradec -lvo-amrwbenc -lvorbisenc -lvorbis -logg -lx264 /data/Silas/MXE/usr/x86_64-w64-mingw32.static/lib/xvidcore.a -lole32 -lswresample  -lavutil -lm -luser32 -lbcrypt

#DEFINES = 

