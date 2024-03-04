import QtQuick 2.12
import QtQuick.Controls 2.12
import Qt.TigerVNC 1.0

Menu {
    id: contextMenu

    signal menuKey
    signal ctrlKeyToggle(bool checked)
    signal altKeyToggle(bool checked)
    signal ctrlAltDel

    signal optionDialogAction
    signal infoDialogAction
    signal aboutDialogAction

    Action {
        text: ContextMenuActions.disconnectAction
        onTriggered: ContextMenuActions.disconnect()
    }

    MenuSeparator {}

    Action {
        text: ContextMenuActions.fullScreenAction
        checkable: true
        checked: AppManager.isFullscreen
        onTriggered: AppManager.toggleFullscreen()
    }

    Action {
        text: ContextMenuActions.minimizeAction
        onTriggered: ContextMenuActions.minimize()
    }

    Action {
        text: ContextMenuActions.revertSizeAction
        onTriggered: ContextMenuActions.revertSize()
    }

    MenuSeparator {}

    Action {
        text: ContextMenuActions.ctrlKeyToggleAction
        checkable: true
        onTriggered: ctrlKeyToggle(checked)
    }

    Action {
        text: ContextMenuActions.altKeyToggleAction
        checkable: true
        onTriggered: altKeyToggle(checked)
    }

    Action {
        text: ContextMenuActions.menuKeyAction
        onTriggered: menuKey()
    }

    Action {
        text: ContextMenuActions.ctrlAltDelAction
        onTriggered: ctrlAltDel()
    }

    MenuSeparator {}

    Action {
        text: ContextMenuActions.refreshAction
        onTriggered: ContextMenuActions.refresh()
    }

    MenuSeparator {}

    Action {
        text: ContextMenuActions.optionDialogAction
        onTriggered: optionDialogAction()
    }

    Action {
        text: ContextMenuActions.infoDialogAction
        onTriggered: infoDialogAction()
    }

    Action {
        text: ContextMenuActions.aboutDialogAction
        onTriggered: aboutDialogAction()
    }
}
