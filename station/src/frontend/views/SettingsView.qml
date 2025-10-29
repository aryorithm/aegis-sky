import QtQuick
import QtQuick.Controls
import Aegis.Backend 1.0

Item {
    // Instantiate the C++ Class
    CoreClient {
        id: client
        hostAddress: "127.0.0.1"
        port: 9090
        
        onConnectionChanged: (connected) => {
            console.log("Connection Status: " + connected)
            statusLabel.text = connected ? "ONLINE" : "OFFLINE"
            statusLabel.color = connected ? "green" : "red"
        }
    }

    Column {
        TextField { id: ipField; text: "127.0.0.1" }
        Button {
            text: "Connect"
            onClicked: {
                client.hostAddress = ipField.text
                client.connectToCore()
            }
        }
        Text {
            id: statusLabel
            text: "OFFLINE"
            color: "red"
        }
    }
}