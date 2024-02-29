#include "quickvncitem.h"

#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"

#include <QDateTime>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

QuickVNCItem::QuickVNCItem(QQuickItem* parent) : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);

    connect(AppManager::instance()->connection(),
            &QVNCConnection::refreshFramebufferEnded,
            this,
            &QuickVNCItem::updateWindow,
            Qt::QueuedConnection);
    connect(AppManager::instance(),
            &AppManager::refreshRequested,
            this,
            &QuickVNCItem::updateWindow,
            Qt::QueuedConnection);

    delayedInitializeTimer_.setInterval(1000);
    delayedInitializeTimer_.setSingleShot(true);
    connect(&delayedInitializeTimer_, &QTimer::timeout, this, [=]() {
        AppManager::instance()->connection()->refreshFramebuffer();
        emit popupToast(QString::asprintf(_("Press %s to open the context menu"),
                                          ViewerConfig::config()->menuKey().toStdString().c_str()));
    });
    delayedInitializeTimer_.start();

    mousePointerTimer_.setInterval(ViewerConfig::config()->pointerEventInterval());
    mousePointerTimer_.setSingleShot(true);
    connect(&mousePointerTimer_, &QTimer::timeout, this, [=]() {
        mbemu_->filterPointerEvent(lastPointerPos_, lastButtonMask_);
    });

    mouseButtonEmulationTimer_.setInterval(50);
    mouseButtonEmulationTimer_.setSingleShot(true);
    connect(&mouseButtonEmulationTimer_, &QTimer::timeout, this, [=]() {
        if (ViewerConfig::config()->viewOnly())
        {
            return;
        }
        mbemu_->handleTimeout();
    });
}

QSGNode* QuickVNCItem::updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData* updatePaintNodeData)
{
    auto node = dynamic_cast<QSGSimpleTextureNode*>(oldNode);

    if (!node)
    {
        node = new QSGSimpleTextureNode();
    }

    QSGTexture* texture = window()->createTextureFromImage(image_, QQuickWindow::TextureIsOpaque);
    node->setOwnsTexture(true);
    node->setRect(image_.rect());
    node->markDirty(QSGNode::DirtyForceUpdate);
    node->setTexture(texture);
    return node;
}

void QuickVNCItem::updateWindow()
{
    // copied from DesktopWindow.cxx
    QVNCConnection* cc = AppManager::instance()->connection();
    if (firstUpdate_)
    {
        if (cc->server()->supportsSetDesktopSize)
        {
            // Hack: Wait until we're in the proper mode and position until
            // resizing things, otherwise we might send the wrong thing.
            // xTODO
            // if (delayedFullscreen_)
            //     delayedDesktopSize_ = true;
            // else
            //     handleDesktopSize();
        }
        firstUpdate_ = false;
    }

    framebuffer_     = (PlatformPixelBuffer*)AppManager::instance()->connection()->framebuffer();
    rfb::Rect r      = framebuffer_->getDamage();
    int       x      = r.tl.x;
    int       y      = r.tl.y;
    int       width  = r.br.x - x;
    int       height = r.br.y - y;
    rect_            = QRect{x, y, x + width, y + height};
    if (!rect_.isEmpty())
    {
        image_ = framebuffer_->image();
        if (!image_.isNull())
        {
            update();
            qDebug() << QDateTime::currentDateTimeUtc() << "QuickVNCItem::updateWindow" << rect_ << image_.rect();
        }
    }
}

void QuickVNCItem::grabPointer()
{
    mouseGrabbed_ = true;
}

void QuickVNCItem::ungrabPointer()
{
    mouseGrabbed_ = false;
}

void QuickVNCItem::getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask, int& wheelMask)
{
    buttonMask = 0;
    wheelMask  = 0;
    if (event->buttons() & Qt::LeftButton)
    {
        buttonMask |= 1;
    }
    if (event->buttons() & Qt::MiddleButton)
    {
        buttonMask |= 2;
    }
    if (event->buttons() & Qt::RightButton)
    {
        buttonMask |= 4;
    }

    x = event->x();
    y = event->y();
}

void QuickVNCItem::filterPointerEvent(rfb::Point const& pos, int mask)
{
    if (ViewerConfig::config()->viewOnly())
    {
        return;
    }
    bool instantPosting = ViewerConfig::config()->pointerEventInterval() == 0 || (mask != lastButtonMask_);
    lastPointerPos_     = pos;
    lastButtonMask_     = mask;
    if (instantPosting)
    {
        mbemu_->filterPointerEvent(lastPointerPos_, lastButtonMask_);
    }
    else
    {
        mousePointerTimer_.start();
    }
}

void QuickVNCItem::hoverMoveEvent(QHoverEvent* event)
{
    grabPointer();
    filterPointerEvent(rfb::Point(event->pos().x(), event->pos().y()), 0);
}

void QuickVNCItem::mouseMoveEvent(QMouseEvent* event)
{
    grabPointer();
    int x, y, buttonMask, wheelMask;
    getMouseProperties(event, x, y, buttonMask, wheelMask);
    filterPointerEvent(rfb::Point(x, y), buttonMask | wheelMask);
}

void QuickVNCItem::mousePressEvent(QMouseEvent* event)
{
    qDebug() << "QuickVNCItem::mousePressEvent";

    if (ViewerConfig::config()->viewOnly())
    {
        return;
    }

    setFocus(Qt::FocusReason::MouseFocusReason);

    int x, y, buttonMask, wheelMask;
    getMouseProperties(event, x, y, buttonMask, wheelMask);
    filterPointerEvent(rfb::Point(x, y), buttonMask);

    if (keyboardGrabbed_ && !mouseGrabbed_)
    {
        grabPointer();
    }
}

void QuickVNCItem::mouseReleaseEvent(QMouseEvent* event)
{
    qDebug() << "QuickVNCItem::mouseReleaseEvent";

    if (ViewerConfig::config()->viewOnly())
    {
        return;
    }

    setFocus(Qt::FocusReason::MouseFocusReason);

    int x, y, buttonMask, wheelMask;
    getMouseProperties(event, x, y, buttonMask, wheelMask);
    filterPointerEvent(rfb::Point(x, y), buttonMask);

    if (keyboardGrabbed_ && !mouseGrabbed_)
    {
        grabPointer();
    }
}
