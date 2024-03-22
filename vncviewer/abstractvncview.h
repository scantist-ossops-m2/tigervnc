#ifndef ABSTRACTVNCVIEW_H
#define ABSTRACTVNCVIEW_H

#include <QWidget>
#include <QScrollArea>
#include <QList>
#include <QLabel>

class QMenu;
class QAction;
class QCursor;
class QLabel;
class QScreen;
class QClipboard;
class QMoveEvent;
class QGestureEvent;
class QVNCToast;
class EmulateMB;
class GestureHandler;
class BaseKeyboardHandler;

namespace rfb {
  struct Point;
}

using DownMap = std::map<int, quint32>;

class QAbstractVNCView : public QWidget
{
  Q_OBJECT

public:
  QAbstractVNCView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QAbstractVNCView();
  virtual void resize(int width, int height);
  void toggleContextMenu();
  virtual bool hasViewFocus() const { return true; }
  bool hasFocus() const { return hasViewFocus() || QWidget::hasFocus() || isActiveWindow(); }
  double devicePixelRatio() const { return devicePixelRatio_; }
  virtual double effectiveDevicePixelRatio(QScreen *screen = nullptr) const;
  QScreen *getCurrentScreen();
  QClipboard *clipboard() const { return clipboard_; }
  virtual QRect getExtendedFrameProperties();
  bool isVisibleContextMenu() const;
  void sendContextMenuKey();
  void sendCtrlAltDel();
  void toggleKey(bool toggle, int keyCode, quint32 keySym);
  virtual void disableIM();
  virtual void enableIM();
  virtual void resetKeyboard();
  virtual void dim(bool enabled) {}

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
  QScreen *screen() const;
#endif

public slots:
  virtual void setCursorPos(int x, int y);
  virtual void handleClipboardData(const char* data);
  virtual void maybeGrabKeyboard();
  virtual void grabKeyboard();
  virtual void ungrabKeyboard();
  virtual void grabPointer();
  virtual void ungrabPointer();
  virtual bool isFullscreenEnabled();
  virtual void bell() = 0;
  virtual void remoteResize(int width, int height);
  virtual void updateWindow();
  QRect toastGeometry() const;
  void showToast();
  void hideToast();
  void paintEvent(QPaintEvent *event) override;
  void getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);
  void getMouseProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  virtual void handleDesktopSize();
  virtual void fullscreen(bool enabled);
  virtual void moveView(int x, int y);
  void postRemoteResizeRequest();

signals:
  void fullscreenChanged(bool enabled);
  void delayedInitialized();

protected:
  QPixmap pixmap;
  QRegion damage;

  static QClipboard *clipboard_;
  QByteArray geometry_;
  double devicePixelRatio_;

  quint32 menuKeySym_;
  QMenu *contextMenu_;
  QList<QAction*> actions_;

  bool pendingServerClipboard_;
  bool pendingClientClipboard_;
  int clipboardSource_;
  bool firstUpdate_;
  bool delayedFullscreen_;
  bool delayedDesktopSize_;
  bool mouseGrabbed_;

  QTimer *resizeTimer_;
  QTimer *delayedInitializeTimer_;
  bool fullscreenEnabled_;
  bool pendingFullscreen_;

  DownMap downKeySym_;
  QTimer *mouseButtonEmulationTimer_;
  EmulateMB *mbemu_;

  int fw_;
  int fh_;
  int fxmin_;
  int fymin_;
  QScreen *fscreen_;

  rfb::Point *lastPointerPos_;
  int lastButtonMask_;
  QTimer *mousePointerTimer_;

  BaseKeyboardHandler* keyboardHandler_ = nullptr;
  void initKeyboardHandler();
  void installKeyboardHandler();
  void removeKeyboardHandler();

  void createContextMenu();
  QList<int> fullscreenScreens();
  void filterPointerEvent(const rfb::Point &pos, int buttonMask);
  void handleMouseButtonEmulationTimeout();
  void sendPointerEvent(const rfb::Point& pos, int buttonMask);
  virtual bool bypassWMHintingEnabled() const { return false; }
  virtual void fullscreenOnCurrentDisplay();
  virtual void fullscreenOnSelectedDisplay(QScreen *screen, int vx, int vy, int vwidth, int vheight);
  virtual void fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight);
  virtual void exitFullscreen();

  // As QMenu eventFilter
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  QTimer *toastTimer_;
  QSize toastSize_ = { 300, 40 };
};

#endif // ABSTRACTVNCVIEW_H
