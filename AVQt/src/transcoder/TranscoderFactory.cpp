// Copyright (c) 2022.
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
// Created by silas on 13.01.22.
//

#include "transcoder/TranscoderFactory.hpp"
#include "VaapiTranscoderImpl.hpp"

namespace AVQt {
    TranscoderFactory::TranscoderFactory() = default;

    TranscoderFactory &TranscoderFactory::getInstance() {
        static TranscoderFactory instance;
        return instance;
    }

    bool TranscoderFactory::registerTranscoder(const QString &name, QMetaObject implMeta) {
        if (name.isEmpty() || m_transcoders.contains(name)) {
            return false;
        }
        m_transcoders.insert(name, implMeta);
        return true;
    }

    bool TranscoderFactory::unregisterTranscoder(const QString &name) {
        if (name.isEmpty() || !m_transcoders.contains(name)) {
            return false;
        }
        m_transcoders.remove(name);
        return true;
    }

    api::ITranscoderImpl *TranscoderFactory::createTranscoder(const QString &transcoderName, EncodeParameters params) {
        if (transcoderName.isEmpty() || !m_transcoders.contains(transcoderName)) {
            return nullptr;
        }
        auto metaObj = m_transcoders.value(transcoderName);
        auto instance = dynamic_cast<api::ITranscoderImpl *>(metaObj.newInstance(Q_ARG(EncodeParameters, params)));
        return instance;
    }

    void TranscoderFactory::registerTranscoders() {
        bool shouldBe = false;
        if (m_initialized.compare_exchange_strong(shouldBe, true)) {
            registerTranscoder("VAAPI", VAAPITranscoderImpl::staticMetaObject);
        }
    }
}// namespace AVQt
