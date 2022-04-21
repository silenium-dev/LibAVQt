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

//
// Created by silas on 23.10.21.
//

#ifndef LIBAVQT_MESSAGE_H
#define LIBAVQT_MESSAGE_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>
#include <pgraph/api/Data.hpp>


namespace AVQt::communication {
    class MessageBuilder;

    class Message : public pgraph::api::Data {
        Q_DISABLE_COPY(Message)
    public:
        class Action {
        public:
            enum Enum {
                INIT,
                CLEANUP,
                START,
                STOP,
                PAUSE,
                RESIZE,
                RESET,
                DATA,
                NONE
            };
            QString name();

            Action &operator=(Enum &type);

            operator Enum();// NOLINT(google-explicit-constructor) // Is required

            friend bool operator==(const Action &lhs, const Enum &rhs);
            friend bool operator==(const Enum &lhs, const Action &rhs);
            friend bool operator==(const Action &lhs, const Action &rhs);

            explicit Action(const Enum &type);
            Action();

        private:
            Enum m_action;
        };

        Message(Action type, QVariantMap payload);
        Message(Message &&c) noexcept;
        ~Message() override = default;

        static MessageBuilder builder();

        QVariant getPayload(const QString &key);
        QVariantMap getPayloads();
        Action getAction();

        QUuid getType() override;

        static const QUuid Type;

    private:
        Action m_type;
        QVariantMap m_payload;

        friend class MessageBuilder;
    };

    class MessageBuilder {
    public:
        MessageBuilder &withAction(Message::Action::Enum type);
        template<typename T, typename... Ts>
        [[maybe_unused]] MessageBuilder &withPayload(QPair<QString, T> p, QPair<QString, Ts>... pl);
        [[maybe_unused]] MessageBuilder &withPayload(const QVariantMap &pl);
        MessageBuilder &withPayload(const QString &key, const QVariant &p);

        std::shared_ptr<Message> build();

    private:
        MessageBuilder();

        Message::Action m_action;
        QVariantMap m_payload;

        friend class Message;
    };
}// namespace AVQt

#endif//LIBAVQT_MESSAGE_H
