#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QProcess>
#if !defined(WIN32)
#include <QCoreApplication>
#endif
#include "parameters.h"
#include "tunnelfactory.h"

TunnelFactory::TunnelFactory()
 : QThread(nullptr)
 , errorOccurred_(false)
 , error_(QProcess::FailedToStart)
#if defined(WIN32)
 , command_(QString(qgetenv("SYSTEMROOT")) + "\\System32\\OpenSSH\\ssh.exe")
#else
 , command_("/usr/bin/ssh")
 , operationSocketName_("vncviewer-tun-" + QString::number(QCoreApplication::applicationPid()))
#endif
 , process_(nullptr)
{
}

void TunnelFactory::run()
{
  QString gatewayHost = ViewerConfig::config()->gatewayHost();
  if (gatewayHost.isEmpty()) {
    return;
  }
  QString remoteHost = ViewerConfig::config()->serverHost();
  if (remoteHost.isEmpty()) {
    return;
  }
  int remotePort = ViewerConfig::config()->serverPort();
  int localPort = ViewerConfig::config()->gatewayLocalPort();

  QString viacmd(qgetenv("VNC_VIA_CMD"));
  qputenv("G", gatewayHost.toUtf8());
  qputenv("H", remoteHost.toUtf8());
  qputenv("R", QString::number(remotePort).toUtf8());
  qputenv("L", QString::number(localPort).toUtf8());

  QStringList args;
  if (viacmd.isEmpty()) {
    args = QStringList({
#if !defined(WIN32)
                         "-fnNTM",
                         "-S",
                         operationSocketName_,
#endif
                         "-L",
                         QString::number(localPort) + ":" + remoteHost + ":" + QString::number(remotePort),
                         gatewayHost,
                       });
  }
  else {
#if !defined(WIN32)
    /* Compatibility with TigerVNC's method. */
    viacmd.replace('%', '$');
#endif
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    args = splitCommand(viacmd);
#else
    args = QProcess::splitCommand(viacmd);
#endif
    command_ = args.length() > 0 ? args[0] : "";
    args.removeFirst();
  }
  delete process_;
  process_ = new QProcess;

#if !defined(WIN32)
  if (!process_->execute(command_, args)) {
    QString serverName = "localhost::" + QString::number(ViewerConfig::config()->gatewayLocalPort());
    ViewerConfig::config()->setAccessPoint(serverName);
  }
  else {
    errorOccurred_ = true;
  }
#else
  connect(process_, &QProcess::started, this, []() {
    QString serverName = "localhost::" + QString::number(ViewerConfig::config()->gatewayLocalPort());
    ViewerConfig::config()->setAccessPoint(serverName);
  });
  connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
    errorOccurred_ = true;
    error_ = e;
  });
  process_->start(command_, args);
  while (true) {
    //qDebug() << "state=" << process_->state();
    if (process_->state() == QProcess::Running || errorOccurred_) {
      break;
    }
    QThread::usleep(10);
  }
#endif
}

TunnelFactory::~TunnelFactory()
{
  close();
  delete process_;
}

void TunnelFactory::close()
{
#if !defined(WIN32)
  if (process_) {
    QString gatewayHost = ViewerConfig::config()->gatewayHost();
    QStringList args({ "-S",
                       operationSocketName_,
                       "-O",
                       "exit",
                       gatewayHost,
                     });
    QProcess process;
    process.start(command_, args);
    QThread::msleep(500);
  }
#endif
  if (process_ && process_->state() != QProcess::NotRunning) {
    process_->kill();
  }
}

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
QStringList TunnelFactory::splitCommand(QStringView command)
{
    QStringList args;
    QString tmp;
    int quoteCount = 0;
    bool inQuote = false;
    // handle quoting. tokens can be surrounded by double quotes
    // "hello world". three consecutive double quotes represent
    // the quote character itself.
    for (int i = 0; i < command.size(); ++i) {
        if (command.at(i) == QLatin1Char('"')) {
            ++quoteCount;
            if (quoteCount == 3) {
                // third consecutive quote
                quoteCount = 0;
                tmp += command.at(i);
            }
            continue;
        }
        if (quoteCount) {
            if (quoteCount == 1)
                inQuote = !inQuote;
            quoteCount = 0;
        }
        if (!inQuote && command.at(i).isSpace()) {
            if (!tmp.isEmpty()) {
                args += tmp;
                tmp.clear();
            }
        } else {
            tmp += command.at(i);
        }
    }
    if (!tmp.isEmpty())
        args += tmp;
    return args;
}
#endif