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
// Created by silas on 23.11.21.
//

#ifndef LIBAVQT_FILEINPUT_P_HPP
#define LIBAVQT_FILEINPUT_P_HPP

#include "../FileInput.hpp"
#include <QtGlobal>

namespace AVQt {
    class FileInputPrivate {
        Q_DECLARE_PUBLIC(AVQt::FileInput)
    private:
        FileInputPrivate(FileInput *q) : q_ptr(q) {}
        FileInput *q_ptr;

        uint32_t m_outputPadId{0};
        QString filename{};
        std::unique_ptr<QFile> inputFile{};
        std::atomic_bool m_running{false}, m_paused{false};
        std::atomic_size_t m_loopCount{0};

        QFutureInterface<void> m_sendStartIf{};
        QFuture<void> m_sendStart{};

        static constexpr size_t START_DATA_SIZE = 256;
    };
}// namespace AVQt


#endif//LIBAVQT_FILEINPUT_P_HPP
