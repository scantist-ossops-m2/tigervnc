#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "appmanager.h"
#include "contextmenuactions.h"
#include "parameters.h"
#include "vncapplication.h"
#include "vnctranslator.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QStyle>
#include <QUrl>

class StandardIconProvider : public QQuickImageProvider
{
public:
  StandardIconProvider(QStyle* style) : QQuickImageProvider(Pixmap), style_(style)
  {
  }

  QPixmap requestPixmap(QString const& id, QSize* size, QSize const& requestedSize) override
  {
    Q_UNUSED(size)
    int const defaultWidth  = 48;
    int const defaultHeight = 48;
    QSize     imageSize     = {requestedSize.width() > 0 ? requestedSize.width() : defaultWidth,
                       requestedSize.height() > 0 ? requestedSize.height() : defaultHeight};
    static auto const metaobject = QMetaEnum::fromType<QStyle::StandardPixmap>();
    int const         value      = metaobject.keyToValue(id.toLatin1());
    QIcon             icon       = style_->standardIcon(static_cast<QStyle::StandardPixmap>(value));
    return icon.pixmap(imageSize);
  }

private:
  QStyle* style_;
};

int main(int argc, char* argv[])
{
  if (qEnvironmentVariableIsEmpty("QTGLESSTREAM_DISPLAY"))
  {
    qputenv("QT_QPA_EGLFS_PHYSICAL_WIDTH", QByteArray("213"));
    qputenv("QT_QPA_EGLFS_PHYSICAL_HEIGHT", QByteArray("120"));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#endif
  }

  QVNCApplication app(argc, argv);

  app.setOrganizationName("TigerVNC Team");
  app.setOrganizationDomain("tigervnc.org");
  app.setApplicationName("TigerVNC Viewer");

  QQmlApplicationEngine engine;

  ViewerConfig::initialize();
  AppManager::initialize(&engine);
  ContextMenuActions::initialize();

  VNCTranslator translator;
  app.installTranslator(&translator);

  engine.addImageProvider(QLatin1String("qticons"), new StandardIconProvider(app.style()));
  QUrl const url(QStringLiteral("qrc:/qml/main.qml"));
  QObject::connect(
      &engine,
      &QQmlApplicationEngine::objectCreated,
      &app,
      [url](QObject* obj, QUrl const& objUrl) {
        if (!obj && url == objUrl)
        {
          QCoreApplication::exit(-1);
        }
      },
      Qt::QueuedConnection);
  engine.load(url);

  return app.exec();
}
