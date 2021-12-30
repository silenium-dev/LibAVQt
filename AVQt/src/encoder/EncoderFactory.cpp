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

//
// Created by silas on 28.12.21.
//

#include "encoder/EncoderFactory.hpp"
#include "encoder/VAAPIEncoderImpl.hpp"

#include <QtDebug>

namespace AVQt {
    EncoderFactory &EncoderFactory::getInstance() {
        static EncoderFactory instance;
        return instance;
    }

    void EncoderFactory::registerEncoder(const QString &name, const QMetaObject &metaObject) {
        if (m_encoders.contains(name)) {
            qWarning() << "EncoderFactory: Encoder with name" << name << "already registered";
        }
        m_encoders.insert(name, metaObject);
    }

    void EncoderFactory::unregisterEncoder(const QString &name) {
        if (!m_encoders.contains(name)) {
            qWarning() << "EncoderFactory: Encoder with name" << name << "not registered";
        }
        m_encoders.remove(name);
    }

    api::IEncoderImpl *EncoderFactory::create(const QString &name, const EncodeParameters &params) {
        if (!m_encoders.contains(name)) {
            qWarning() << "EncoderFactory: Encoder with name" << name << "not registered";
            return nullptr;
        }
        return qobject_cast<api::IEncoderImpl *>(m_encoders[name].newInstance(Q_ARG(AVQt::EncodeParameters, params)));
    }

    void EncoderFactory::registerEncoders() {
        static std::atomic_bool registered{false};
        bool shouldBe = false;
        if (registered.compare_exchange_strong(shouldBe, true)) {
            getInstance().registerEncoder("VAAPI", VAAPIEncoderImpl::staticMetaObject);
        }
    }
}// namespace AVQt
