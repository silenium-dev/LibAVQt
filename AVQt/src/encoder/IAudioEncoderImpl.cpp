#include "encoder/IAudioEncoderImpl.hpp"

namespace AVQt::api {
    IAudioEncoderImpl::IAudioEncoderImpl(const AudioEncodeParameters &parameters)
        : m_encodeParameters(parameters) {
    }

    const AudioEncodeParameters &IAudioEncoderImpl::getEncodeParameters() const {
        return m_encodeParameters;
    }
}// namespace AVQt::api
