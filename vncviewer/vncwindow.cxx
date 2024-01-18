#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "abstractvncview.h"
#include "i18n.h"
#include "parameters.h"
#include "rfb/LogWriter.h"
#include "vnctoast.h"
#include "vncwindow.h"

#include <QDebug>
#include <QGestureEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QResizeEvent>
#undef asprintf
#if defined(WIN32)
#include <windows.h>
#endif
#if !defined(WIN32)
#include "vncscrollbar.h"
#endif

static rfb::LogWriter vlog("VNCWindow");

QVNCWindow::QVNCWindow(QWidget* parent)
    : QScrollArea(parent), toast_(new QVNCToast(this))
#if !defined(WIN32)
      ,
      hscroll_(new QVNCScrollBar), vscroll_(new QVNCScrollBar)
#endif
{
    setAttribute(Qt::WA_InputMethodTransparent);
    setAttribute(Qt::WA_NativeWindow);
    setFocusPolicy(Qt::StrongFocus);

    setWidgetResizable(ViewerConfig::config()->remoteResize());
    setContentsMargins(0, 0, 0, 0);
    setFrameStyle(QFrame::NoFrame);
    setLineWidth(0);
    setViewportMargins(0, 0, 0, 0);

    QPalette p(palette());
    p.setColor(QPalette::Window, QColor::fromRgb(40, 40, 40));
    setPalette(p);
    setBackgroundRole(QPalette::Window);

    setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

#if !defined(WIN32)
    setHorizontalScrollBar(hscroll_);
    setVerticalScrollBar(vscroll_);
#endif
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Support for -geometry option. Note that although we do support
    // negative coordinates, we do not support -XOFF-YOFF (ie
    // coordinates relative to the right edge / bottom edge) at this
    // time.
    int geom_x = 0, geom_y = 0;
    if (!ViewerConfig::config()->geometry().isEmpty())
    {
        int nfields =
            sscanf((char const*)ViewerConfig::config()->geometry().toStdString().c_str(), "+%d+%d", &geom_x, &geom_y);
        if (nfields != 2)
        {
            int geom_w, geom_h;
            nfields = sscanf((char const*)ViewerConfig::config()->geometry().toStdString().c_str(),
                             "%dx%d+%d+%d",
                             &geom_w,
                             &geom_h,
                             &geom_x,
                             &geom_y);
            if (nfields != 4)
            {
                vlog.error(_("Invalid geometry specified!"));
            }
        }
        if (nfields == 2 || nfields == 4)
        {
            move(geom_x, geom_y);
        }
    }
}

QVNCWindow::~QVNCWindow()
{
    delete toast_;
#if !defined(WIN32) && !defined(__APPLE__)
    hscroll_->deleteLater();
    vscroll_->deleteLater();
#endif
}

void QVNCWindow::popupToast()
{
    toast_->move((width() - toast_->width()) / 2, 50);
    toast_->show();
    toast_->raise();
}

void QVNCWindow::moveEvent(QMoveEvent* e)
{
    QScrollArea::moveEvent(e);
}

void QVNCWindow::resizeEvent(QResizeEvent* e)
{
    qDebug() << "QVNCWindow::resizeEvent: w=" << e->size().width() << ", h=" << e->size().height()
             << ", widgetResizable=" << widgetResizable();
    if (ViewerConfig::config()->remoteResize())
    {
        QSize size = e->size();
        widget()->resize(size.width(), size.height());
    }
    else
    {
        scrollContentsBy(0, 0);
        QSize size = e->size();
        if (widget()->size().width() > size.width())
        {
            setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        }
        else
        {
            setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
        if (widget()->size().height() > size.height())
        {
            setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        }
        else
        {
            setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }
    QScrollArea::resizeEvent(e);
}

void QVNCWindow::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::WindowStateChange)
    {
        if (ViewerConfig::config()->remoteResize())
        {
            qDebug() << "QVNCWindow::changeEvent: w=" << width() << ",h=" << height() << ",state=" << windowState()
                     << ",oldState=" << (static_cast<QWindowStateChangeEvent*>(e))->oldState();
            widget()->resize(width(), height());
        }
    }
    QScrollArea::changeEvent(e);
}

void QVNCWindow::focusInEvent(QFocusEvent*)
{
    qDebug() << "QVNCWindow::focusInEvent";
    QAbstractVNCView* view = (QAbstractVNCView*)widget();
    view->maybeGrabKeyboard();
}

void QVNCWindow::focusOutEvent(QFocusEvent*)
{
    qDebug() << "QVNCWindow::focusOutEvent";
    if (ViewerConfig::config()->fullscreenSystemKeys())
    {
        QAbstractVNCView* view = (QAbstractVNCView*)widget();
        view->ungrabKeyboard();
    }
}

void QVNCWindow::resize(int width, int height)
{
    qDebug() << "QVNCWindow::resize: w=" << width << ", h=" << height;
    if (widgetResizable())
    {
#if !defined(__APPLE__)
        double dpr = devicePixelRatioF();
        width *= dpr;
        height *= dpr;
        widget()->resize(width, height);
#endif
    }
    QScrollArea::resize(width, height);
}

void QVNCWindow::normalizedResize(int width, int height)
{
#if !defined(__APPLE__)
    double dpr = devicePixelRatioF();
    width /= dpr;
    height /= dpr;
#endif
    QScrollArea::resize(width, height);
}