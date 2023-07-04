#ifndef VNCWINDOW_H
#define VNCWINDOW_H

#include <QScrollArea>

class QMoveEvent;
class QResizeEvent;
class QVNCToast;
class QVNCScrollBar;

class QVNCWindow : public QScrollArea
{
  Q_OBJECT
public:
  QVNCWindow(QWidget *parent = nullptr);
  virtual ~QVNCWindow();
  void resize(int width, int height);
  void normalizedResize(int width, int height);

public slots:
  void popupToast();

protected:
  void moveEvent(QMoveEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  void changeEvent(QEvent *e) override;
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;

private:
  QVNCToast *toast_;
#if !defined(WIN32)
  QVNCScrollBar *hscroll_;
  QVNCScrollBar *vscroll_;
#endif
};

#endif // VNCWINDOW_H
