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
#include <set>
#include <map>

#include <core/Exception.h>

namespace core {

class Object {
protected:
    // Must always be sub-classed
    Object();
public:
    virtual ~Object();

    // connectSignal() registers an object and method on that object to
    // be called whenever a signal of the specified name is emitted. Any
    // method registered will automatically be unregistered when the
    // method's object is destroyed.
    template<class T, class S>
    void connectSignal(const char *name, T *obj,
                       void (T::*callback)(S*, const char*));

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal(). Only the specified object and
    // the specific name will be unregistered.
    template<class T, class S>
    void disconnectSignal(const char *name, T *obj,
                          void (T::*callback)(S*, const char*));

    // disconnectSignals() unregisters all methods for all names for the
    // specified object. This is automatically called when the specified
    // object is destroyed.
    void disconnectSignals(Object *obj);

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
    template<class T, class S> class SignalReceiverTS;

    void connectSignal(const char *name, Object *obj, SignalReceiver *receiver);
    void disconnectSignal(const char *name, Object *obj, SignalReceiver *receiver);

private:
    typedef std::list<SignalReceiver*> ReceiverList;

    // Mapping between signal names and the methods receiving them
    std::map<std::string, ReceiverList> signalReceivers;

    // Other objects that we have connected to signals on
    std::set<Object*> connectedObjects;
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

    virtual Object* getObject()=0;
    virtual bool operator==(SignalReceiver&)=0;
};

template<class T, class S>
class Object::SignalReceiverTS: public Object::SignalReceiver {
public:
    SignalReceiverTS(T *obj, void (T::*callback)(S*, const char*));
    virtual ~SignalReceiverTS() {}

    virtual void emit(Object*, const char*);

    virtual Object* getObject();
    virtual bool operator==(SignalReceiver&);

private:
    T *obj;
    void (T::*callback)(S*, const char*);
};

// Object - Inline methods definitions

template<class T, class S>
void Object::connectSignal(const char *name, T *obj,
                           void (T::*callback)(S*, const char*))
{
    if (dynamic_cast<S*>(this) == NULL)
        throw Exception("Invalid callback object type");
    connectSignal(name, obj, new SignalReceiverTS<T,S>(obj, callback));
}

template<class T, class S>
void Object::disconnectSignal(const char *name, T *obj,
                              void (T::*callback)(S*, const char*))
{
    SignalReceiverTS<T,S> other(obj, callback);
    disconnectSignal(name, obj, &other);
}

// Object::SignalReceiver - Inline methods definitions

template<class T, class S>
Object::SignalReceiverTS<T,S>::SignalReceiverTS(T *obj_, void (T::*callback_)(S*, const char*))
    : obj(obj_), callback(callback_)
{
}

template<class T, class S>
void Object::SignalReceiverTS<T,S>::emit(Object *_sender, const char *name)
{
    S *sender;

    sender = dynamic_cast<S*>(_sender);
    if (sender == NULL)
        throw Exception("Incorrect object type");

    (obj->*callback)(sender, name);
}

template<class T, class S>
Object* Object::SignalReceiverTS<T,S>::getObject()
{
    return obj;
}

template<class T, class S>
bool Object::SignalReceiverTS<T,S>::operator==(Object::SignalReceiver &_other)
{
    Object::SignalReceiverTS<T,S> *other;

    other = dynamic_cast<Object::SignalReceiverTS<T,S>*>(&_other);
    if (other == NULL)
        return false;

    if (other->obj != obj)
        return false;
    if (other->callback != callback)
        return false;

    return true;
}

}

#endif