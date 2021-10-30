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
// Created by silas on 23.10.21.
//

#ifndef LIBAVQT_MESSAGE_H
#define LIBAVQT_MESSAGE_H

#include <ProcessingGraph/Pad.h>
#include <QtCore>


namespace AVQt {
    class MessageBuilder;

    class Message {
    public:
        class Type {
        public:
            enum Enum {
                INIT,
                CLEANUP,
                START,
                STOP,
                PAUSE,
                DATA,
                NONE
            };
            QString name();

            Type &operator=(Enum &type);

            explicit Type(const Enum &type);
            Type();

        private:
            Enum m_type;
        };

        explicit Message(QObject *parent = nullptr) { // Constructor is required by Qt's metatype system, it won't be called
            qFatal("Error");
        };

        Message(Message &&c) noexcept;
        Message(const Message &c);

        static MessageBuilder builder();

        QVariant getPayload(const QString &key);
        QVariantMap getPayloads();
        Type getType();

    private:
        Message(Type type, QVariantMap payload);

        Type m_type;
        QVariantMap m_payload;

        friend class MessageBuilder;
    };

    class MessageBuilder {
    public:
        MessageBuilder &withType(Message::Type::Enum type);
        template<typename T, typename... Ts>
        MessageBuilder &withPayload(QPair<QString, T> p, QPair<QString, Ts>... pl);
        MessageBuilder &withPayload(const QVariantMap &pl);
        MessageBuilder &withPayload(const QString &key, const QVariant &p);

        Message build();

    private:
        MessageBuilder();

        Message::Type m_type;
        QVariantMap m_payload;

        friend class Message;
    };

    using MessagePad = ProcessingGraph::Pad<Message>;
}// namespace AVQt

PG_DECLARE_DATATYPE(AVQt::Message)

#endif//LIBAVQT_MESSAGE_H
