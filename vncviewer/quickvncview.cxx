#include "quickvncview.h"

#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"
#include "rfb/LogWriter.h"

#include <QApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>

static rfb::LogWriter vlog("QuickVNCView");

QuickVNCView::QuickVNCView(QWindow* parent) : QQuickView(QUrl("qrc:/qml/ConnectionView.qml"), parent)
{
}

void QuickVNCView::remoteResize(int w, int h)
{
  QVNCConnection*                cc = AppManager::instance()->connection();
  rfb::ScreenSet                 layout;
  rfb::ScreenSet::const_iterator iter;
  double                         f = effectiveDevicePixelRatio();
  if (!AppManager::instance()->isFullscreen() || (w > width() * f) || (h > height() * f))
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
      double dpr = screen->devicePixelRatio();
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
            && (iter->dimensions.height() == sh) && (std::find(layout.begin(), layout.end(), *iter) == layout.end()))
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

  vlog.debug("Requesting framebuffer resize from %dx%d to %dx%d", cc->server()->width(), cc->server()->height(), w, h);

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

void QuickVNCView::handleDesktopSize()
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
    // qDebug() << "QuickVNCView::handleDesktopSize(explicit): width=" << w << ", height=" << h;
  }
  else if (ViewerConfig::config()->remoteResize())
  {
    // No explicit size, but remote resizing is on so make sure it
    // matches whatever size the window ended up being
    remoteResize(width() * f, height() * f);
    // qDebug() << "QuickVNCView::handleDesktopSize(implicit): width=" << width() << ", height=" << height();
  }
}

QList<int> QuickVNCView::fullscreenScreens()
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
    QScreen* cscreen = screen();
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

void QuickVNCView::fullscreen(bool enabled)
{
  // qDebug() << "QuickVNCView::fullscreen: enabled=" << enabled;
  QApplication*   app     = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  if (enabled)
  {
    // cf. DesktopWindow::fullscreen_on()
    if (AppManager::instance()->isFullscreen())
    {
      previousGeometry_ = geometry();
      previousScreen_   = screen();
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
        double   dpr    = screen->devicePixelRatio();
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
        fullscreenOnSelectedDisplay(selectedPrimaryScreen);
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

  if (!enabled)
  {
    ViewerConfig::config()->setFullScreen(false);
  }
}

void QuickVNCView::fullscreenOnCurrentDisplay()
{
  showFullScreen();
}

void QuickVNCView::fullscreenOnSelectedDisplay(QScreen* screen)
{
  setScreen(screen);
  showFullScreen();
}

void QuickVNCView::fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight)
{
  setFlags(flags() | Qt::BypassWindowManagerHint | Qt::FramelessWindowHint);
  setX(vx);
  setY(vy);
  resize(vwidth, vheight);
  showNormal();
}

void QuickVNCView::exitFullscreen()
{
  setFlags(flags() & ~Qt::BypassWindowManagerHint & ~Qt::FramelessWindowHint);
  if (previousScreen_)
  {
    setScreen(previousScreen_);
    setGeometry(previousGeometry_);
  }
  showNormal();
}
