#ifndef VNCSCROLLBAR_H
#define VNCSCROLLBAR_H

#include <QScrollBar>

class QVNCScrollBar : public QScrollBar
{
  Q_OBJECT
public:
  QVNCScrollBar(QWidget *parent = nullptr);
  virtual ~QVNCScrollBar();

protected:
  void wheelEvent(QWheelEvent *e) override;
};

#endif // VNCSCROLLBAR_H
