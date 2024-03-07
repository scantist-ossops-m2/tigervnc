#include "quickvncitem.h"

#include "appmanager.h"
#include "i18n.h"
#include "menukey.h"
#include "parameters.h"
#include "rdr/Exception.h"
#include "rfb/LogWriter.h"

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QDateTime>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#ifdef Q_OS_WINDOWS
#include "Win32KeyboardHandler.h"
#endif

#ifdef Q_OS_LINUX
#include "X11KeyboardHandler.h"

#include <QX11Info>
#include <X11/extensions/Xrender.h>
#endif

#ifdef Q_OS_DARWIN
#include "MacKeyboardHandler.h"
#include "cocoa.h"
#endif

static rfb::LogWriter vlog("QuickVNCItem");

QuickVNCItem::QuickVNCItem(QQuickItem* parent) : QQuickItem(parent)
{
  setFlag(QQuickItem::ItemHasContents, true);
  setAcceptHoverEvents(true);
  setAcceptedMouseButtons(Qt::AllButtons);
  setKeepMouseGrab(true);

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

#ifdef Q_OS_WINDOWS
  keyboardHandler_ = new Win32KeyboardHandler(this);
#endif

#ifdef Q_OS_LINUX
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  display_ = QX11Info::display();
#else
  display_ = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  keyboardHandler_ = new X11KeyboardHandler(this);
#endif

#ifdef Q_OS_DARWIN
  keyboardHandler_ = new MacKeyboardHandler(this);
#endif

  connect(keyboardHandler_, &BaseKeyboardHandler::contextMenuKeyPressed, this, [=](bool menuShortCutMode) {
    if (contextMenuVisible())
    {
      if (!menuShortCutMode)
      {
        if (ViewerConfig::config()->viewOnly())
          return;
        menuKey();
        setContextMenuVisible(false);
      }
    }
    else
    {
      setContextMenuVisible(true);
    }
  });

  QAbstractEventDispatcher::instance()->installNativeEventFilter(keyboardHandler_);
  connect(
      AppManager::instance(),
      &AppManager::vncWindowClosed,
      this,
      [=]() {
        QAbstractEventDispatcher::instance()->removeNativeEventFilter(keyboardHandler_);
      },
      Qt::QueuedConnection);

  qDebug() << "QuickVNCItem";
}

QuickVNCItem::~QuickVNCItem()
{
  qDebug() << "~QuickVNCItem";
}

QSGNode* QuickVNCItem::updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData* updatePaintNodeData)
{
  if (texture)
  {
    if (rect_.isEmpty())
      return oldNode;

    auto node = dynamic_cast<QSGSimpleTextureNode*>(oldNode);

    if (node->rect() == image_.rect())
    {
      texture->bind();
      glPixelStorei(GL_UNPACK_ROW_LENGTH, image_.bytesPerLine() / 4);
      glTexSubImage2D(GL_TEXTURE_2D,
                      0,
                      rect_.x(),
                      rect_.y(),
                      rect_.width(),
                      rect_.height(),
                      GL_BGRA,
                      GL_UNSIGNED_BYTE,
                      image_.constScanLine(rect_.y()) + rect_.x() * 4);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      // glTexImage2D(GL_TEXTURE_2D,
      //              0,
      //              GL_RGBA,
      //              image_.width(),
      //              image_.height(),
      //              0,
      //              GL_BGRA,
      //              GL_UNSIGNED_BYTE,
      //              image_.constBits());
      rect_ = QRect();

      node->markDirty(QSGNode::DirtyForceUpdate);

      update();
      return node;
    }
  }

  if (rect_.isEmpty())
    return oldNode;

  auto node = dynamic_cast<QSGSimpleTextureNode*>(oldNode);

  if (!node)
  {
    node = new QSGSimpleTextureNode();
  }

  initializeOpenGLFunctions();
  if (texture)
    texture->deleteLater();
  texture = window()->createTextureFromImage(image_, QQuickWindow::TextureIsOpaque);
  node->setOwnsTexture(true);
  node->setRect(image_.rect());
  node->markDirty(QSGNode::DirtyForceUpdate);
  node->setTexture(texture);
  rect_ = QRect();

  return node;
}

void QuickVNCItem::updateWindow()
{
  framebuffer_ = (PlatformPixelBuffer*)AppManager::instance()->connection()->framebuffer();
  if (!framebuffer_)
    return;

  rfb::Rect r      = framebuffer_->getDamage();
  int       x      = r.tl.x;
  int       y      = r.tl.y;
  int       width  = r.br.x - x;
  int       height = r.br.y - y;
  QRect     rect   = QRect{x, y, width, height};

  if (rect_.isNull())
    rect_ = rect;
  else
    rect_ = rect_.united(rect);

  if (boundingRect() != image_.rect())
    AppManager::instance()->setRemoteViewSize(image_.width(), image_.height());

  image_ = framebuffer_->image();
  update();
}

void QuickVNCItem::bell()
{
#if defined(Q_OS_WINDOWS)
  MessageBeep(0xFFFFFFFF); // cf. fltk/src/drivers/WinAPI/Fl_WinAPI_Screen_Driver.cxx:245
#endif

#ifdef Q_OS_DARWIN
  cocoa_beep();
#endif

#ifdef Q_OS_LINUX
  XBell(display_, 0 /* volume */);
#endif
}

void QuickVNCItem::menuKey()
{
  int     dummy;
  int     keyCode;
  quint32 keySym;
  ::getMenuKey(&dummy, &keyCode, &keySym);
  keyboardHandler_->handleKeyPress(keyCode, keySym, true);
  keyboardHandler_->handleKeyRelease(keyCode);
}

void QuickVNCItem::ctrlKeyToggle(bool checked)
{
  int     keyCode = 0x1d;
  quint32 keySym  = XK_Control_L;
  if (checked)
    keyboardHandler_->handleKeyPress(keyCode, keySym);
  else
    keyboardHandler_->handleKeyRelease(keyCode);
  keyboardHandler_->setMenuKeyStatus(keySym, checked);
}

void QuickVNCItem::altKeyToggle(bool checked)
{
  int     keyCode = 0x38;
  quint32 keySym  = XK_Alt_L;
  if (checked)
    keyboardHandler_->handleKeyPress(keyCode, keySym);
  else
    keyboardHandler_->handleKeyRelease(keyCode);
  keyboardHandler_->setMenuKeyStatus(keySym, checked);
}

void QuickVNCItem::ctrlAltDel()
{
  keyboardHandler_->handleKeyPress(0x1d, XK_Control_L);
  keyboardHandler_->handleKeyPress(0x38, XK_Alt_L);
  keyboardHandler_->handleKeyPress(0xd3, XK_Delete);
  keyboardHandler_->handleKeyRelease(0xd3);
  keyboardHandler_->handleKeyRelease(0x38);
  keyboardHandler_->handleKeyRelease(0x1d);
}

bool QuickVNCItem::contextMenuVisible() const
{
  return contextMenuVisible_;
}

void QuickVNCItem::setContextMenuVisible(bool newContextMenuVisible)
{
  keyboardHandler_->setContextMenuVisible(newContextMenuVisible);
  if (contextMenuVisible_ == newContextMenuVisible)
    return;
  contextMenuVisible_ = newContextMenuVisible;
  emit contextMenuVisibleChanged();
}

QPointF QuickVNCItem::cursorPos() const
{
  return QCursor::pos();
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
  if (event->buttons() & Qt::XButton1)
  {
    wheelMask |= 32;
  }
  if (event->buttons() & Qt::XButton2)
  {
    wheelMask |= 64;
  }

  x = event->x();
  y = event->y();
}

void QuickVNCItem::getMouseProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask)
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
  if (event->buttons() & Qt::XButton1)
  {
    wheelMask |= 32;
  }
  if (event->buttons() & Qt::XButton2)
  {
    wheelMask |= 64;
  }
  if (event->delta() > 0)
  {
    wheelMask |= 8;
  }
  if (event->delta() < 0)
  {
    wheelMask |= 16;
  }

  x = event->x();
  y = event->y();
}

void QuickVNCItem::focusInEvent(QFocusEvent* event)
{
  grabPointer();
  keyboardHandler_->maybeGrabKeyboard();
  // We may have gotten our lock keys out of sync with the server
  // whilst we didn't have focus. Try to sort this out.
  keyboardHandler_->pushLEDState();
  // Resend Ctrl/Alt if needed
  if (keyboardHandler_->menuCtrlKey())
  {
    keyboardHandler_->handleKeyPress(0x1d, XK_Control_L);
  }
  if (keyboardHandler_->menuAltKey())
  {
    keyboardHandler_->handleKeyPress(0x38, XK_Alt_L);
  }
  QQuickItem::focusInEvent(event);
}

void QuickVNCItem::focusOutEvent(QFocusEvent* event)
{
  grabPointer();
  if (ViewerConfig::config()->fullscreenSystemKeys())
    keyboardHandler_->ungrabKeyboard();
  keyboardHandler_->resetKeyboard();
  QQuickItem::focusOutEvent(event);
}

void QuickVNCItem::hoverEnterEvent(QHoverEvent* event)
{
  grabPointer();
  QQuickItem::hoverEnterEvent(event);
}

void QuickVNCItem::hoverLeaveEvent(QHoverEvent* event)
{
  ungrabPointer();
  QQuickItem::hoverLeaveEvent(event);
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
    if (!mousePointerTimer_.isActive())
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
  // qDebug() << "QuickVNCItem::mousePressEvent" << event->x() << event->y();
  grabPointer();
  keyboardHandler_->maybeGrabKeyboard();
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

  grabPointer();
  keyboardHandler_->maybeGrabKeyboard();
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

  grabPointer();
  keyboardHandler_->maybeGrabKeyboard();
}

void QuickVNCItem::wheelEvent(QWheelEvent* event)
{
  qDebug() << "QuickVNCItem::wheelEvent";

  int x, y, buttonMask, wheelMask;
  getMouseProperties(event, x, y, buttonMask, wheelMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask | wheelMask);
}
