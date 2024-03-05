#include "contextmenuactions.h"

#include "appmanager.h"

#include <QApplication>
#include <QQmlEngine>

ContextMenuActions* ContextMenuActions::contextMenuActions_;

ContextMenuActions::ContextMenuActions(QObject* parent) : QObject(parent)
{
}

int ContextMenuActions::initialize()
{
  contextMenuActions_ = new ContextMenuActions();
  qmlRegisterSingletonType<ContextMenuActions>("Qt.TigerVNC",
                                               1,
                                               0,
                                               "ContextMenuActions",
                                               [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
                                                 Q_UNUSED(engine)
                                                 Q_UNUSED(scriptEngine)
                                                 return contextMenuActions_;
                                               });
  return 0;
}

void ContextMenuActions::disconnect()
{
  QApplication::quit();
}

void ContextMenuActions::minimize()
{
  AppManager::instance()->minimizeVNCWindow();
}

void ContextMenuActions::revertSize()
{
  // xTODO
  // connect(this, &QAction::triggered, this, []() {
  //     QVNCWindow*       window = AppManager::instance()->window();
  //     QAbstractVNCView* view   = AppManager::instance()->view();
  //     window->normalizedResize(view->width() + window->horizontalScrollBar()->width(),
  //                              view->height()
  //                                  + window->verticalScrollBar()->height()); // Needed to hide scrollbars.
  //     window->normalizedResize(view->width(), view->height());
  // });
  // connect(ViewerConfig::config(), &ViewerConfig::fullScreenChanged, this, [this](bool enabled) {
  //     setEnabled(!enabled); // cf. Viewport::initContextMenu()
  // });
}

void ContextMenuActions::refresh()
{
  AppManager::instance()->connection()->refreshFramebuffer();
}
