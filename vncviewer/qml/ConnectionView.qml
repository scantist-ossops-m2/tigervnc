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
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOn
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        contentWidth: remoteView.width
        contentHeight: remoteView.height

        VNCItem {
            id: remoteView
            anchors.centerIn: parent
            width: AppManager.remoteViewWidth
            height: AppManager.remoteViewHeight
            focus: true

            onPopupToast: function(text) {
                toastLabel.text = text
                toastTimer.start()
                toast.visible = true
            }
        }
    }

    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 50
        width: 300
        height: 40
        color: "#96101010"
        radius: 5

        Label {
            id: toastLabel
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: "#e0ffffff"
            font.pixelSize: 14
            font.weight: Font.bold
        }

        Timer {
            id: toastTimer
            interval: 5000
            onTriggered: {
                toast.visible = false
            }
        }
    }
}
