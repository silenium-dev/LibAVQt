// Copyright (c) 2021-2022.
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

#include <qglobal.h>

#include <AVQt/communication/IComponent.hpp>
#include <AVQt/communication/Message.hpp>
#include <AVQt/communication/PacketPadParams.hpp>
#include <AVQt/communication/VideoPadParams.hpp>

#include <AVQt/input/Demuxer.hpp>

#include "decoder/VideoDecoder.hpp"
#include "encoder/VideoEncoder.hpp"
#include <AVQt/decoder/IVideoDecoderImpl.hpp>
#include <AVQt/decoder/VideoDecoderFactory.hpp>
#include <AVQt/encoder/IVideoEncoderImpl.hpp>
#include <AVQt/encoder/VideoEncoderFactory.hpp>

#include <AVQt/filter/VaapiYuvToRgbMapper.hpp>

#include <AVQt/renderers/IOpenGLFrameMapper.hpp>
#include <AVQt/renderers/OpenGLFrameMapperFactory.hpp>

#include <AVQt/capture/DesktopCaptureFactory.hpp>
#include <AVQt/capture/DesktopCapturer.hpp>
#include <AVQt/capture/IDesktopCaptureImpl.hpp>

#ifdef QT_DEBUG
#include <AVQt/debug/CommandConsumer.hpp>
#endif
