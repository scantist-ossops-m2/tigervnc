#ifndef BASEKEYBOARDHANDLER_H
#define BASEKEYBOARDHANDLER_H

#include <QAbstractNativeEventFilter>
#include <QTimer>
#include <QDataStream>
#include <QTextStream>
#include <QUrl>

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"

using DownMap = std::map<int, quint32>;

class BaseKeyboardHandler : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    BaseKeyboardHandler(QObject* parent = nullptr);

    virtual void maybeGrabKeyboard();
    virtual void grabKeyboard();
    virtual void ungrabKeyboard();

    virtual void setLEDState(unsigned int state) = 0;
    virtual void pushLEDState()                  = 0;

    virtual void handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) = 0;
    virtual void handleKeyRelease(int)                                                      = 0;
    void         resetKeyboard();

    void setMenuKeyStatus(quint32 keysym, bool checked);

protected:
    bool keyboardGrabbed_ = false;

    DownMap downKeySym_;

    quint32 menuKeySym_  = XK_F8;
    bool    menuCtrlKey_ = false;
    bool    menuAltKey_  = false;
};

#endif // BASEKEYBOARDHANDLER_H
