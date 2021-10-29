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

#include "Command.h"

#include <utility>

namespace AVQt {
    Command::Command(Command::Type type, QVariantMap payload) : m_type(type), m_payload(std::move(payload)) {
    }

    Command::Command(Command &&c) noexcept : m_type(c.m_type), m_payload(std::move(c.m_payload)) {
    }

    Command::Command(const Command &c) : m_type(c.m_type) {
        m_payload.insert(c.m_payload);
    }
    QVariantMap Command::getPayloads() {
        return m_payload;
    }

    QVariant Command::getPayload(const QString &key) {
        if (m_payload.contains(key)) {
            return m_payload[key];
        }
        return {};
    }

    Command::Type Command::getType() {
        return m_type;
    }
    CommandBuilder Command::builder() {
        return {};
    }

    CommandBuilder::CommandBuilder() = default;

    CommandBuilder &CommandBuilder::withType(Command::Type::Enum type) {
        m_type = type;
        return *this;
    }
    template<typename T, typename... Ts>
    CommandBuilder &CommandBuilder::withPayload(QPair<QString, T> p, QPair<QString, Ts>... pl) {
        m_payload.insert(p.first, p.second);
        m_payload.insert({{pl.first, pl.second}...});
        QMap<int, int> map{{0, 1}, {0, 1}};
        map.first();
        return *this;
    }
    CommandBuilder &CommandBuilder::withPayload(const QString &key, const QVariant &p) {
        m_payload.insert(key, p);
        return *this;
    }
    CommandBuilder &CommandBuilder::withPayload(const QVariantMap &pl) {
        m_payload.insert(pl);
        return *this;
    }
    Command CommandBuilder::build() {
        return {m_type, m_payload};
    }

    QString Command::Type::name() {
        switch (m_type) {
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
            case NONE:
                return "NONE";
            default:
                return "INVALID";
        }
    }

    Command::Type::Type(const Command::Type::Enum &type) : m_type(type) {
    }

    Command::Type::Type() : m_type(NONE) {
    }

    Command::Type &Command::Type::operator=(Command::Type::Enum &type) {
        m_type = type;
        return *this;
    }
}// namespace AVQt
