#ifndef WIN32KEYBOARDHANDLER_H
#define WIN32KEYBOARDHANDLER_H

#include "BaseKeyboardHandler.h"

#include <QQuickView>
#include <QTimer>
#include <windows.h>

class Win32KeyboardHandler : public BaseKeyboardHandler
{
    Q_OBJECT

public:
    Win32KeyboardHandler(QObject* parent = nullptr);

    bool nativeEventFilter(QByteArray const& eventType, void* message, long*) override;

    void handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false);
    void handleKeyRelease(int keyCode) override;

public slots:
    void pushLEDState();
    void setLEDState(unsigned int state);
    void grabKeyboard() override;
    void ungrabKeyboard() override;

private:
    bool         altGrArmed_ = false;
    unsigned int altGrCtrlTime_;
    QTimer       altGrCtrlTimer_;

    void resolveAltGrDetection(bool isAltGrSequence);
    int  handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam);
    int  handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam);
};

#endif // WIN32KEYBOARDHANDLER_H
