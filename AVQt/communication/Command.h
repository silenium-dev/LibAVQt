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

#ifndef LIBAVQT_COMMAND_H
#define LIBAVQT_COMMAND_H

#include <ProcessingGraph/Pad.h>
#include <QtCore>


namespace AVQt {
    class CommandBuilder;

    class Command {
    public:
        class Type {
        public:
            enum Enum {
                INIT,
                CLEANUP,
                START,
                STOP,
                PAUSE,
                NONE
            };
            QString name();

            Type &operator=(Enum &type);

            explicit Type(const Enum &type);
            Type();

        private:
            Enum m_type;
        };

        explicit Command(QObject *parent = nullptr) {
            qFatal("Error");
        };

        Command(Command &&c) noexcept;
        Command(const Command &c);

        static CommandBuilder builder();

        QVariant getPayload(const QString &key);
        QVariantMap getPayloads();
        Type getType();

    private:
        Command(Type type, QVariantMap payload);

        Type m_type;
        QVariantMap m_payload;

        friend class CommandBuilder;
    };

    class CommandBuilder {
    public:
        CommandBuilder &withType(Command::Type::Enum type);
        template<typename T, typename... Ts>
        CommandBuilder &withPayload(QPair<QString, T> p, QPair<QString, Ts>... pl);
        CommandBuilder &withPayload(const QVariantMap &pl);
        CommandBuilder &withPayload(const QString &key, const QVariant &p);

        Command build();

    private:
        CommandBuilder();

        Command::Type m_type;
        QVariantMap m_payload;

        friend class Command;
    };

    using CommandPad = ProcessingGraph::Pad<Command>;
}// namespace AVQt

PG_DECLARE_DATATYPE(AVQt::Command)

#endif//LIBAVQT_COMMAND_H
