#ifndef APPMANAGER_H
#define APPMANAGER_H

#include "vncconnection.h"

#include <QObject>

class QuickVNCView;
class QTimer;

class AppManager : public QObject
{
  Q_OBJECT
  Q_PROPERTY(int error READ error NOTIFY errorOcurred)
  Q_PROPERTY(QVNCConnection* connection READ connection CONSTANT)
  Q_PROPERTY(int remoteViewWidth READ remoteViewWidth NOTIFY remoteViewSizeChanged)
  Q_PROPERTY(int remoteViewHeight READ remoteViewHeight NOTIFY remoteViewSizeChanged)
  Q_PROPERTY(bool visibleInfo READ visibleInfo NOTIFY visibleInfoChanged)
  Q_PROPERTY(bool isFullscreen READ isFullscreen WRITE setIsFullscreen NOTIFY isFullscreenChanged)

public:
  virtual ~AppManager();

  static AppManager* instance()
  {
    return manager_;
  }

  static int initialize();

  QVNCConnection* connection() const
  {
    return facade_;
  }

  int error() const
  {
    return error_;
  }

  bool visibleInfo() const
  {
    return visibleInfo_;
  }

  int remoteViewWidth() const
  {
    return remoteViewWidth_;
  }

  int remoteViewHeight() const
  {
    return remoteViewHeight_;
  }

  bool isFullscreen() const;
  void setIsFullscreen(bool newIsFullscreen);

signals:
  void errorOcurred(int seq, QString message, bool quit = false);
  void credentialRequested(bool secured, bool userNeeded, bool passwordNeeded);
  void messageDialogRequested(int flags, QString title, QString text);
  void dataReady(QByteArray bytes);
  void connectToServerRequested(QString const addressport);
  void authenticateRequested(QString user, QString password);
  void cancelAuthRequested();
  void messageResponded(int response);
  void newVncWindowRequested(int width, int height, QString name);
  void resetConnectionRequested();
  void invalidateRequested(int x0, int y0, int x1, int y1);
  void refreshRequested();
  void contextMenuRequested();
  void infoDialogRequested();
  void optionDialogRequested();
  void aboutDialogRequested();
  void vncWindowOpened();
  void vncWindowClosed();
  void visibleInfoChanged();
  void remoteViewSizeChanged(int width, int height);

  void isFullscreenChanged();

public slots:
  void publishError(QString const message, bool quit = false);
  void connectToServer(QString const addressport);
  void authenticate(QString user, QString password);
  void cancelAuth();
  void resetConnection();
  void openVNCWindow(int width, int height, QString name);
  void minimizeVNCWindow();
  void closeVNCWindow();
  void updateWindow();
  void setWindowName(QString name);
  /**
   * @brief Request the framebuffer to add the given dirty region. Typically, called
   * by PlatformPixelBuffer::commitBufferRW().
   * @param x0 X of the top left point.
   * @param y0 Y of the top left point.
   * @param x1 X of the bottom right point.
   * @param y1 Y of the bottom right point.
   */
  void invalidate(int x0, int y0, int x1, int y1);
  void refresh();
  void openContextMenu();
  void openInfoDialog();
  void openOptionDialog();
  void openAboutDialog();
  void respondToMessage(int response);
  void closeOverlay();
  void toggleFullscreen();

private:
  AppManager();

  static AppManager* manager_;
  int                error_;
  QVNCConnection*    facade_;
  QuickVNCView*      connectionView_;
  QTimer*            rfbTimerProxy_;
  bool               visibleInfo_;
  int                remoteViewWidth_  = 0;
  int                remoteViewHeight_ = 0;
  bool               isFullscreen_;
};

#endif // APPMANAGER_H
