/* Copyright 2022 Pierre Ossman for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <core/Exception.h>
#include <core/Object.h>

using namespace core;

Object::Object()
{
}

Object::~Object()
{
    // Disconnect from any signals we might have subscribed to
    while (!connectedObjects.empty())
        (*connectedObjects.begin())->disconnectSignals(this);

    // And prevent other objects from trying to disconnect from us as we
    // are going away
    {
        std::map<std::string, ReceiverList>::iterator sigiter;

        for (sigiter = signalReceivers.begin(); sigiter != signalReceivers.end(); ++sigiter) {
            ReceiverList *siglist;

            siglist = &sigiter->second;
            while (!siglist->empty())
                disconnectSignals((*siglist->begin())->getObject());
        }
    }

    while (!signalCheckers.empty()) {
        delete signalCheckers.begin()->second;
        signalCheckers.erase(signalCheckers.begin());
    }
}

void Object::registerSignal(const char *name, InfoChecker *checker)
{
    if (signalReceivers.count(name) != 0)
        throw Exception("Signal already registered: %s", name);

    // Just to force it being created
    signalReceivers[name].clear();

    signalCheckers[name] = checker;
}

void Object::emitSignal(const char *name)
{
    ReceiverList *siglist;
    ReceiverList::iterator iter;

    if (signalReceivers.count(name) == 0)
        throw Exception("Unknown signal: %s", name);

    if (signalCheckers[name] != NULL)
        throw Exception("Missing signal info");

    siglist = &signalReceivers[name];
    for (iter = siglist->begin(); iter != siglist->end(); ++iter)
        (*iter)->emit(this, name);
}

void Object::emitSignal(const char *name, const SignalInfo &info)
{
    ReceiverList *siglist;
    ReceiverList::iterator iter;

    if (signalReceivers.count(name) == 0)
        throw Exception("Unknown signal: %s", name);

    if (signalCheckers[name] == NULL)
        throw Exception("Unexpected signal info");

    if (!signalCheckers[name]->isInstanceOf(info))
        throw Exception("Wrong signal info");

    siglist = &signalReceivers[name];
    for (iter = siglist->begin(); iter != siglist->end(); ++iter)
        (*iter)->emit(this, name, info);
}

void Object::connectSignal(const char *name, Object *obj,
                           SignalReceiver *receiver,
                           const std::type_info *info)
{
    ReceiverList *siglist;
    ReceiverList::iterator iter;

    if (signalReceivers.count(name) == 0)
        throw Exception("Unknown signal: %s", name);

    if (signalCheckers[name] == NULL) {
        if (info != NULL)
            throw Exception("Unexpected signal info");
    } else {
        if (info == NULL)
            throw Exception("Missing signal info");
        if (!signalCheckers[name]->isType(*info))
            throw Exception("Wrong signal info");
    }

    siglist = &signalReceivers[name];

    iter = siglist->begin();
    while (iter != siglist->end()) {
        if (**iter == *receiver)
            throw Exception("Already connected: %s", name);
        else
            ++iter;
    }

    siglist->push_back(receiver);

    obj->connectedObjects.insert(this);
}

void Object::disconnectSignal(const char *name, Object *obj,
                              SignalReceiver *receiver)
{
    ReceiverList *siglist;
    ReceiverList::iterator iter;
    bool hasOthers;

    if (signalReceivers.count(name) == 0)
        throw Exception("Unknown signal: %s", name);

    hasOthers = false;
    siglist = &signalReceivers[name];
    iter = siglist->begin();
    while (iter != siglist->end()) {
        if (**iter == *receiver) {
            delete *iter;
            siglist->erase(iter++);
        } else {
            if ((*iter)->getObject() == obj)
                hasOthers = true;
            ++iter;
        }
    }

    if (!hasOthers)
        obj->connectedObjects.erase(this);
}

void Object::disconnectSignals(Object *obj)
{
    std::map<std::string, ReceiverList>::iterator sigiter;

    for (sigiter = signalReceivers.begin();
         sigiter != signalReceivers.end(); ++sigiter) {
        ReceiverList *siglist;
        ReceiverList::iterator iter;

        siglist = &sigiter->second;
        iter = siglist->begin();
        while (iter != siglist->end()) {
            if ((*iter)->getObject() == obj) {
                delete *iter;
                siglist->erase(iter++);
            } else {
                ++iter;
            }
        }
    }

    obj->connectedObjects.erase(this);
}
