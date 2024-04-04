#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// clang-format off
// QEvent must be included before X headers to avoid symbol comflicts.
#include <QEvent>
// QAction must be included before X headers to avoid symbol comflicts.
#include <QAction>
// clang-format on

#include "BaseKeyboardHandler.h"
#include "EmulateMB.h"
#include "PlatformPixelBuffer.h"
#include "appmanager.h"
#include "contextmenuactions.h"
#include "i18n.h"
#include "locale.h"
#include "menukey.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/util.h"
#include "vncconnection.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDebug>
#include <QMenu>
#include <QMoveEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QWindow>
#include <QMimeData>
#undef asprintf
#include "abstractvncview.h"
#include "parameters.h"
#include "vncwindow.h"
#undef asprintf

#if defined(WIN32) || defined(__APPLE__)
#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"
#endif

#undef KeyPress

#if defined(__APPLE__)
#include "cocoa.h"
#endif

static rfb::LogWriter vlog("VNCView");

QAbstractVNCView::QAbstractVNCView(QWidget* parent, Qt::WindowFlags f)
  : QWidget(parent, f)
  , mbemu(new EmulateMB)
  , mousePointerTimer(new QTimer)
  , menuKeySym(XK_F8)
  , delayedInitializeTimer(new QTimer)
  , toastTimer(new QTimer(this))
#ifdef QT_DEBUG
  , fpsTimer(this)
#endif
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_AcceptTouchEvents);
  setFocusPolicy(Qt::StrongFocus);
  setContentsMargins(0, 0, 0, 0);

  connect(QGuiApplication::clipboard(), &QClipboard::changed, this, &QAbstractVNCView::handleClipboardChange);

  delayedInitializeTimer->setInterval(1000);
  delayedInitializeTimer->setSingleShot(true);
  connect(delayedInitializeTimer, &QTimer::timeout, this, [this]() {
    AppManager::instance()->getConnection()->refreshFramebuffer();
    emit delayedInitialized();
  });
  delayedInitializeTimer->start();

  mousePointerTimer->setInterval(ViewerConfig::config()->pointerEventInterval());
  mousePointerTimer->setSingleShot(true);

  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::cursorChanged,
          this,
          &QAbstractVNCView::setCursor,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::cursorPositionChanged,
          this,
          &QAbstractVNCView::setCursorPos,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::clipboardRequested,
          this,
          &QAbstractVNCView::handleClipboardRequest,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::clipboardAnnounced,
          this,
          &QAbstractVNCView::handleClipboardAnnounce,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::clipboardDataReceived,
          this,
          &QAbstractVNCView::handleClipboardData,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::bellRequested,
          this,
          &QAbstractVNCView::bell,
          Qt::QueuedConnection);
  connect(AppManager::instance()->getConnection(),
          &QVNCConnection::refreshFramebufferEnded,
          this,
          &QAbstractVNCView::updateWindow,
          Qt::QueuedConnection);
  connect(AppManager::instance(),
          &AppManager::refreshRequested,
          this,
          &QAbstractVNCView::updateWindow,
          Qt::QueuedConnection);

  toastTimer->setInterval(5000);
  toastTimer->setSingleShot(true);
  connect(toastTimer, &QTimer::timeout, this, &QAbstractVNCView::hideToast);
  connect(this, &QAbstractVNCView::delayedInitialized, this, &QAbstractVNCView::showToast);

#ifdef QT_DEBUG
  gettimeofday(&fpsLast, NULL);
  fpsTimer.start(5000);
#endif

  connect(
      this,
      &QAbstractVNCView::bufferResized,
      this,
      [=]() {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        repaint();
      },
      Qt::QueuedConnection);
}

QAbstractVNCView::~QAbstractVNCView()
{
  for (QAction*& action : contextMenuActions) {
    delete action;
  }
  delete contextMenu;
  delete delayedInitializeTimer;
  delete mousePointerTimer;
}

void QAbstractVNCView::toggleContextMenu()
{
  if (isVisibleContextMenu()) {
    contextMenu->hide();
  } else {
    createContextMenu();
    removeKeyboardHandler();
    contextMenu->exec(QCursor::pos());
    contextMenu->setFocus();
  }
}

void QAbstractVNCView::createContextMenu()
{
  if (!contextMenu) {
    contextMenuActions << new QDisconnectAction(p_("ContextMenu|", "Dis&connect"));
    contextMenuActions << new QMenuSeparator();
    contextMenuActions << new QFullScreenAction(p_("ContextMenu|", "&Full screen"));
    contextMenuActions << new QMinimizeAction(p_("ContextMenu|", "Minimi&ze"));
    contextMenuActions << new QRevertSizeAction(p_("ContextMenu|", "Resize &window to session"));
    contextMenuActions << new QMenuSeparator();
    contextMenuActions << new QKeyToggleAction(p_("ContextMenu|", "&Ctrl"), 0x1d, XK_Control_L);
    contextMenuActions << new QKeyToggleAction(p_("ContextMenu|", "&Alt"), 0x38, XK_Alt_L);
    contextMenuActions << new QMenuKeyAction(
        QString::asprintf(p_("ContextMenu|", "Send %s"), ViewerConfig::config()->menuKey().toStdString().c_str()));
    contextMenuActions << new QCtrlAltDelAction(p_("ContextMenu|", "Send Ctrl-Alt-&Del"));
    contextMenuActions << new QMenuSeparator();
    contextMenuActions << new QRefreshAction(p_("ContextMenu|", "&Refresh screen"));
    contextMenuActions << new QMenuSeparator();
    contextMenuActions << new QOptionDialogAction(p_("ContextMenu|", "&Options..."));
    contextMenuActions << new QInfoDialogAction(p_("ContextMenu|", "Connection &info..."));
    contextMenuActions << new QAboutDialogAction(p_("ContextMenu|", "About &TigerVNC viewer..."));
    contextMenu = new QMenu;
#if defined(__APPLE__)
    contextMenu->setAttribute(Qt::WA_NativeWindow);
    cocoa_set_overlay_property(contextMenu->winId());
#endif
    for (QAction*& action : contextMenuActions) {
      contextMenu->addAction(action);
    }
    contextMenu->installEventFilter(this);
    connect(contextMenu, &QMenu::aboutToHide, this, &QAbstractVNCView::installKeyboardHandler, Qt::QueuedConnection);
  }
}

bool QAbstractVNCView::isVisibleContextMenu() const
{
  return contextMenu && contextMenu->isVisible();
}

void QAbstractVNCView::sendContextMenuKey()
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  qDebug() << "QAbstractVNCView::sendContextMenuKey";
  int dummy;
  int keyCode;
  quint32 keySym;
  ::getMenuKey(&dummy, &keyCode, &keySym);
  keyboardHandler->handleKeyPress(keyCode, keySym, true);
  keyboardHandler->handleKeyRelease(keyCode);
  contextMenu->hide();
}

void QAbstractVNCView::sendCtrlAltDel()
{
  keyboardHandler->handleKeyPress(0x1d, XK_Control_L);
  keyboardHandler->handleKeyPress(0x38, XK_Alt_L);
  keyboardHandler->handleKeyPress(0xd3, XK_Delete);
  keyboardHandler->handleKeyRelease(0xd3);
  keyboardHandler->handleKeyRelease(0x38);
  keyboardHandler->handleKeyRelease(0x1d);
}

void QAbstractVNCView::toggleKey(bool toggle, int keyCode, quint32 keySym)
{
  if (toggle) {
    keyboardHandler->handleKeyPress(keyCode, keySym);
  } else {
    keyboardHandler->handleKeyRelease(keyCode);
  }
  keyboardHandler->setMenuKeyStatus(keySym, toggle);
}

// As QMenu eventFilter
bool QAbstractVNCView::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent* e = static_cast<QKeyEvent*>(event);
    if (isVisibleContextMenu()) {
      if (QKeySequence(e->key()).toString() == ViewerConfig::config()->menuKey()) {
        toggleContextMenu();
        return true;
      }
    }
  }
  return QWidget::eventFilter(obj, event);
}

void QAbstractVNCView::resize(int width, int height)
{
  qDebug() << "QAbstractVNCView::resize: w=" << width << ", h=" << height;
  QWidget::resize(width, height);
}

void QAbstractVNCView::initKeyboardHandler()
{
  installKeyboardHandler();
  connect(AppManager::instance(),
          &AppManager::vncWindowClosed,
          this,
          &QAbstractVNCView::removeKeyboardHandler,
          Qt::QueuedConnection);
  connect(
      AppManager::instance()->getConnection(),
      &QVNCConnection::ledStateChanged,
      this,
      [=](unsigned int state) {
        qDebug() << "QVNCConnection::ledStateChanged";
        // The first message is just considered to be the server announcing
        // support for this extension. We will push our state to sync up the
        // server when we get focus. If we already have focus we need to push
        // it here though.
        if (firstLEDState) {
          firstLEDState = false;
          if (hasFocus()) {
            qDebug() << "pushLEDState";
            keyboardHandler->pushLEDState();
          }
        } else if (hasFocus()) {
          qDebug() << "setLEDState";
          keyboardHandler->setLEDState(state);
        }
      },
      Qt::QueuedConnection);
  connect(keyboardHandler,
          &BaseKeyboardHandler::contextMenuKeyPressed,
          this,
          &QAbstractVNCView::toggleContextMenu,
          Qt::QueuedConnection);
}

void QAbstractVNCView::installKeyboardHandler()
{
  qDebug() << "QAbstractVNCView::installKeyboardHandler";
  QAbstractEventDispatcher::instance()->installNativeEventFilter(keyboardHandler);
}

void QAbstractVNCView::removeKeyboardHandler()
{
  qDebug() << "QAbstractVNCView::removeKeyboardHandler";
  QAbstractEventDispatcher::instance()->removeNativeEventFilter(keyboardHandler);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
QScreen* QAbstractVNCView::screen() const
{
  // FIXME: check best overlap
  return qApp->screens()[0];
}
#endif

void QAbstractVNCView::resetKeyboard()
{
  if (keyboardHandler)
    keyboardHandler->resetKeyboard();
}

void QAbstractVNCView::setCursorPos(int, int)
{
  qDebug() << "QAbstractVNCView::setCursorPos";
}

void QAbstractVNCView::flushPendingClipboard()
{
  if (pendingServerClipboard) {
    vlog.debug("Focus regained after remote clipboard change, requesting data");
    AppManager::instance()->getConnection()->requestClipboard();
  }

  if (pendingClientClipboard) {
    vlog.debug("Focus regained after local clipboard change, notifying server");
    AppManager::instance()->getConnection()->announceClipboard(true);
  }

  pendingServerClipboard = false;
  pendingClientClipboard = false;
}

void QAbstractVNCView::handleClipboardRequest()
{
  qDebug() << "QAbstractVNCView::handleClipboardRequest" << pendingClientData;
  AppManager::instance()->getConnection()->sendClipboardData(pendingClientData);
  pendingClientData = "";
}

void QAbstractVNCView::handleClipboardChange(QClipboard::Mode mode)
{
  qDebug() << "QAbstractVNCView::handleClipboardChange:" << mode << QGuiApplication::clipboard()->text(mode);
  qDebug() << "QAbstractVNCView::handleClipboardChange: ownsClipboard=" << QGuiApplication::clipboard()->ownsClipboard();
  qDebug() << "QAbstractVNCView::handleClipboardChange: hasText=" << QGuiApplication::clipboard()->mimeData(mode)->hasText();

  if (!ViewerConfig::config()->sendClipboard()) {
    return;
  }

  if (mode == QClipboard::Mode::Selection && !ViewerConfig::config()->sendPrimary()) {
    return;
  }

  if(mode == QClipboard::Mode::Clipboard && QGuiApplication::clipboard()->ownsClipboard()) {
    return;
  }

  if(mode == QClipboard::Mode::Selection && QGuiApplication::clipboard()->ownsSelection()) {
    return;
  }

  if (!QGuiApplication::clipboard()->mimeData(mode)->hasText()) {
    return;
  }

  pendingServerClipboard = false;
  pendingClientData = QGuiApplication::clipboard()->text(mode);

  if (!hasFocus()) {
    vlog.debug("Local clipboard changed whilst not focused, will notify server later");
    pendingClientClipboard = true;
    // Clear any older client clipboard from the server
    AppManager::instance()->getConnection()->announceClipboard(false);
    return;
  }

  vlog.debug("Local clipboard changed, notifying server");
  AppManager::instance()->getConnection()->announceClipboard(true);
}

void QAbstractVNCView::handleClipboardAnnounce(bool available)
{
  qDebug() << "QAbstractVNCView::handleClipboardAnnounce" << available;

  if (!ViewerConfig::config()->acceptClipboard())
    return;

  if (!available) {
    vlog.debug("Clipboard is no longer available on server");
    pendingServerClipboard = false;
    return;
  }

  pendingClientClipboard = false;
  pendingClientData = "";

  if (!hasFocus()) {
    vlog.debug("Got notification of new clipboard on server whilst not focused, will request data later");
    pendingServerClipboard = true;
    return;
  }

  vlog.debug("Got notification of new clipboard on server, requesting data");
  QVNCConnection* cc = AppManager::instance()->getConnection();
  cc->requestClipboard();
}

void QAbstractVNCView::handleClipboardData(const char* data)
{
  qDebug() << "QAbstractVNCView::handleClipboardData" << data;
  vlog.debug("Got clipboard data (%d bytes)", (int)strlen(data));
  QGuiApplication::clipboard()->setText(data);
  if (ViewerConfig::config()->shouldSetPrimary())
    QGuiApplication::clipboard()->setText(data, QClipboard::Mode::Selection);
}

void QAbstractVNCView::maybeGrabKeyboard()
{
  QVNCWindow* window = AppManager::instance()->getWindow();
  if (ViewerConfig::config()->fullscreenSystemKeys() && window->allowKeyboardGrab() && hasFocus()) {
    grabKeyboard();
  }
}

void QAbstractVNCView::grabKeyboard()
{
  keyboardHandler->grabKeyboard();

  QPoint gpos = QCursor::pos();
  QPoint lpos = mapFromGlobal(gpos);
  QRect r = rect();
  if (r.contains(lpos)) {
    grabPointer();
  }
}

void QAbstractVNCView::ungrabKeyboard()
{
  if (keyboardHandler)
    keyboardHandler->ungrabKeyboard();
}

void QAbstractVNCView::grabPointer()
{
  activateWindow();
  setMouseTracking(true);
  mouseGrabbed = true;
}

void QAbstractVNCView::ungrabPointer()
{
  setMouseTracking(false);
  mouseGrabbed = false;
}

void QAbstractVNCView::showToast()
{
  qDebug() << "QAbstractVNCView::showToast" << toastGeometry();
  toastTimer->start();
  damage += toastGeometry(); // xTODO
  update(localRectAdjust(damage.boundingRect()));
}

void QAbstractVNCView::hideToast()
{
  qDebug() << "QAbstractVNCView::hideToast";
  toastTimer->stop();
  damage += toastGeometry(); // xTODO
  update(localRectAdjust(damage.boundingRect()));
}

QFont QAbstractVNCView::toastFont() const
{
  QFont f;
  f.setBold(true);
  f.setPixelSize(14);
  return f;
}

QString QAbstractVNCView::toastText() const
{
  return QString::asprintf(_("Press %s to open the context menu"),
                           ViewerConfig::config()->menuKey().toStdString().c_str());
}

QRect QAbstractVNCView::toastGeometry() const
{
  QFontMetrics fm(toastFont());
  int b = 8;
  QRect r = fm.boundingRect(toastText()).adjusted(-2 * b, -2 * b, 2 * b, 2 * b);

  int x = (width() - r.width()) / 2;
  int y = 50;
  return QRect(QPoint(x, y), r.size());
}

QRect QAbstractVNCView::localRectAdjust(QRect r)
{
  return r.adjusted((width() - pixmap.width()) / 2,
                    (height() - pixmap.height()) / 2,
                    (width() - pixmap.width()) / 2,
                    (height() - pixmap.height()) / 2);
}

QRect QAbstractVNCView::remoteRectAdjust(QRect r)
{
  return r.adjusted(-(width() - pixmap.width()) / 2,
                    -(height() - pixmap.height()) / 2,
                    -(width() - pixmap.width()) / 2,
                    -(height() - pixmap.height()) / 2);
}

rfb::Point QAbstractVNCView::remotePointAdjust(const rfb::Point& pos)
{
  return rfb::Point(pos.x - (width() - pixmap.width()) / 2, pos.y - (height() - pixmap.height()) / 2);
}

// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.
void QAbstractVNCView::updateWindow()
{
  // copied from DesktopWindow.cxx.
  QVNCConnection* cc = AppManager::instance()->getConnection();
  if (firstUpdate) {
    if (cc->server()->supportsSetDesktopSize) {
      emit remoteResizeRequest();
    }
    firstUpdate = false;
  }

  PlatformPixelBuffer* framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  rfb::Rect rect = framebuffer->getDamage();
  int x = rect.tl.x;
  int y = rect.tl.y;
  int w = rect.br.x - x;
  int h = rect.br.y - y;
  if (!rect.is_empty()) {
    damage += QRect(x, y, w, h);
    update(localRectAdjust(QRect(x, y, w, h)));
  }
}

void QAbstractVNCView::paintEvent(QPaintEvent* event)
{
  QVNCConnection* cc = AppManager::instance()->getConnection();
  PlatformPixelBuffer* framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());

  if ((framebuffer->width() != pixmap.width()) || (framebuffer->height() != pixmap.height())) {
    pixmap = QPixmap(framebuffer->width(), framebuffer->height());
    damage = QRegion(0, 0, pixmap.width(), pixmap.height());
    resize(pixmap.width(), pixmap.height());
    qDebug() << "bufferResized" << pixmap.size() << size();
    emit bufferResized();
  }

  if (!damage.isEmpty()) {
    QPainter pixmapPainter(&pixmap);
    const uint8_t* data;
    int stride;
    QRect bounds = damage.boundingRect();
    int x = bounds.x();
    int y = bounds.y();
    int w = bounds.width();
    int h = bounds.height();
    rfb::Rect rfbrect(x, y, x + w, y + h);

    if (rfbrect.enclosed_by(framebuffer->getRect())) {
      data = framebuffer->getBuffer(rfbrect, &stride);
      QImage image(data, w, h, stride * 4, QImage::Format_RGB32);

      pixmapPainter.drawImage(bounds, image);
    }
    damage = QRegion();
  }

  QPainter painter(this);
  QRect r = event->rect();

  painter.drawPixmap(r, pixmap, remoteRectAdjust(r));

  if (toastTimer->isActive()) {
    painter.setFont(toastFont());
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor("#96101010"));
    painter.drawRoundedRect(toastGeometry(), 15, 15, Qt::AbsoluteSize);
    QPen p;
    p.setColor("#e0ffffff");
    painter.setPen(p);
    painter.drawText(toastGeometry(), toastText(), QTextOption(Qt::AlignCenter));
  }

#ifdef QT_DEBUG
  fpsCounter++;
  painter.setFont(toastFont());
  painter.setPen(Qt::NoPen);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor("#96101010"));
  painter.drawRect(fpsRect);
  QPen p;
  p.setColor("#e0ffffff");
  painter.setPen(p);
  QString text = QString("%1 fps").arg(fpsValue);
  painter.drawText(fpsRect, text, QTextOption(Qt::AlignCenter));

  painter.setBrush(Qt::NoBrush);
  painter.setPen(Qt::red);
  painter.drawRect(rect());
#endif

  setAttribute(Qt::WA_OpaquePaintEvent, true);
}

#ifdef QT_DEBUG
bool QAbstractVNCView::handleTimeout(rfb::Timer* t)
{
  struct timeval now;
  int count;

  gettimeofday(&now, NULL);
  count = fpsCounter;

  fpsValue = int(count * 1000.0 / rfb::msSince(&fpsLast));

  vlog.info("%d frames in %g seconds = %d FPS", count, rfb::msSince(&fpsLast) / 1000.0, fpsValue);

  fpsCounter -= count;
  fpsLast = now;

  damage += fpsRect;
  update(damage);

  return true;
}
#endif

void QAbstractVNCView::getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask, int& wheelMask)
{
  buttonMask = 0;
  wheelMask = 0;
  if (event->buttons() & Qt::LeftButton) {
    buttonMask |= 1;
  }
  if (event->buttons() & Qt::MiddleButton) {
    buttonMask |= 2;
  }
  if (event->buttons() & Qt::RightButton) {
    buttonMask |= 4;
  }
  if (event->buttons() & Qt::XButton1) {
    wheelMask |= 32;
  }
  if (event->buttons() & Qt::XButton2) {
    wheelMask |= 64;
  }

  x = event->x();
  y = event->y();
}

void QAbstractVNCView::getMouseProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask)
{
  buttonMask = 0;
  wheelMask = 0;
  if (event->buttons() & Qt::LeftButton) {
    buttonMask |= 1;
  }
  if (event->buttons() & Qt::MiddleButton) {
    buttonMask |= 2;
  }
  if (event->buttons() & Qt::RightButton) {
    buttonMask |= 4;
  }
  if (event->buttons() & Qt::XButton1) {
    wheelMask |= 32;
  }
  if (event->buttons() & Qt::XButton2) {
    wheelMask |= 64;
  }
  if (event->delta() > 0) {
    wheelMask |= 8;
  }
  if (event->delta() < 0) {
    wheelMask |= 16;
  }

  x = event->x();
  y = event->y();
}

void QAbstractVNCView::mouseMoveEvent(QMouseEvent* event)
{
  // qDebug() << "QAbstractVNCView::mousePressEvent" << event->x() << event->y();
  grabPointer();
  maybeGrabKeyboard();
  int x, y, buttonMask, wheelMask;
  getMouseProperties(event, x, y, buttonMask, wheelMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask | wheelMask);
}

void QAbstractVNCView::mousePressEvent(QMouseEvent* event)
{
  qDebug() << "QAbstractVNCView::mousePressEvent";

  if (ViewerConfig::config()->viewOnly()) {
    return;
  }

  setFocus(Qt::FocusReason::MouseFocusReason);

  int x, y, buttonMask, wheelMask;
  getMouseProperties(event, x, y, buttonMask, wheelMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask);

  grabPointer();
  maybeGrabKeyboard();
}

void QAbstractVNCView::mouseReleaseEvent(QMouseEvent* event)
{
  qDebug() << "QAbstractVNCView::mouseReleaseEvent";

  if (ViewerConfig::config()->viewOnly()) {
    return;
  }

  setFocus(Qt::FocusReason::MouseFocusReason);

  int x, y, buttonMask, wheelMask;
  getMouseProperties(event, x, y, buttonMask, wheelMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask);

  grabPointer();
  maybeGrabKeyboard();
}

void QAbstractVNCView::wheelEvent(QWheelEvent* event)
{
  qDebug() << "QAbstractVNCView::wheelEvent";

  int x, y, buttonMask, wheelMask;
  getMouseProperties(event, x, y, buttonMask, wheelMask);
  if (wheelMask) {
    filterPointerEvent(rfb::Point(x, y), buttonMask | wheelMask);
  }
  filterPointerEvent(rfb::Point(x, y), buttonMask);
  event->accept();
}

void QAbstractVNCView::focusInEvent(QFocusEvent* event)
{
  qDebug() << "QAbstractVNCView::focusInEvent";
  if (keyboardHandler) {
    maybeGrabKeyboard();

    flushPendingClipboard();

    // We may have gotten our lock keys out of sync with the server
    // whilst we didn't have focus. Try to sort this out.
    qDebug() << "pushLEDState";
    keyboardHandler->pushLEDState();

    // Resend Ctrl/Alt if needed
    if (keyboardHandler->getMenuCtrlKey()) {
      keyboardHandler->handleKeyPress(0x1d, XK_Control_L);
    }
    if (keyboardHandler->getMenuAltKey()) {
      keyboardHandler->handleKeyPress(0x38, XK_Alt_L);
    }
  }
  QWidget::focusInEvent(event);
}

void QAbstractVNCView::focusOutEvent(QFocusEvent* event)
{
  qDebug() << "QAbstractVNCView::focusOutEvent";
  if (ViewerConfig::config()->fullscreenSystemKeys()) {
    ungrabKeyboard();
  }
  // We won't get more key events, so reset our knowledge about keys
  resetKeyboard();
  QWidget::focusOutEvent(event);
}

void QAbstractVNCView::resizeEvent(QResizeEvent* event)
{
  qDebug() << "QAbstractVNCView::resizeEvent" << event->size();
  // Some systems require a grab after the window size has been changed.
  // Otherwise they might hold on to displays, resulting in them being unusable.
  grabPointer();
  maybeGrabKeyboard();
}

bool QAbstractVNCView::event(QEvent *event)
{
  switch (event->type()) {
  case QEvent::WindowActivate:
    // qDebug() << "WindowActivate";
    grabPointer();
    break;
  case QEvent::WindowDeactivate:
    // qDebug() << "WindowDeactivate";
    ungrabPointer();
    break;
  case QEvent::CursorChange:
    // qDebug() << "CursorChange";
    event->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    return true;
  }
  return QWidget::event(event);
}

void QAbstractVNCView::filterPointerEvent(const rfb::Point& pos, int mask)
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  bool instantPosting = ViewerConfig::config()->pointerEventInterval() == 0 || (mask != lastButtonMask);
  rfb::Point pointerPos = remotePointAdjust(pos);
  lastButtonMask = mask;
  if (instantPosting) {
    mbemu->filterPointerEvent(pointerPos, mask);
  } else {
    if (!mousePointerTimer->isActive()) {
      mbemu->filterPointerEvent(pointerPos, mask);
      mousePointerTimer->start();
    }
  }
}
