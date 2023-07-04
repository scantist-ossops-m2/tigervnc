#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QWheelEvent>
#include "vncscrollbar.h"

QVNCScrollBar::QVNCScrollBar(QWidget *parent)
 : QScrollBar(parent)
{
}

QVNCScrollBar::~QVNCScrollBar()
{
}

void QVNCScrollBar::wheelEvent(QWheelEvent *e)
{
  QPoint p = e->position().toPoint();
  QRect rect = geometry();
  bool inside = rect.contains(p);
  if (inside) {
    e->accept();
    QScrollBar::wheelEvent(e);
  }
  else {
    e->ignore();
  }
}
