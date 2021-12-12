
// Copyright (c) 2021-2021.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and t
//  permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BELIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORTOR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OROTHER DEALINGS IN THE SOFTWARE.

#ifndef LIBAVQT_GLOBAL_H
#define LIBAVQT_GLOBAL_H

#include <qglobal.h>
#include <QMetaType>
#include <pgraph/impl/SimpleProducer.hpp>
#include <pgraph/impl/SimpleConsumer.hpp>

#ifdef AVQT_LIBRARY_BUILD
#define AVQT_DEPRECATED
#else
#define AVQT_DEPRECATED Q_DECL_DEPRECATED
#endif

Q_DECLARE_METATYPE(pgraph::impl::SimpleProducer *)
Q_DECLARE_METATYPE(pgraph::impl::SimpleConsumer *)
Q_DECLARE_METATYPE(AVCodecParameters *)
Q_DECLARE_METATYPE(AVPacket *)

#endif//LIBAVQT_GLOBAL_H
