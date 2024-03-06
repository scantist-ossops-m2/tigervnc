import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import Qt.TigerVNC 1.0

Item {
    id: root
    anchors.fill: parent

    Flickable {
        id: flickable
        anchors.fill: parent
        clip: true
        contentWidth: container.width
        contentHeight: container.height

        Item {
            id: container
            width: Math.max(remoteView.width, root.width)
            height: Math.max(remoteView.height, root.height)

            VNCItem {
                id: remoteView
                anchors.centerIn: parent
                width: AppManager.remoteViewWidth
                height: AppManager.remoteViewHeight
                focus: true
                clip: true

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

        ScrollBar.vertical: ScrollBar {
            id: verticalScrollbar
            policy: flickable.contentHeight > flickable.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            contentItem: Rectangle {
                implicitWidth: 8
                color: "lightgray"
                border.width: 2
                border.color: "gray"
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                onWheel: {
                    if (wheel.angleDelta.y > 0) {
                        verticalScrollbar.decrease()
                    } else {
                        verticalScrollbar.increase()
                    }
                }
            }
        }
        ScrollBar.horizontal: ScrollBar {
            id: horizontalScrollbar
            policy: flickable.contentWidth > flickable.width ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            contentItem: Rectangle {
                implicitHeight: 8
                color: "lightgray"
                border.width: 2
                border.color: "gray"
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                onWheel: {
                    if (wheel.angleDelta.y > 0) {
                        horizontalScrollbar.decrease()
                    } else {
                        horizontalScrollbar.increase()
                    }
                }
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
