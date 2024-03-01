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
                toastLabel.text = text
                toastTimer.start()
                toast.state = "$VISIBLE"
            }
        }
    }

    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: -height
        width: 300
        height: 40
        color: "#96101010"
        radius: 5
        opacity: 0

        state: "$HIDDEN"

        transitions: [
            Transition {
                from: "$HIDDEN"
                to: "$VISIBLE"
                ParallelAnimation {
                    NumberAnimation {
                        target: toast
                        properties: "anchors.topMargin"
                        easing.type: Easing.InOutQuad
                        from: -toast.height
                        to: 50
                        duration: 1000
                    }
                    NumberAnimation {
                        target: toast
                        properties: "opacity"
                        easing.type: Easing.InOutQuad
                        from: 0
                        to: 1
                        duration: 1000
                    }
                }
            },
            Transition {
                from: "$VISIBLE"
                to: "$HIDDEN"
                ParallelAnimation {
                    NumberAnimation {
                        target: toast
                        properties: "anchors.topMargin"
                        easing.type: Easing.InOutQuad
                        from: 50
                        to: -toast.height
                        duration: 1000
                    }
                    NumberAnimation {
                        target: toast
                        properties: "opacity"
                        easing.type: Easing.InOutQuad
                        from: 1
                        to: 0
                        duration: 1000
                    }
                }
            }
        ]

        Label {
            id: toastLabel
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: "#e0ffffff"
            font.pixelSize: 14
            font.bold: true
        }

        Timer {
            id: toastTimer
            interval: 5000
            onTriggered: {
                toast.state = "$HIDDEN"
            }
        }
    }
}
