import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.4

import org.qfield 1.0
import Theme 1.0

Item {
  id: qfieldcloudLogin
  property bool isServerUrlEditingActive: false

  Column {
    id: connectionSettings
    width: parent.width
    spacing: 10

    Image {
      anchors.horizontalCenter: parent.horizontalCenter
      fillMode: Image.PreserveAspectFit
      smooth: true
      source: "qrc:/images/qfieldcloud_logo.svg"
      sourceSize.width: 124
      sourceSize.height: 124
    }

    Text {
      id: cloudIntroLabel
      width: parent.width
      text: '<style>a, a:hover, a:visited { color:' + Theme.mainColor + '; }></style>' + qsTr( 'The easiest way to transfer you project from QGIS to your devices!' ) +
            ' <a href="https://qfield.cloud/">' + qsTr( 'Learn more about QFieldCloud' ) + '</a>.'
      horizontalAlignment: Text.AlignHCenter
      font: Theme.defaultFont
      textFormat: Text.RichText
      wrapMode: Text.WordWrap

      onLinkActivated: Qt.openUrlExternally(link)

      MouseArea {
        anchors.fill: parent
        onDoubleClicked: {
          if ( cloudConnection.url != cloudConnection.defaultUrl )
          {
            isServerUrlEditingActive = true
            return
          }

          isServerUrlEditingActive = !isServerUrlEditingActive
        }
      }
    }

    Text {
      id: serverUrlLabel
      width: parent.width
      visible: cloudConnection.status === QFieldCloudConnection.Disconnected
               && ( cloudConnection.url !== cloudConnection.defaultUrl || isServerUrlEditingActive )
      text: qsTr( "Server URL. Empty to use the default." )
      horizontalAlignment: Text.AlignHCenter
      font: Theme.defaultFont
      color: 'gray'
    }

    TextField {
      id: serverUrlField
      width: parent.width / 1.3
      anchors.horizontalCenter: parent.horizontalCenter
      visible: cloudConnection.status === QFieldCloudConnection.Disconnected
               && ( cloudConnection.url !== cloudConnection.defaultUrl || isServerUrlEditingActive )
      enabled: visible
      height: Math.max(fontMetrics.height, fontMetrics.boundingRect(text).height) + 34
      topPadding: 10
      bottomPadding: 10
      font: Theme.defaultFont
      horizontalAlignment: Text.AlignHCenter
      text: cloudConnection.url === cloudConnection.defaultUrl ? '' : cloudConnection.url
      background: Rectangle {
        y: serverUrlField.height - height * 2 - serverUrlField.bottomPadding / 2
        implicitWidth: 120
        height: serverUrlField.activeFocus ? 2 : 1
        color: serverUrlField.activeFocus ? "#4CAF50" : "#C8E6C9"
      }

      Keys.onReturnPressed: loginFormSumbitHandler()
      Keys.onReleased: {
        cloudConnection.url = text ? text : cloudConnection.defaultUrl
      }
    }

    Text {
      id: usernamelabel
      width: parent.width
      visible: cloudConnection.status === QFieldCloudConnection.Disconnected
      text: qsTr( "Username" )
      horizontalAlignment: Text.AlignHCenter
      font: Theme.defaultFont
      color: 'gray'
    }

    TextField {
      id: usernameField
      width: parent.width / 1.3
      anchors.horizontalCenter: parent.horizontalCenter
      visible: cloudConnection.status === QFieldCloudConnection.Disconnected
      enabled: visible
      height: Math.max(fontMetrics.height, fontMetrics.boundingRect(text).height) + 34
      topPadding: 10
      bottomPadding: 10
      font: Theme.defaultFont
      horizontalAlignment: Text.AlignHCenter

      background: Rectangle {
        y: usernameField.height - height * 2 - usernameField.bottomPadding / 2
        implicitWidth: 120
        height: usernameField.activeFocus ? 2 : 1
        color: usernameField.activeFocus ? "#4CAF50" : "#C8E6C9"
      }

      Keys.onReturnPressed: loginFormSumbitHandler()
    }

    Text {
      id: passwordlabel
      width: parent.width
      visible: cloudConnection.status === QFieldCloudConnection.Disconnected
      text: qsTr( "Password" )
      horizontalAlignment: Text.AlignHCenter
      font: Theme.defaultFont
      color: 'gray'
    }

    TextField {
      id: passwordField
      echoMode: TextInput.Password
      width: parent.width / 1.3
      anchors.horizontalCenter: parent.horizontalCenter
      visible: cloudConnection.status === QFieldCloudConnection.Disconnected
      enabled: visible
      height: Math.max(fontMetrics.height, fontMetrics.boundingRect(text).height) + 34
      topPadding: 10
      bottomPadding: 10
      font: Theme.defaultFont
      horizontalAlignment: Text.AlignHCenter

      background: Rectangle {
        y: passwordField.height - height * 2 - passwordField.bottomPadding / 2
        implicitWidth: 120
        height: passwordField.activeFocus ? 2 : 1
        color: passwordField.activeFocus ? "#4CAF50" : "#C8E6C9"
      }

      Keys.onReturnPressed: loginFormSumbitHandler()
    }

    FontMetrics {
      id: fontMetrics
      font: Theme.defaultFont
    }

    QfButton {
      width: parent.width
      text: cloudConnection.status == QFieldCloudConnection.LoggedIn ? qsTr( "Logout" ) : cloudConnection.status == QFieldCloudConnection.Connecting ? qsTr( "Logging in, please wait" ) : qsTr( "Login" )
      enabled: cloudConnection.status != QFieldCloudConnection.Connecting

      onClicked: loginFormSumbitHandler()
    }
  }

  Connections {
    target: cloudConnection

    onStatusChanged: {
      if ( cloudConnection.status === QFieldCloudConnection.LoggedIn )
        usernameField.text = cloudConnection.username
    }
  }

  function loginFormSumbitHandler() {
    if (cloudConnection.status == QFieldCloudConnection.LoggedIn) {
      cloudConnection.logout()
    } else {
      cloudConnection.username = usernameField.text
      cloudConnection.password = passwordField.text
      cloudConnection.login()
    }
  }
}
