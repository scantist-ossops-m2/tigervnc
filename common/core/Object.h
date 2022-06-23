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

//
// Object - Base class for all non-trival objects. Handles signal
//          infrastructure for passing events between objects.
//

#ifndef __CORE_OBJECT_H__
#define __CORE_OBJECT_H__

#include <string>
#include <list>
#include <map>

namespace core {

class Object {
protected:
    // Must always be sub-classed
    Object();
public:
    virtual ~Object();

    // connectSignal() registers an object and method on that object to
    // be called whenever a signal of the specified name is emitted.
    template<class T>
    void connectSignal(const char *name, T *obj,
                       void (T::*callback)(Object*, const char*));

protected:
    // registerSignal() registers a new signal type with the specified
    // name. This must always be done before connectSignal() or
    // emitSignal() is used.
    void registerSignal(const char *name);

    // emitSignal() calls all the registered object methods for the
    // specified name.
    void emitSignal(const char *name);

private:
    // Helper classes to handle the type glue for calling object methods
    class SignalReceiver;
    template<class T> class SignalReceiverT;

    void connectSignal(const char *name, SignalReceiver *receiver);

private:
    typedef std::list<SignalReceiver*> ReceiverList;

    // Mapping between signal names and the methods receiving them
    std::map<std::string, ReceiverList> signalReceivers;
};

//
// Object::SignalReceiver - Glue objects that allow us to call any
//                          method on any object, as long as the object
//                          is derived from Object.
//

class Object::SignalReceiver {
public:
    SignalReceiver() {}
    virtual ~SignalReceiver() {}

    virtual void emit(Object*, const char*)=0;
};

template<class T>
class Object::SignalReceiverT: public Object::SignalReceiver {
public:
    SignalReceiverT(T *obj, void (T::*callback)(Object*, const char*));
    virtual ~SignalReceiverT() {}

    virtual void emit(Object*, const char*);

private:
    T *obj;
    void (T::*callback)(Object*, const char*);
};

// Object - Inline methods definitions

template<class T>
void Object::connectSignal(const char *name, T *obj,
                           void (T::*callback)(Object*, const char*))
{
    connectSignal(name, new SignalReceiverT<T>(obj, callback));
}

// Object::SignalReceiver - Inline methods definitions

template<class T>
Object::SignalReceiverT<T>::SignalReceiverT(T *obj_, void (T::*callback_)(Object*, const char*))
    : obj(obj_), callback(callback_)
{
}

template<class T>
void Object::SignalReceiverT<T>::emit(Object *sender, const char *name)
{
    (obj->*callback)(sender, name);
}

}

#endif
