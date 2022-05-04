// Copyright (c) 2021.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "AVQt/input/IFrameSource.h"
#include "output/IPacketSink.h"

#ifndef LIBAVQT_DECODER_H
#define LIBAVQT_DECODER_H

namespace AVQt {
    class IDecoder : public IFrameSource, public IPacketSink {


        // IPacketSink interface
    public:
        ~IDecoder() Q_DECL_OVERRIDE = default;

        bool isPaused() Q_DECL_OVERRIDE = 0;

        void init(IPacketSource *source, AVRational framerate, AVRational timebase, int64_t duration, AVCodecParameters *vParams,
                  AVCodecParameters *aParams, AVCodecParameters *sParams) Q_DECL_OVERRIDE = 0;

        void deinit(IPacketSource *source) Q_DECL_OVERRIDE = 0;

        void start(IPacketSource *source) Q_DECL_OVERRIDE = 0;

        void stop(IPacketSource *source) Q_DECL_OVERRIDE = 0;

        void pause(bool paused) Q_DECL_OVERRIDE = 0;

        void onPacket(IPacketSource *source, AVPacket *packet, int8_t packetType) Q_DECL_OVERRIDE = 0;

    signals:

        void started() Q_DECL_OVERRIDE = 0;

        void stopped() Q_DECL_OVERRIDE = 0;

        // IFrameSource interface
    public:
        qint64 registerCallback(IFrameSink *frameSink) Q_DECL_OVERRIDE = 0;

        qint64 unregisterCallback(IFrameSink *frameSink) Q_DECL_OVERRIDE = 0;

        int init() Q_DECL_OVERRIDE = 0;

        int deinit() Q_DECL_OVERRIDE = 0;

        int start() Q_DECL_OVERRIDE = 0;

        int stop() Q_DECL_OVERRIDE = 0;

    signals:

        void paused(bool pause) Q_DECL_OVERRIDE = 0;
    };
}

Q_DECLARE_INTERFACE(AVQt::IDecoder, "AVQt::IDecoder")

#endif //LIBAVQT_DECODER_H
