#ifndef QUICKVNCVIEW_H
#define QUICKVNCVIEW_H

#include <QQuickView>

class QuickVNCView : public QQuickView
{
  Q_OBJECT

public:
  QuickVNCView(QQmlEngine* engine, QWindow* parent = nullptr);
  ~QuickVNCView();

  void       remoteResize(int width, int height);
  void       handleDesktopSize();
  QList<int> fullscreenScreens();
  void       fullscreen(bool enabled);

  void fullscreenOnCurrentDisplay();
  void fullscreenOnSelectedDisplay(QScreen* screen);
  void fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight);
  void exitFullscreen();

protected:
  void hideEvent(QHideEvent* event);

private:
  int      fxmin_ = 0;
  int      fymin_ = 0;
  int      fw_    = 0;
  int      fh_    = 0;
  QRect    previousGeometry_;
  QScreen* previousScreen_ = nullptr;
};

#endif // QUICKVNCVIEW_H
