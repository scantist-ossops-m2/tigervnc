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

//
// SignalInfo - Base class for all extra information included with a
//              signal. Any structure containing extra information must
//              be subclassed from SignalInfo.
//

struct SignalInfo {
    virtual ~SignalInfo() {}
};

class Object {
protected:
    // Must always be sub-classed
    Object();
public:
    virtual ~Object();

    // connectSignal() registers an object and method on that object to
    // be called whenever a signal of the specified name is emitted.
    // Inclusion of signal information must match how the signal is
    // emitted, or an exception will be thrown. Any method registered
    // will automatically be unregistered when the method's object is
    // destroyed.
    template<class T, class S>
    void connectSignal(const char *name, T *obj,
                       void (T::*callback)(S*, const char*));
    template<class T, class S, class I>
    void connectSignal(const char *name, T *obj,
                       void (T::*callback)(S*, const char*, const I&));

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal(). Only the specified object and
    // the specific name will be unregistered.
    template<class T, class S>
    void disconnectSignal(const char *name, T *obj,
                          void (T::*callback)(S*, const char*));
    template<class T, class S, class I>
    void disconnectSignal(const char *name, T *obj,
                          void (T::*callback)(S*, const char*, const I&));

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
    // specified name. Inclusion of signal information must match the
    // registered connected methods or an exception will be thrown.
    void emitSignal(const char *name);
    void emitSignal(const char *name, const SignalInfo &info);

private:
    // Helper classes to handle the type glue for calling object methods
    class SignalReceiver;
    template<class T, class S> class SignalReceiverTS;
    template<class T, class S, class I> class SignalReceiverTSI;

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
    virtual void emit(Object*, const char*, const SignalInfo&)=0;

    virtual Object* getObject()=0;
    virtual bool operator==(SignalReceiver&)=0;
};

template<class T, class S>
class Object::SignalReceiverTS: public Object::SignalReceiver {
public:
    SignalReceiverTS(T *obj, void (T::*callback)(S*, const char*));
    virtual ~SignalReceiverTS() {}

    virtual void emit(Object*, const char*);
    virtual void emit(Object*, const char*, const SignalInfo&);

    virtual Object* getObject();
    virtual bool operator==(SignalReceiver&);

private:
    T *obj;
    void (T::*callback)(S*, const char*);
};

template<class T, class S, class I>
class Object::SignalReceiverTSI: public Object::SignalReceiver {
public:
    SignalReceiverTSI(T *obj, void (T::*callback)(S*, const char*, const I&));
    virtual ~SignalReceiverTSI() {}

    virtual void emit(Object*, const char*);
    virtual void emit(Object*, const char*, const SignalInfo&);

    virtual Object* getObject();
    virtual bool operator==(SignalReceiver&);

private:
    T *obj;
    void (T::*callback)(S*, const char*, const I&);
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

template<class T, class S, class I>
void Object::connectSignal(const char *name, T *obj,
                           void (T::*callback)(S*, const char*, const I&))
{
    if (dynamic_cast<S*>(this) == NULL)
        throw Exception("Invalid callback object type");
    connectSignal(name, obj, new SignalReceiverTSI<T,S,I>(obj, callback));
}

template<class T, class S>
void Object::disconnectSignal(const char *name, T *obj,
                              void (T::*callback)(S*, const char*))
{
    SignalReceiverTS<T,S> other(obj, callback);
    disconnectSignal(name, obj, &other);
}

template<class T, class S, class I>
void Object::disconnectSignal(const char *name, T *obj,
                              void (T::*callback)(S*, const char*, const I&))
{
    SignalReceiverTSI<T,S,I> other(obj, callback);
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
void Object::SignalReceiverTS<T,S>::emit(Object * /*sender*/,
                                         const char * /*name*/,
                                         const SignalInfo & /*_info*/)
{
    throw Exception("Unexpected SignalInfo");
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

template<class T, class S, class I>
Object::SignalReceiverTSI<T, S, I>::SignalReceiverTSI(T *obj_, void (T::*callback_)(S*, const char*, const I&))
    : obj(obj_), callback(callback_)
{
}

template<class T, class S, class I>
void Object::SignalReceiverTSI<T, S, I>::emit(Object * /*sender*/,
                                              const char * /*name*/)
{
    throw Exception("Missing SignalInfo");
}

template<class T, class S, class I>
void Object::SignalReceiverTSI<T, S, I>::emit(Object *_sender, const char *name, const SignalInfo &_info)
{
    S *sender;
    const I *info;

    sender = dynamic_cast<S*>(_sender);
    if (sender == NULL)
        throw Exception("Incorrect object type");

    info = dynamic_cast<const I*>(&_info);
    if (info == NULL)
        throw Exception("Incorrect signal info");

    (obj->*callback)(sender, name, *info);
}

template<class T, class S, class I>
Object* Object::SignalReceiverTSI<T, S, I>::getObject()
{
    return obj;
}

template<class T, class S, class I>
bool Object::SignalReceiverTSI<T, S, I>::operator==(Object::SignalReceiver &_other)
{
    Object::SignalReceiverTSI<T, S, I> *other;

    other = dynamic_cast<Object::SignalReceiverTSI<T, S, I>*>(&_other);
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
