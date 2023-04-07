#ifndef ABSTRACTVNCVIEW_H
#define ABSTRACTVNCVIEW_H

#include <QWidget>
#include <QList>

class QMenu;
class QAction;
class QCursor;

class QAbstractVNCView : public QWidget
{
  Q_OBJECT
public:
  QAbstractVNCView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QAbstractVNCView();
  void resize(int width, int height);
  void popupContextMenu();

public slots:
  virtual void handleKeyPress(int keyCode, quint32 keySym);
  virtual void handleKeyRelease(int keyCode);
  virtual void setQCursor(const QCursor &cursor);
  virtual void setCursorPos(int x, int y);
  virtual void pushLEDState();
  virtual void setLEDState(unsigned int state);
  virtual void handleClipboardAnnounce(bool available);
  virtual void handleClipboardData(const char* data);
  virtual void maybeGrabKeyboard();
  virtual void grabKeyboard();
  virtual void ungrabKeyboard();
  virtual void grabPointer();
  virtual void ungrabPointer();
  virtual bool isFullscreen();
  virtual void bell();
  virtual void handleResizeTimeout();
  virtual void remoteResize(int width, int height);
  virtual void updateWindow();
  virtual void handleDesktopSize();

protected:
  double m_devicePixelRatio;

  QMenu *m_contextMenu;
  QList<QAction*> m_actions;

  bool m_firstLEDState;
  bool m_pendingServerClipboard;
  bool m_pendingClientClipboard;
  int m_clipboardSource;
  bool m_firstUpdate;
  bool m_delayedFullscreen;
  bool m_delayedDesktopSize;
  bool m_keyboardGrabbed;
  bool m_mouseGrabbed;

  QTimer *m_resizeTimer;

  void createContextMenu();
  void postResizeRequest();
};

#endif // ABSTRACTVNCVIEW_H
