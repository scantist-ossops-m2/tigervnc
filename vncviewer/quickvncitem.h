#ifndef QUICKVNCITEM_H
#define QUICKVNCITEM_H

#include "BaseKeyboardHandler.h"
#include "EmulateMB.h"
#include "PlatformPixelBuffer.h"

#include <QImage>
#include <QOpenGLFunctions>
#include <QQuickItem>
#include <QTimer>
#include <rfb/Timer.h>

struct _XDisplay;
class QSGTexture;

class QuickVNCItem : public QQuickItem, public rfb::Timer::Callback
{
  Q_OBJECT
  Q_PROPERTY(bool contextMenuVisible READ contextMenuVisible WRITE setContextMenuVisible NOTIFY
                 contextMenuVisibleChanged FINAL)

public:
  QuickVNCItem(QQuickItem* parent = nullptr);
  ~QuickVNCItem();
  QSGNode* updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData* updatePaintNodeData);

  bool contextMenuVisible() const;
  void setContextMenuVisible(bool newContextMenuVisible);

  Q_INVOKABLE QPointF cursorPos() const;

public slots:
  void bell();
  void menuKey();
  void ctrlKeyToggle(bool checked);
  void altKeyToggle(bool checked);
  void ctrlAltDel();

signals:
  void popupToast(QString const& text);
  void contextMenuVisibleChanged();

protected:
  void grabPointer();
  void ungrabPointer();

  void getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);
  void getMouseProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);

  void focusInEvent(QFocusEvent* event);
  void focusOutEvent(QFocusEvent* event);
  void hoverEnterEvent(QHoverEvent* event);
  void hoverLeaveEvent(QHoverEvent* event);
  void hoverMoveEvent(QHoverEvent* event);

  void mouseMoveEvent(QMouseEvent* event);
  void mousePressEvent(QMouseEvent* event);
  void mouseReleaseEvent(QMouseEvent* event);

  void wheelEvent(QWheelEvent* event);

  void filterPointerEvent(rfb::Point const& pos, int mask);

private:
  void updateWindow();

  virtual bool handleTimeout(rfb::Timer* t);

  bool                 firstUpdate_ = true;
  PlatformPixelBuffer* framebuffer_ = nullptr;
  QRect                rect_;
  QImage               image_;

  bool       mouseGrabbed_ = false;
  rfb::Point lastPointerPos_;
  int        lastButtonMask_;
  QTimer     mousePointerTimer_;
  QTimer     mouseButtonEmulationTimer_;
  QTimer     delayedInitializeTimer_;
  EmulateMB* mbemu_ = new EmulateMB(&mouseButtonEmulationTimer_);

  BaseKeyboardHandler* keyboardHandler_ = nullptr;

  QAtomicInt     fpsCounter;
  struct timeval fpsLast;
  rfb::Timer     fpsTimer;

#ifdef Q_OS_LINUX
  _XDisplay* display_;
#endif

  bool contextMenuVisible_ = false;
  int  widthDivider        = 1;
  int  heightDivider       = 1;
};

#endif // QUICKVNCITEM_H
