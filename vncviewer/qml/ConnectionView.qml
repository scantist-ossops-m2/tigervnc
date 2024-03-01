import QtQuick 2.12
import QtQuick.Controls 2.12
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

    Menu {
        id: contextMenu

        onVisibleChanged: remoteView.contextMenuVisible = visible

        Action {
            text: "Cut"
        }
        Action {
            text: "Copy"
        }
        Action {
            text: "Paste"
        }

        MenuSeparator {}

        Menu {
            title: "Find/Replace"
            Action {
                text: "Find Next"
            }
            Action {
                text: "Find Previous"
            }
            Action {
                text: "Replace"
            }
        }
    }
}
