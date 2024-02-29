#ifndef QUICKVNCITEM_H
#define QUICKVNCITEM_H

#include "EmulateMB.h"
#include "PlatformPixelBuffer.h"

#include <QImage>
#include <QQuickItem>
#include <QTimer>

class QuickVNCItem : public QQuickItem
{
    Q_OBJECT

public:
    QuickVNCItem(QQuickItem* parent = nullptr);
    QSGNode* updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData* updatePaintNodeData);

    void grabPointer();
    void ungrabPointer();

signals:
    void popupToast(QString const& text);

protected:
    void getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);

    void hoverMoveEvent(QHoverEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);

    void filterPointerEvent(rfb::Point const& pos, int mask);

private:
    void updateWindow();

    bool                 firstUpdate_ = true;
    PlatformPixelBuffer* framebuffer_;
    QRect                rect_;
    QImage               image_;

    bool       keyboardGrabbed_ = false;
    bool       mouseGrabbed_    = false;
    rfb::Point lastPointerPos_;
    int        lastButtonMask_;
    QTimer     mousePointerTimer_;
    QTimer     mouseButtonEmulationTimer_;
    QTimer     delayedInitializeTimer_;
    EmulateMB* mbemu_ = new EmulateMB(&mouseButtonEmulationTimer_);
};

#endif // QUICKVNCITEM_H
