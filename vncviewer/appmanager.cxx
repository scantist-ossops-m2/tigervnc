#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QDebug>
#include <QProcess>
#include <QQmlEngine>
#include <QScreen>
#include <QTcpSocket>
#include <QTimer>
#if defined(Q_OS_UNIX)
#include <QApplication>
#endif

#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"
#include "quickvncitem.h"
#include "rdr/Exception.h"
#include "rfb/Timer.h"
#include "vncconnection.h"
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
        rfbTimerProxy_->setInterval(rfb::Timer::checkTimeouts());
    });
    rfbTimerProxy_->setSingleShot(false);
    rfbTimerProxy_->setInterval(0);
    rfbTimerProxy_->start();
}

AppManager::~AppManager()
{
    facade_->deleteLater();
    connectionView_->deleteLater();
    rfbTimerProxy_->deleteLater();
}

int AppManager::initialize()
{
    qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
    qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    qmlRegisterType<QVNCConnection>("Qt.TigerVNC", 1, 0, "VNCConnection");
    manager_ = new AppManager();
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
    connectionView_ = new QQuickView(QUrl("qrc:/qml/ConnectionView.qml"));
    connectionView_->resize(width, height);
    if (!ViewerConfig::config()->remoteResize())
    {
        // xTODO
    }
    remoteViewWidth_  = width;
    remoteViewHeight_ = height;
    emit remoteViewSizeChanged(remoteViewWidth_, remoteViewHeight_);
    connectionView_->setTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
    connectionView_->show();

    if (ViewerConfig::config()->fullScreen())
    {
        connectionView_->showFullScreen();
    }

    emit vncWindowOpened();
}

void AppManager::closeVNCWindow()
{
    connectionView_->deleteLater();
    connectionView_ = nullptr;
    emit vncWindowClosed();
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
