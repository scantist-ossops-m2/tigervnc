#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "EmulateMB.h"
#include "abstractvncview.h"
#include "appmanager.h"
#include "i18n.h"
#include "locale.h"
#include "menukey.h"
#include "parameters.h"
#include "rfb/LogWriter.h"
#include "rfb/PixelBuffer.h"
#include "rfb/ScreenSet.h"
#include "rfb/ServerParams.h"
#include "vncconnection.h"
#include "vncwindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDebug>
#include <QMenu>
#include <QMoveEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QWindow>
#undef asprintf

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"

#if defined(__APPLE__)
#include "cocoa.h"
#endif

static rfb::LogWriter vlog("VNCView");

QClipboard* QAbstractVNCView::clipboard_ = nullptr;

QAbstractVNCView::QAbstractVNCView(QWidget* parent, Qt::WindowFlags f)
    : QWidget(parent, f), devicePixelRatio_(devicePixelRatioF()), menuKeySym_(XK_F8), contextMenu_(nullptr),
      firstLEDState_(false), pendingServerClipboard_(false), pendingClientClipboard_(false), clipboardSource_(0),
      firstUpdate_(true), delayedFullscreen_(false), delayedDesktopSize_(false), keyboardGrabbed_(false),
      mouseGrabbed_(false), resizeTimer_(new QTimer), delayedInitializeTimer_(new QTimer), fullscreenEnabled_(false),
      pendingFullscreen_(false), mouseButtonEmulationTimer_(new QTimer),
      mbemu_(new EmulateMB(mouseButtonEmulationTimer_)), lastPointerPos_(new rfb::Point), lastButtonMask_(0),
      mousePointerTimer_(new QTimer), menuCtrlKey_(false), menuAltKey_(false)
{
    if (!clipboard_)
    {
        clipboard_ = QGuiApplication::clipboard();
        connect(clipboard_, &QClipboard::dataChanged, this, []() {
            if (!ViewerConfig::config()->sendClipboard())
            {
                return;
            }
            // qDebug() << "QClipboard::dataChanged: owns=" << clipboard_->ownsClipboard() << ", text=" <<
            // clipboard_->text();
            if (!clipboard_->ownsClipboard())
            {
                AppManager::instance()->connection()->announceClipboard(true);
            }
        });
    }
    setContentsMargins(0, 0, 0, 0);

    resizeTimer_->setInterval(100); // <-- DesktopWindow::resize(int x, int y, int w, int h)
    resizeTimer_->setSingleShot(true);
    connect(resizeTimer_, &QTimer::timeout, this, &QAbstractVNCView::handleDesktopSize);

    delayedInitializeTimer_->setInterval(1000);
    delayedInitializeTimer_->setSingleShot(true);
    connect(delayedInitializeTimer_, &QTimer::timeout, this, [this]() {
        AppManager::instance()->connection()->refreshFramebuffer();
        emit delayedInitialized();
    });
    delayedInitializeTimer_->start();

    mouseButtonEmulationTimer_->setInterval(50);
    mouseButtonEmulationTimer_->setSingleShot(true);
    connect(mouseButtonEmulationTimer_, &QTimer::timeout, this, &QAbstractVNCView::handleMouseButtonEmulationTimeout);

    mousePointerTimer_->setInterval(ViewerConfig::config()->pointerEventInterval());
    mousePointerTimer_->setSingleShot(true);
    connect(mousePointerTimer_, &QTimer::timeout, this, [this]() {
        mbemu_->filterPointerEvent(*lastPointerPos_, lastButtonMask_);
    });

    connect(AppManager::instance()->connection(),
            &QVNCConnection::cursorChanged,
            this,
            &QAbstractVNCView::setQCursor,
            Qt::QueuedConnection);
    connect(AppManager::instance()->connection(),
            &QVNCConnection::cursorPositionChanged,
            this,
            &QAbstractVNCView::setCursorPos,
            Qt::QueuedConnection);
    connect(AppManager::instance()->connection(),
            &QVNCConnection::ledStateChanged,
            this,
            &QAbstractVNCView::setLEDState,
            Qt::QueuedConnection);
    connect(AppManager::instance()->connection(),
            &QVNCConnection::clipboardDataReceived,
            this,
            &QAbstractVNCView::handleClipboardData,
            Qt::QueuedConnection);
    connect(AppManager::instance()->connection(),
            &QVNCConnection::bellRequested,
            this,
            &QAbstractVNCView::bell,
            Qt::QueuedConnection);
    connect(AppManager::instance()->connection(),
            &QVNCConnection::refreshFramebufferEnded,
            this,
            &QAbstractVNCView::updateWindow,
            Qt::QueuedConnection);
    connect(AppManager::instance(),
            &AppManager::refreshRequested,
            this,
            &QAbstractVNCView::updateWindow,
            Qt::QueuedConnection);
}

QAbstractVNCView::~QAbstractVNCView()
{
    for (QAction*& action : actions_)
    {
        delete action;
    }
    delete contextMenu_;
    delete resizeTimer_;
    delete delayedInitializeTimer_;
    delete mouseButtonEmulationTimer_;
    delete lastPointerPos_;
    delete mousePointerTimer_;
}

void QAbstractVNCView::postRemoteResizeRequest()
{
    resizeTimer_->start();
}

void QAbstractVNCView::resize(int width, int height)
{
    qDebug() << "QAbstractVNCView::resize: w=" << width << ", h=" << height;
    resizeTimer_->stop();
    width /= effectiveDevicePixelRatio();
    height /= effectiveDevicePixelRatio();
    QWidget::resize(width, height);
    QVNCConnection* cc = AppManager::instance()->connection();
    if (cc->server()->supportsSetDesktopSize)
    {
        handleDesktopSize();
    }
    ungrabPointer();
    grabPointer();
    maybeGrabKeyboard();
    // qDebug() << "QWidget::resize: width=" << width << ", height=" << height;
}

void QAbstractVNCView::handleClipboardData(char const* data)
{
    vlog.debug("Got clipboard data (%d bytes)", (int)strlen(data));
    clipboard_->setText(data);
}

bool QAbstractVNCView::isFullscreenEnabled()
{
    return fullscreenEnabled_;
}

void QAbstractVNCView::remoteResize(int w, int h)
{
    QVNCConnection*                cc = AppManager::instance()->connection();
    rfb::ScreenSet                 layout;
    rfb::ScreenSet::const_iterator iter;
    double                         f      = effectiveDevicePixelRatio();
    QVNCWindow*                    window = AppManager::instance()->window();
    if ((!fullscreenEnabled_ && !pendingFullscreen_) || (w > window->width() * f) || (h > window->height() * f))
    {
        // In windowed mode (or the framebuffer is so large that we need
        // to scroll) we just report a single virtual screen that covers
        // the entire framebuffer.

        layout = cc->server()->screenLayout();

        // Not sure why we have no screens, but adding a new one should be
        // safe as there is nothing to conflict with...
        if (layout.num_screens() == 0)
            layout.add_screen(rfb::Screen());
        else if (layout.num_screens() != 1)
        {
            // More than one screen. Remove all but the first (which we
            // assume is the "primary").

            while (true)
            {
                iter = layout.begin();
                ++iter;

                if (iter == layout.end())
                    break;

                layout.remove_screen(iter->id);
            }
        }

        // Resize the remaining single screen to the complete framebuffer
        layout.begin()->dimensions.tl.x = 0;
        layout.begin()->dimensions.tl.y = 0;
        layout.begin()->dimensions.br.x = w;
        layout.begin()->dimensions.br.y = h;
    }
    else
    {
        uint32_t id;

        // In full screen we report all screens that are fully covered.
        rfb::Rect viewport_rect;
        viewport_rect.setXYWH(fxmin_, fymin_, fw_, fh_);

        // If we can find a matching screen in the existing set, we use
        // that, otherwise we create a brand new screen.
        //
        // FIXME: We should really track screens better so we can handle
        //        a resized one.
        //
        QApplication*   app     = static_cast<QApplication*>(QApplication::instance());
        QList<QScreen*> screens = app->screens();
        //    std::sort(screens.begin(), screens.end(), [](QScreen *a, QScreen *b) {
        //                return a->geometry().x() == b->geometry().x() ? (a->geometry().y() < b->geometry().y()) :
        //                (a->geometry().x() < b->geometry().x());
        //              });
        for (QScreen*& screen : screens)
        {
            double dpr = effectiveDevicePixelRatio(screen);
            QRect  vg  = screen->geometry();
            int    sx  = vg.x();
            int    sy  = vg.y();
            int    sw  = vg.width() * dpr;
            int    sh  = vg.height() * dpr;

            // Check that the screen is fully inside the framebuffer
            rfb::Rect screen_rect;
            screen_rect.setXYWH(sx, sy, sw, sh);
            if (!screen_rect.enclosed_by(viewport_rect))
                continue;

            // Adjust the coordinates so they are relative to our viewport
            sx -= viewport_rect.tl.x;
            sy -= viewport_rect.tl.y;

            // Look for perfectly matching existing screen that is not yet present in
            // in the screen layout...
            for (iter = cc->server()->screenLayout().begin(); iter != cc->server()->screenLayout().end(); ++iter)
            {
                if ((iter->dimensions.tl.x == sx) && (iter->dimensions.tl.y == sy) && (iter->dimensions.width() == sw)
                    && (iter->dimensions.height() == sh)
                    && (std::find(layout.begin(), layout.end(), *iter) == layout.end()))
                    break;
            }

            // Found it?
            if (iter != cc->server()->screenLayout().end())
            {
                layout.add_screen(*iter);
                continue;
            }

            // Need to add a new one, which means we need to find an unused id
            while (true)
            {
                id = rand();
                for (iter = cc->server()->screenLayout().begin(); iter != cc->server()->screenLayout().end(); ++iter)
                {
                    if (iter->id == id)
                        break;
                }

                if (iter == cc->server()->screenLayout().end())
                    break;
            }

            layout.add_screen(rfb::Screen(id, sx, sy, sw, sh, 0));
        }

        // If the viewport doesn't match a physical screen, then we might
        // end up with no screens in the layout. Add a fake one...
        if (layout.num_screens() == 0)
            layout.add_screen(rfb::Screen(0, 0, 0, w, h, 0));
    }

    // Do we actually change anything?
    if ((w == cc->server()->width()) && (h == cc->server()->height()) && (layout == cc->server()->screenLayout()))
        return;

    vlog.debug("Requesting framebuffer resize from %dx%d to %dx%d",
               cc->server()->width(),
               cc->server()->height(),
               w,
               h);

    char buffer[2048];
    layout.print(buffer, sizeof(buffer));
    if (!layout.validate(w, h))
    {
        vlog.error(_("Invalid screen layout computed for resize request!"));
        vlog.error("%s", buffer);
        return;
    }
    else
    {
        vlog.debug("%s", buffer);
    }
    qDebug() << "QAbstractVNCView::remoteResize: w=" << w << ", h=" << h << ", layout=" << buffer;
    emit AppManager::instance()->connection()->writeSetDesktopSize(w, h, layout);
}

// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.
void QAbstractVNCView::updateWindow()
{
    // copied from DesktopWindow.cxx.
    QVNCConnection* cc = AppManager::instance()->connection();
    if (firstUpdate_)
    {
        if (cc->server()->supportsSetDesktopSize)
        {
            // Hack: Wait until we're in the proper mode and position until
            // resizing things, otherwise we might send the wrong thing.
            if (delayedFullscreen_)
                delayedDesktopSize_ = true;
            else
                handleDesktopSize();
        }
        firstUpdate_ = false;
    }
}

void QAbstractVNCView::handleDesktopSize()
{
    double f = effectiveDevicePixelRatio();
    if (!ViewerConfig::config()->desktopSize().isEmpty())
    {
        int w, h;
        // An explicit size has been requested
        if (sscanf(ViewerConfig::config()->desktopSize().toStdString().c_str(), "%dx%d", &w, &h) != 2)
        {
            return;
        }
        remoteResize(w * f, h * f);
        // qDebug() << "QAbstractVNCView::handleDesktopSize(explicit): width=" << w << ", height=" << h;
    }
    else if (ViewerConfig::config()->remoteResize())
    {
        // No explicit size, but remote resizing is on so make sure it
        // matches whatever size the window ended up being
        remoteResize(width() * f, height() * f);
        // qDebug() << "QAbstractVNCView::handleDesktopSize(implicit): width=" << width() << ", height=" << height();
    }
}

QList<int> QAbstractVNCView::fullscreenScreens()
{
    QApplication*   app     = static_cast<QApplication*>(QApplication::instance());
    QList<QScreen*> screens = app->screens();
    QList<int>      applicableScreens;
    if (ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSAll)
    {
        for (int i = 0; i < screens.length(); i++)
        {
            applicableScreens << i;
        }
    }
    else if (ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSSelected)
    {
        for (int& id : ViewerConfig::config()->selectedScreens())
        {
            int i = id - 1; // Screen ID in config is 1-origin.
            if (i < screens.length())
            {
                applicableScreens << i;
            }
        }
    }
    else
    {
        QScreen* cscreen = getCurrentScreen();
        for (int i = 0; i < screens.length(); i++)
        {
            if (screens[i] == cscreen)
            {
                applicableScreens << i;
                break;
            }
        }
    }

    return applicableScreens;
}

void QAbstractVNCView::fullscreen(bool enabled)
{
    QVNCWindow* window = AppManager::instance()->window();
    // qDebug() << "QAbstractVNCView::fullscreen: enabled=" << enabled;
    //  TODO: Flag fullscreenEnabled_ seems have to be disabled before executing fullscreen().
    bool fullscreenEnabled0 = fullscreenEnabled_;
    fullscreenEnabled_      = false;
    pendingFullscreen_      = enabled;
    resizeTimer_->stop();
    QApplication*   app     = static_cast<QApplication*>(QApplication::instance());
    QList<QScreen*> screens = app->screens();
    setWindowManager();
    if (enabled)
    {
        // cf. DesktopWindow::fullscreen_on()
        if (!isFullscreenEnabled())
        {
            geometry_ = window->saveGeometry();
            fscreen_  = getCurrentScreen();
        }

        auto       mode            = ViewerConfig::config()->fullScreenMode();
        QList<int> selectedScreens = fullscreenScreens();
        if (mode != ViewerConfig::FSCurrent && selectedScreens.length() > 0)
        {
            QScreen* selectedPrimaryScreen = screens[selectedScreens[0]];
            int      xmin                  = INT_MAX;
            int      ymin                  = INT_MAX;
            int      xmax                  = INT_MIN;
            int      ymax                  = INT_MIN;
            for (int& screenIndex : selectedScreens)
            {
                QScreen* screen = screens[screenIndex];
                QRect    rect   = screen->geometry();
                double   dpr    = effectiveDevicePixelRatio(screen);
                if (xmin > rect.x())
                {
                    xmin = rect.x();
                }
                if (xmax < rect.x() + rect.width() * dpr)
                {
                    xmax = rect.x() + rect.width() * dpr;
                }
                if (ymin > rect.y())
                {
                    ymin = rect.y();
                }
                if (ymax < rect.y() + rect.height() * dpr)
                {
                    ymax = rect.y() + rect.height() * dpr;
                }
            }
            int w = xmax - xmin;
            int h = ymax - ymin;
            // qDebug() << "Fullsize Geometry=" << QRect(xmin, ymin, w, h);
            //  Capture the fullscreen geometry.
            fxmin_ = xmin;
            fymin_ = ymin;
            fw_    = w;
            fh_    = h;

            if (selectedScreens.length() == 1)
            { // Fullscreen on the selected single display.
                fullscreenOnSelectedDisplay(selectedPrimaryScreen, xmin, ymin, w, h);
            }
            else
            { // Fullscreen on multiple displays.
                fullscreenOnSelectedDisplays(xmin, ymin, w, h);
            }
        }
        else
        { // Fullscreen on the current single display.
            fullscreenOnCurrentDisplay();
        }
    }
    else
    { // Exit fullscreen mode.
        exitFullscreen();
    }
    fullscreenEnabled_ = enabled;
    pendingFullscreen_ = false;
    setFocus();
    window->activateWindow();
    window->raise();

    if (!enabled)
    {
        ViewerConfig::config()->setFullScreen(false);
    }
    if (fullscreenEnabled_ != fullscreenEnabled0)
    {
        emit fullscreenChanged(fullscreenEnabled_);
    }
}

void QAbstractVNCView::fullscreenOnCurrentDisplay()
{
    QVNCWindow* window = AppManager::instance()->window();
    if (bypassWMHintingEnabled())
    {
        window->setWindowFlag(Qt::BypassWindowManagerHint, true);
    }
    QScreen* screen = getCurrentScreen();
    window->windowHandle()->setScreen(screen);
    window->showFullScreen();

    // Capture the fullscreen geometry.
    double dpr = effectiveDevicePixelRatio(screen);
    QRect  vg  = screen->geometry();
    fxmin_     = vg.x();
    fymin_     = vg.y();
    fw_        = vg.width() * dpr;
    fh_        = vg.height() * dpr;

    grabKeyboard();
}

void QAbstractVNCView::fullscreenOnSelectedDisplay(QScreen* screen, int vx, int vy, int, int)
{
    QVNCWindow* window = AppManager::instance()->window();
    if (bypassWMHintingEnabled())
    {
        window->setWindowFlag(Qt::BypassWindowManagerHint, true);
    }
    window->windowHandle()->setScreen(screen);
    window->move(vx, vy);
    window->showFullScreen();
    grabKeyboard();
}

void QAbstractVNCView::fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight)
{
    QVNCWindow* window = AppManager::instance()->window();
    if (bypassWMHintingEnabled())
    {
        window->setWindowFlag(Qt::BypassWindowManagerHint, true);
    }

    window->setWindowFlag(Qt::FramelessWindowHint, true);

    QRect r = getExtendedFrameProperties();
    window->move(vx + r.x(), vy);
    window->resize(vwidth, vheight);
    resize(vwidth, vheight);
    window->showNormal();
    grabKeyboard();
}

void QAbstractVNCView::exitFullscreen()
{
    QVNCWindow* window = AppManager::instance()->window();
    if (bypassWMHintingEnabled())
    {
        window->setWindowFlag(Qt::BypassWindowManagerHint, false);
    }
    window->setWindowFlag(Qt::FramelessWindowHint, false);
    window->setWindowFlag(Qt::Window);
    window->showNormal();
    window->windowHandle()->setScreen(fscreen_);
    window->restoreGeometry(geometry_);
    ungrabKeyboard();
}

void QAbstractVNCView::moveView(int x, int y)
{
    move(x, y);
}

QScreen* QAbstractVNCView::getCurrentScreen()
{
    int    centerX         = x() + width() / 2;
    int    centerY         = y() + height() / 2;
    QPoint globalCursorPos = mapToGlobal(QPoint(centerX, centerY));
    // qDebug() << "QAbstractVNCView::getCurrentScreen: pos=" << globalCursorPos;
    QApplication*   app     = static_cast<QApplication*>(QApplication::instance());
    QList<QScreen*> screens = app->screens();
    for (QScreen*& screen : screens)
    {
        if (screen->geometry().contains(globalCursorPos))
        {
            // qDebug() << "QAbstractVNCView::getCurrentScreen: found screen isPrimary=" << (screen ==
            // app->primaryScreen());
            return screen;
        }
    }
    return screens[0];
}

double QAbstractVNCView::effectiveDevicePixelRatio(QScreen* screen) const
{
    if (screen)
    {
        return screen->devicePixelRatio();
    }
    return devicePixelRatio_;
}
