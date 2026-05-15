import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import WifiCam 1.0

Kirigami.ApplicationWindow {
    visible: true

    pageStack.initialPage: Kirigami.Page {

        ColumnLayout {
            anchors.fill: parent

            MpvItem {
                id: player
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Button {
                text: "Start Stream"

                onClicked: {
                    console.log("start stream clicked!")
                    cameraClient.start(player)
                }
            }
        }
    }
}
