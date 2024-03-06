#ifndef VNCCONNECTION_H
#define VNCCONNECTION_H

// #include "rdr/types.h"
#include "CConn.h"
#include "rfb/Rect.h"

#include <QObject>

class QTimer;
class QCursor;
class QSocketNotifier;
class TunnelFactory;

namespace rdr
{
class InStream;
class OutStream;
} // namespace rdr

namespace rfb
{
class ServerParams;
class SecurityClient;
class PixelFormat;
class ModifiablePixelBuffer;
class CMsgReader;
class CMsgWriter;
struct ScreenSet;
struct Point;
} // namespace rfb
Q_DECLARE_METATYPE(rfb::ScreenSet)
Q_DECLARE_METATYPE(rfb::Point)

namespace network
{
class Socket;
}

class QVNCConnection : public QObject
{
  Q_OBJECT

public:
  QVNCConnection();
  virtual ~QVNCConnection();

  rfb::ServerParams* server()
  {
    return &rfbcon_->server;
  }

  void setState(int state);

  rdr::InStream* istream()
  {
    return rfbcon_->getInStream();
  }

  rdr::OutStream* ostream()
  {
    return rfbcon_->getOutStream();
  }

  rfb::CMsgReader* reader()
  {
    return rfbcon_->reader();
  }

  rfb::CMsgWriter* writer()
  {
    return rfbcon_->writer();
  }

  void setQualityLevel(int level)
  {
    rfbcon_->setQualityLevel(level);
  }

  rfb::ModifiablePixelBuffer* framebuffer()
  {
    if (rfbcon_)
      return rfbcon_->framebuffer();
    else
      return nullptr;
  }

  void setCompressLevel(int level)
  {
    rfbcon_->setCompressLevel(level);
  }

  QTimer* updateTimer() const
  {
    return updateTimer_;
  }

  void updatePixelFormat()
  {
    rfbcon_->updatePixelFormat();
  }

  void announceClipboard(bool available);

  void setPreferredEncoding(int encoding)
  {
    rfbcon_->setPreferredEncoding(encoding);
  }

signals:
  void socketNotified();
  void newVncWindowRequested(int width, int height, QString name);
  void cursorChanged(QCursor const& cursor);
  void cursorPositionChanged(int x, int y);
  void ledStateChanged(unsigned int state);
  void clipboardAnnounced(bool available);
  void clipboardDataReceived(char const* data);
  void framebufferResized(int width, int height);
  void refreshFramebufferStarted();
  void refreshFramebufferEnded();
  void bellRequested();

  void writePointerEvent(rfb::Point const& pos, int buttonMask);
  void writeSetDesktopSize(int width, int height, rfb::ScreenSet const& layout);
  void writeKeyEvent(uint32_t keysym, uint32_t keycode, bool down);

public slots:
  void listen();
  void connectToServer(QString addressport = "");
  void resetConnection();
  void startProcessing();
  void refreshFramebuffer();

  QString infoText()
  {
    return rfbcon_ ? rfbcon_->connectionInfo() : "";
  }

  QString host()
  {
    return rfbcon_ ? rfbcon_->host() : "";
  }

private:
  CConn*           rfbcon_;
  network::Socket* socket_;
  QSocketNotifier* socketNotifier_;
  QSocketNotifier* socketErrorNotifier_;
  QTimer*          updateTimer_;
  TunnelFactory*   tunnelFactory_;
  bool             closing_;
  QString          addressport_;

  void bind(int fd);

  void setHost(QString host)
  {
    rfbcon_->setHost(host);
  }

  void setPort(int port)
  {
    rfbcon_->setPort(port);
  }
};

#endif // VNCCONNECTION_H
