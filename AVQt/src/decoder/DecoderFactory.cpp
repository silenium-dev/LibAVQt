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
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BELIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORTOR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OROTHER DEALINGS IN THE SOFTWARE.

//
// Created by silas on 12.12.21.
//

#include "include/AVQt/decoder/DecoderFactory.hpp"
#include "decoder/VAAPIDecoderImpl.hpp"

#include <QDebug>

void AVQt::DecoderFactory::registerDecoder(const QString &name, const QMetaObject& metaObject) {
    if (m_decoders.contains(name)) {
        qWarning() << "Decoder with name" << name << "already registered";
        return;
    }
    m_decoders.insert(name, metaObject);
}
void AVQt::DecoderFactory::unregisterDecoder(const QString &name) {
    if (!m_decoders.contains(name)) {
        qWarning() << "Decoder with name" << name << "not registered";
        return;
    }
    m_decoders.remove(name);
}

AVQt::DecoderFactory &AVQt::DecoderFactory::getInstance() {
    static DecoderFactory instance;
    return instance;
}

AVQt::api::IDecoderImpl *AVQt::DecoderFactory::create(const QString &name) {
    if (!m_decoders.contains(name)) {
        qWarning() << "Decoder with name" << name << "not registered";
        return nullptr;
    }
    return qobject_cast<api::IDecoderImpl*>(m_decoders[name].newInstance());
}

void AVQt::DecoderFactory::registerDecoder() {
    static bool registered = false;
    if (!registered) {
        registered = true;
        AVQt::DecoderFactory::getInstance().registerDecoder("VAAPI", VAAPIDecoderImpl::staticMetaObject);
    }
}
