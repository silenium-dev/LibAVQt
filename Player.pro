# Created by and for Qt Creator This file was created for editing the project sources only.
# You may attempt to use it for building too, by modifying this file here.

#TARGET = Player

TEMPLATE = subdirs app
CONFIG += ordered

SUBDIRS += AVQt \
           Player
Player.depends = AVQt
