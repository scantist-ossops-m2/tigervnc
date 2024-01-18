#ifndef VNCWINVIEW_H
#define VNCWINVIEW_H

#include "PlatformPixelBuffer.h"
#include "abstractvncview.h"

#include <map>
#include <windows.h>

class QTimer;
class Win32TouchHandler;

class QVNCWinView : public QAbstractVNCView
{
    Q_OBJECT

public:
    QVNCWinView(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Window);
    virtual ~QVNCWinView();

    void  setWindow(HWND);
    QRect getExtendedFrameProperties() override;
    void  handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) override;
    void  handleKeyRelease(int keyCode) override;
    void  dim(bool enabled) override;

public slots:
    void setQCursor(QCursor const& cursor) override;
    void setCursorPos(int x, int y) override;
    void pushLEDState() override;
    void setLEDState(unsigned int state) override;
    void grabKeyboard() override;
    void ungrabKeyboard() override;
    void bell() override;
    void moveView(int x, int y) override;
    void updateWindow() override;

protected:
    static void getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);
    bool        event(QEvent* e) override;
    void        showEvent(QShowEvent*) override;
    void        enterEvent(QEvent*) override;
    void        leaveEvent(QEvent*) override;
    void        focusInEvent(QFocusEvent*) override;
    void        focusOutEvent(QFocusEvent*) override;
    void        resizeEvent(QResizeEvent*) override;
    void        paintEvent(QPaintEvent*) override;
    void        mouseMoveEvent(QMouseEvent*) override;
    void        mousePressEvent(QMouseEvent*) override;
    void        mouseReleaseEvent(QMouseEvent*) override;

    bool bypassWMHintingEnabled() const override
    {
        return true;
    }

private:
    PlatformPixelBuffer* framebuffer_;
    QRect                rect_;
    QPixmap              pixmap_;

    bool         altGrArmed_;
    unsigned int altGrCtrlTime_;
    QTimer*      altGrCtrlTimer_;

    Win32TouchHandler* touchHandler_;

    void resolveAltGrDetection(bool isAltGrSequence);
    int  handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam);
    int  handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam);
    int  handleTouchEvent(UINT message, WPARAM wParam, LPARAM lParam);
};

#endif // VNCWINVIEW_H
