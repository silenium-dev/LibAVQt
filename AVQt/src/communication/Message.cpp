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

#include "communication/Message.hpp"

#include <boost/uuid/string_generator.hpp>
#include <utility>

namespace AVQt {
    const boost::uuids::uuid Message::Type = boost::uuids::string_generator()("{1db2f16c89f3de68086c5124c5272bab}");

    Message::Message(Message::Action type, QVariantMap payload) : m_type(type), m_payload(std::move(payload)) {
    }

    Message::Message(Message &&c) noexcept : m_type(c.m_type), m_payload(std::move(c.m_payload)) {
    }

    QVariantMap Message::getPayloads() {
        return m_payload;
    }

    QVariant Message::getPayload(const QString &key) {
        if (m_payload.contains(key)) {
            return m_payload[key];
        }
        return {};
    }

    Message::Action Message::getAction() {
        return m_type;
    }

    boost::uuids::uuid Message::getType() {
        return Type;
    }

    MessageBuilder Message::builder() {
        return {};
    }

    MessageBuilder::MessageBuilder() = default;

    MessageBuilder &MessageBuilder::withAction(Message::Action::Enum type) {
        m_action = type;
        return *this;
    }
    template<typename T, typename... Ts>
    MessageBuilder &MessageBuilder::withPayload(QPair<QString, T> p, QPair<QString, Ts>... pl) {
        m_payload.insert(p.first, p.second);
        m_payload.insert({{pl.first, pl.second}...});
        return *this;
    }
    MessageBuilder &MessageBuilder::withPayload(const QString &key, const QVariant &p) {
        m_payload.insert(key, p);
        return *this;
    }
    MessageBuilder &MessageBuilder::withPayload(const QVariantMap &pl) {
        m_payload.insert(pl);
        return *this;
    }
    std::shared_ptr<Message> MessageBuilder::build() {
        return std::make_shared<Message>(m_action, m_payload);
    }

    QString Message::Action::name() {
        switch (m_action) {
            case INIT:
                return "INIT";
            case CLEANUP:
                return "CLEANUP";
            case START:
                return "START";
            case STOP:
                return "STOP";
            case PAUSE:
                return "PAUSE";
            case DATA:
                return "DATA";
            case NONE:
                return "NONE";
            default:
                return "INVALID";
        }
    }

    Message::Action::Action(const Message::Action::Enum &type) : m_action(type) {
    }

    Message::Action::Action() : m_action(NONE) {
    }

    Message::Action &Message::Action::operator=(Message::Action::Enum &type) {
        m_action = type;
        return *this;
    }

    bool operator==(const AVQt::Message::Action &lhs, const AVQt::Message::Action &rhs) {
        return lhs.m_action == rhs.m_action;
    }

    bool operator==(const AVQt::Message::Action &lhs, const AVQt::Message::Action::Enum &rhs) {
        return lhs.m_action == rhs;
    }

    bool operator==(const AVQt::Message::Action::Enum &lhs, const AVQt::Message::Action &rhs) {
        return lhs == rhs.m_action;
    }

    Message::Action::operator Enum() {
        return m_action;
    }
}// namespace AVQt
