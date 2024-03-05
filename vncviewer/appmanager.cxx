#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "appmanager.h"
#include "contextmenuactions.h"
#include "i18n.h"
#include "parameters.h"
#include "quickvncitem.h"
#include "quickvncview.h"
#include "rfb/Timer.h"
#include "vncconnection.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QQmlEngine>
#include <QQuickView>
#include <QScreen>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#undef asprintf

AppManager* AppManager::manager_;

AppManager::AppManager()
    : QObject(nullptr), error_(0), facade_(new QVNCConnection), connectionView_(nullptr), rfbTimerProxy_(new QTimer),
      visibleInfo_(false)
{
  connect(this, &AppManager::connectToServerRequested, facade_, &QVNCConnection::connectToServer);
  connect(facade_, &QVNCConnection::newVncWindowRequested, this, &AppManager::openVNCWindow);
  connect(this, &AppManager::resetConnectionRequested, facade_, &QVNCConnection::resetConnection);
  connect(rfbTimerProxy_, &QTimer::timeout, this, [this]() {
    rfb::Timer::checkTimeouts();
  });
  connect(QApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, [this]() {
    int next = rfb::Timer::checkTimeouts();
    if (next != 0)
      rfbTimerProxy_->start(next);
  });
  rfbTimerProxy_->setSingleShot(true);

  connect(ViewerConfig::config(), &ViewerConfig::fullScreenChanged, this, [this](bool enabled) {
    setIsFullscreen(enabled);
  });

  // connect(AppManager::instance()->connection(),
  //         &QVNCConnection::refreshFramebufferEnded,
  //         this,
  //         &AppManager::updateWindow,
  //         Qt::QueuedConnection);
  // connect(AppManager::instance(), &AppManager::refreshRequested, this, &AppManager::updateWindow,
  // Qt::QueuedConnection);
}

bool AppManager::isFullscreen() const
{
  return isFullscreen_;
}

void AppManager::setIsFullscreen(bool newIsFullscreen)
{
  if (isFullscreen_ == newIsFullscreen)
    return;
  isFullscreen_ = newIsFullscreen;
  emit isFullscreenChanged();
  if (connectionView_)
    connectionView_->fullscreen(isFullscreen());
}

void AppManager::toggleFullscreen()
{
  setIsFullscreen(!isFullscreen());
}

AppManager::~AppManager()
{
  facade_->deleteLater();
  connectionView_->deleteLater();
  rfbTimerProxy_->deleteLater();
}

int AppManager::initialize(QQmlApplicationEngine* engine)
{
  qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
  qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
  qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
  qmlRegisterType<QVNCConnection>("Qt.TigerVNC", 1, 0, "VNCConnection");
  manager_             = new AppManager();
  manager_->qmlEngine_ = engine;
  qmlRegisterSingletonType<AppManager>("Qt.TigerVNC",
                                       1,
                                       0,
                                       "AppManager",
                                       [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
                                         Q_UNUSED(engine)
                                         Q_UNUSED(scriptEngine)
                                         return manager_;
                                       });
  qmlRegisterType<QuickVNCItem>("Qt.TigerVNC", 1, 0, "VNCItem");
  return 0;
}

void AppManager::connectToServer(QString const addressport)
{
  emit connectToServerRequested(addressport);
}

void AppManager::authenticate(QString user, QString password)
{
  emit authenticateRequested(user, password);
}

void AppManager::cancelAuth()
{
  emit cancelAuthRequested();
}

void AppManager::resetConnection()
{
  emit resetConnectionRequested();
}

void AppManager::publishError(QString const message, bool quit)
{
  QString text(message);
  if (!quit)
  {
    text = QString::asprintf(_("%s\n\nAttempt to reconnect?"), message.toStdString().c_str());
  }
  emit errorOcurred(error_++, text, quit);
}

void AppManager::openVNCWindow(int width, int height, QString name)
{
  connectionView_ = new QuickVNCView(qmlEngine_);
  connectionView_->resize(width, height);
  remoteViewWidth_  = width;
  remoteViewHeight_ = height;
  emit remoteViewSizeChanged(remoteViewWidth_, remoteViewHeight_);
  connectionView_->setTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
  connectionView_->show();

  setIsFullscreen(ViewerConfig::config()->fullScreen());

  updateWindow();

  emit vncWindowOpened();
}

void AppManager::minimizeVNCWindow()
{
  connectionView_->showMinimized();
}

void AppManager::closeVNCWindow()
{
  qDebug() << "AppManager::closeVNCWindow";
  if (connectionView_)
  {
    connectionView_->deleteLater();
    connectionView_ = nullptr;
    emit vncWindowClosed();
  }
}

void AppManager::updateWindow()
{
  if (AppManager::instance()->connection()->server()->supportsSetDesktopSize)
  {
    if (connectionView_)
      connectionView_->handleDesktopSize();
  }
}

void AppManager::setWindowName(QString name)
{
  connectionView_->setTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
}

void AppManager::invalidate(int x0, int y0, int x1, int y1)
{
  emit invalidateRequested(x0, y0, x1, y1);
}

void AppManager::refresh()
{
  emit refreshRequested();
}

void AppManager::openContextMenu()
{
  emit contextMenuRequested();
}

void AppManager::openInfoDialog()
{
  visibleInfo_ = true;
  emit visibleInfoChanged();
  emit infoDialogRequested();
}

void AppManager::openOptionDialog()
{
  emit optionDialogRequested();
}

void AppManager::openAboutDialog()
{
  emit aboutDialogRequested();
}

void AppManager::respondToMessage(int response)
{
  emit messageResponded(response);
}

void AppManager::closeOverlay()
{
  visibleInfo_ = false;
  emit visibleInfoChanged();
}
