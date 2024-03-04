import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import Qt.TigerVNC 1.0

Item {
    id: root
    anchors.fill: parent

    ScrollView {
        anchors.centerIn: parent
        width: remoteView.width < root.width ? remoteView.width : root.width
        height: remoteView.height < root.height ? remoteView.height : root.height
        clip: true
        contentWidth: remoteView.width
        contentHeight: remoteView.height

        VNCItem {
            id: remoteView
            anchors.centerIn: parent
            width: AppManager.remoteViewWidth
            height: AppManager.remoteViewHeight
            focus: true

            onPopupToast: function (text) {
                toast.text = text
                toast.start()
                toast.state = "$VISIBLE"
            }

            onContextMenuVisibleChanged: function () {
                contextMenu.visible = contextMenuVisible
                var pos = root.mapFromGlobal(cursorPos().x, cursorPos().y)
                contextMenu.x = pos.x
                contextMenu.y = pos.y
            }
        }
    }

    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: -height
    }

    ContextMenu {
        id: contextMenu

        onVisibleChanged: {
            remoteView.contextMenuVisible = visible
            if (visible) {
                focus = true
                forceActiveFocus()
            }
        }

        onMenuKey: remoteView.menuKey()
        onCtrlKeyToggle: remoteView.ctrlKeyToggle(checked)
        onAltKeyToggle: remoteView.altKeyToggle(checked)
        onCtrlAltDel: remoteView.ctrlAltDel()

        onOptionDialogAction: optionsDialog.open()
        onInfoDialogAction: infoDialog.open()
        onAboutDialogAction: aboutDialog.open()
    }

    Popup {
        id: optionsDialog

        objectName: "OptionsDialog"
        width: optionsContainer.implicitWidth
        height: optionsContainer.implicitHeight
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent

        OptionDialogContent {
            id: optionsContainer
            onCloseRequested: optionsDialog.close()
        }
    }

    Popup {
        id: infoDialog

        objectName: "InfoDialog"
        width: infoContainer.implicitWidth
        height: infoContainer.implicitHeight
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent

        InfoDialogContent {
            id: infoContainer
            onCloseRequested: infoDialog.close()
        }
    }

    Popup {
        id: aboutDialog

        objectName: "AboutDialog"
        width: aboutContainer.implicitWidth
        height: aboutContainer.implicitHeight
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent

        AboutDialogContent {
            id: aboutContainer
            onCloseRequested: aboutDialog.close()
        }
    }
}
