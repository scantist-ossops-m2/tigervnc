#include <QEvent>
#include <QTextStream>
#include <QDataStream>
#include <QUrl>
#if defined(WIN32)
#include <qt_windows.h>
#endif
#include "appmanager.h"
#include "vncconnection.h"
#include "PlatformPixelBuffer.h"
#include "vncx11view.h"

#include <X11/Xlib.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif

#include <QDebug>

/*!
    \class VNCX11view qwinhost.h
    \brief The VNCX11view class provides an API to use native Win32
    windows in Qt applications.

    VNCX11view exists to provide a QWidget that can act as a parent for
    any native Win32 control. Since VNCX11view is a proper QWidget, it
    can be used as a toplevel widget (e.g. 0 parent) or as a child of
    any other QWidget.

    VNCX11view integrates the native control into the Qt user interface,
    e.g. handles focus switches and laying out.

    Applications moving to Qt may have custom Win32 controls that will
    take time to rewrite with Qt. Such applications can use these
    custom controls as children of VNCX11view widgets. This allows the
    application's user interface to be replaced gradually.

    When the VNCX11view is destroyed, and the Win32 window hasn't been
    set with setWindow(), the window will also be destroyed.
*/

/*!
    Creates an instance of VNCX11view. \a parent and \a f are
    passed on to the QWidget constructor. The widget has by default
    no background.

    \warning You cannot change the parent widget of the VNCX11view instance
    after the native window has been created, i.e. do not call
    QWidget::setParent or move the VNCX11view into a different layout.
*/
QVNCX11View::QVNCX11View(QWidget *parent, Qt::WindowFlags f)
  : QAbstractVNCView(parent, f)
  , m_window(0)
  , m_rect(new rfb::Rect)
{
  setAttribute(Qt::WA_NoBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setFocusPolicy(Qt::StrongFocus);
  connect(AppManager::instance(), &AppManager::updateRequested, this, [this](int x0, int y0, int x1, int y1) {
    m_rect->union_boundary(rfb::Rect(x0, y0, x1, y1));
  });
}

/*!
    Destroys the VNCX11view object. If the hosted Win32 window has not
    been set explicitly using setWindow() the window will be
    destroyed.
*/
QVNCX11View::~QVNCX11View()
{
  delete m_rect;
}

qulonglong QVNCX11View::nativeWindowHandle() const
{
  return (qulonglong)m_window;
}

void QVNCX11View::fixParent()
{
  Display *display = QX11Info::display();
  XReparentWindow(display, m_window, winId(), 0, 0);
  XMapWindow(display, m_window); // must be performed after XReparentWindow,
}

/*!
    \reimp
*/
bool QVNCX11View::event(QEvent *e)
{
  switch(e->type()) {
    case QEvent::Polish:
      if (!m_window) {
        qDebug() << "display numbers:  QX11Info::display()=" <<  QX11Info::display() << ", XOpenDisplay(NULL)=" << XOpenDisplay(NULL);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        Display *display = QX11Info::display();
#else
        Display *display = QGuiApplication::instance()->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
        //int screenNumber = DefaultScreen(display);
        //Window rootWindow = XDefaultRootWindow(display);
        //unsigned long color = BlackPixel(display, screenNumber);
        unsigned long color = 0x0000ff;
        int w = width();
        int h = height();
        int borderWidth = 0;
        m_window = XCreateSimpleWindow(display, winId(), 0, 0, w, h, borderWidth, 1 /*magic number*/, color);
        XMapWindow(display, m_window);
        //XSelectInput(display, m_window, KeyPressMask | ButtonPressMask | ExposureMask | KeyReleaseMask | ButtonReleaseMask | PointerMotionMask);
      }
      break;
    case QEvent::WindowBlocked:
      //      if (m_hwnd)
      //        EnableWindow(m_hwnd, false);
      break;
    case QEvent::WindowUnblocked:
      //      if (m_hwnd)
      //        EnableWindow(m_hwnd, true);
      break;
    case QEvent::WindowActivate:
      //qDebug() << "WindowActivate";
      break;
    case QEvent::WindowDeactivate:
      //qDebug() << "WindowDeactivate";
      break;
    case QEvent::Enter:
      //qDebug() << "Enter";
      break;
    case QEvent::Leave:
      //qDebug() << "Leave";
      break;
    case QEvent::CursorChange:
      //qDebug() << "CursorChange";
      e->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    default:
      qDebug() << "Unprocessed Event: " << e->type();
      break;
  }
  return QWidget::event(e);
}

/*!
    \reimp
*/
void QVNCX11View::showEvent(QShowEvent *e)
{
  QWidget::showEvent(e);
}

/*!
    \reimp
*/
void QVNCX11View::focusInEvent(QFocusEvent *e)
{
  QWidget::focusInEvent(e);
}

/*!
    \reimp
*/
void QVNCX11View::resizeEvent(QResizeEvent *e)
{
  QWidget::resizeEvent(e);
}

/*!
    \reimp
*/
bool QVNCX11View::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
  return QWidget::nativeEvent(eventType, message, result);
}

void QVNCX11View::bell()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  Display *display = QX11Info::display();
#else
  Display *display = QGuiApplication::instance()->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  XBell(display, 0 /* volume */);
}
