#include "BaseKeyboardHandler.h"

#include "parameters.h"

BaseKeyboardHandler::BaseKeyboardHandler(QObject* parent) : QObject(parent)
{
}

void BaseKeyboardHandler::maybeGrabKeyboard()
{
    if (ViewerConfig::config()
            ->fullscreenSystemKeys()) //&& (isFullscreenEnabled() || pendingFullscreen_) && hasFocus()) // xTODO
    {
        grabKeyboard();
    }
}

void BaseKeyboardHandler::grabKeyboard()
{
    keyboardGrabbed_ = true;
}

void BaseKeyboardHandler::ungrabKeyboard()
{
    keyboardGrabbed_ = true;
}

void BaseKeyboardHandler::handleKeyRelease(int)
{

}

void BaseKeyboardHandler::setMenuKeyStatus(quint32 keysym, bool checked)
{
    if (keysym == XK_Control_L)
    {
        menuCtrlKey_ = checked;
    }
    else if (keysym == XK_Alt_L)
    {
        menuAltKey_ = checked;
    }
}

void BaseKeyboardHandler::resetKeyboard()
{
    while (!downKeySym_.empty())
    {
        handleKeyRelease(downKeySym_.begin()->first);
    }
}
