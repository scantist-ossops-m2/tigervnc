#include "contextmenuactions.h"

#include "appmanager.h"

#include <QApplication>

ContextMenuActions::ContextMenuActions(QObject* parent) : QObject(parent)
{
}

void ContextMenuActions::disconnect()
{
    QApplication::quit();
}

void ContextMenuActions::fullScreen()
{
    // xTODO
    // connect(this, &QAction::toggled, this, [](bool checked) {
    //     AppManager::instance()->view()->fullscreen(checked);
    // });
    // connect(ViewerConfig::config(), &ViewerConfig::fullScreenChanged, this, [this](bool enabled) {
    //     setChecked(enabled);
    // });

    // setChecked(ViewerConfig::config()->fullScreen());
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

void ContextMenuActions::ctrlAltDel()
{
    // xTODO
    // QAbstractVNCView* view = AppManager::instance()->view();
    // view->handleKeyPress(0x1d, XK_Control_L);
    // view->handleKeyPress(0x38, XK_Alt_L);
    // view->handleKeyPress(0xd3, XK_Delete);
    // view->handleKeyRelease(0xd3);
    // view->handleKeyRelease(0x38);
    // view->handleKeyRelease(0x1d);
}

void ContextMenuActions::refresh()
{
    AppManager::instance()->connection()->refreshFramebuffer();
}
