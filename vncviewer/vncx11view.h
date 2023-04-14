#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include <X11/Xlib.h>
#include "abstractvncview.h"

namespace rfb {
  class Rect;
}

class QVNCX11View : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCX11View(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCX11View();
  qulonglong nativeWindowHandle() const override;

public slots:
  void bell() override;

protected:
  bool event(QEvent *e) override;
  bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;

signals:
  void message(const QString &msg, int timeout);

private:
  Window m_window;
  rfb::Rect *m_rect;

  void fixParent();
};

#endif // VNCX11VIEW_H
