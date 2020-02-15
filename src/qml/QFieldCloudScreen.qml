import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

import org.qfield 1.0
import Theme 1.0
import "."

Page {
  signal finished
  property QFieldCloudConnection connection: QFieldCloudConnection
  {
    url: "http://dev.qfield.cloud"
    onLoginFailed: displayToast( "Login failed: " + reason )
  }

  QFieldCloudProjectsModel {
    id: projectsModel
    cloudConnection: connection

    onWarning: displayToast( message )
  }

  header: PageHeader {
      title: qsTr("QField Cloud")

      showApplyButton: false
      showCancelButton: true

      onFinished: parent.finished()
    }

  ColumnLayout {
    anchors.fill: parent
    Layout.fillWidth: true
    Layout.fillHeight: true
    spacing: 2

    RowLayout {
        id: connectionInformation
        spacing: 2
        Layout.fillWidth: true

        Label {
            Layout.fillWidth: true
            padding: 10 * dp
            visible: projects.visible
            text: switch(connection.status) {
                    case 0: qsTr( 'Disconnected from the cloud.' ); break;
                    case 1: qsTr( 'Connecting to the cloud.' ); break;
                    case 2: qsTr( 'Greetings %1.' ).arg( connection.username ); break;
                  }
            wrapMode: Text.WordWrap
            font: Theme.tipFont
        }

        ToolButton {
          Layout.alignment: Qt.AlignTop

          height: 56 * dp
          width: 56 * dp
          visible: true

          contentItem: Rectangle {
            anchors.fill: parent
            color: "transparent"
            Image {
              anchors.fill: parent
              fillMode: Image.Pad
              horizontalAlignment: Image.AlignHCenter
              verticalAlignment: Image.AlignVCenter
              source: Theme.getThemeIcon( 'ic_gear_black_24dp' )
            }
          }

          onClicked: {
              if (!connectionSettings.visible) {
                connectionSettings.visible = true
                connectionInformation.visible = false
                projects.visible = false
                username.forceActiveFocus()
              } else {
                connectionSettings.visible = false
                connectionInformation.visible = true
                projects.visible = true
                refreshProjectList.forceActiveFocus()
              }
          }
       }
    }

    ColumnLayout {
      id: connectionSettings
      Layout.fillWidth: true
      Layout.fillHeight: true
      Layout.margins: 20 * dp
      spacing: 2

      Text {
          id: cloudDescriptionLabel
          Layout.alignment: Qt.AlignLeft
          Layout.fillWidth: true
          text: qsTr( "Please file required details to connect to your account." )
          font: Theme.defaultFont
          wrapMode: Text.WordWrap
      }

      Text {
          id: usernamelabel
          Layout.alignment: Qt.AlignHCenter
          Layout.topMargin: 20 * dp
          text: qsTr( "Username" )
          font: Theme.defaultFont
          color: 'gray'
      }

      TextField {
          id: username
          Layout.alignment: Qt.AlignHCenter
          Layout.preferredWidth: Math.max( parent.width / 2, usernamelabel.width )
          height: fontMetrics.height + 20 * dp
          font: Theme.defaultFont

          background: Rectangle {
              y: username.height - height * 2 - username.bottomPadding / 2
              implicitWidth: parent.width
              height: username.activeFocus ? 2 * dp : 1 * dp
              color: username.activeFocus ? "#4CAF50" : "#C8E6C9"
          }
      }

      Text {
          id: passwordlabel
          Layout.alignment: Qt.AlignHCenter
          text: qsTr( "Password" )
          font: Theme.defaultFont
          color: 'gray'
      }

      TextField {
          id: password
          echoMode: TextInput.Password
          Layout.alignment: Qt.AlignHCenter
          Layout.preferredWidth: Math.max( parent.width / 2, usernamelabel.width )
          height: fontMetrics.height + 20 * dp
          font: Theme.defaultFont

          background: Rectangle {
              y: password.height - height * 2 - password.bottomPadding / 2
              implicitWidth: parent.width
              height: password.activeFocus ? 2 * dp : 1 * dp
              color: password.activeFocus ? "#4CAF50" : "#C8E6C9"
          }
      }

      FontMetrics {
        id: fontMetrics
        font: username.font
      }

      QfButton {
          Layout.fillWidth: true
          Layout.topMargin: 5 * dp
          text: connection.status == QFieldCloudConnection.LoggedIn ? qsTr( "Logout" ) : qsTr( "Login" )
          enabled: connection.status != QFieldCloudConnection.Connecting

          onClicked: {
              if (connection.status == QFieldCloudConnection.LoggedIn) {
                  connection.logout()
              } else {
                  connection.username = username.text
                  connection.password = password.text
                  connection.login()

                  connectionSettings.visible = false
                  projects.visible = true
              }
          }
      }

      Item {
          Layout.fillHeight: true
          height: 15 * dp
      }
    }

    ColumnLayout {
      id: projects
      Layout.fillWidth: true
      Layout.fillHeight: true
      Layout.margins: 10 * dp
      spacing: 2

      ListView {
          Layout.fillWidth:  true
          Layout.fillHeight: true

          width: 200
          height: 500

          model: projectsModel

          delegate: Rectangle {
              height: childrenRect.height
              Row {
                  Button {
                      Layout.alignment: Qt.AlignTop | Qt.AlignLeft

                      width: 24*dp
                      height: 24*dp
                      clip: true
                      bgcolor: Theme.darkGray

                      iconSource: {
                          switch ( Status )
                          {
                          case 0:
                              Theme.getThemeIcon( 'ic_check_white_48dp' )
                              break;

                          case 1:
                              Theme.getThemeIcon( 'ic_add_white_24dp' )
                              break;

                          default:
                              Theme.getThemeIcon( 'ic_add_circle_outline_white_24dp' )
                              break;
                          }
                      }

                      onClicked: {
                          projectsModel.download( username.text, Name )
                      }
                  }

                  Text {
                      text: Name + ": " + Description + " (" + Id + ")"
                  }
              }
          }
      }
      QfButton {
          Layout.fillWidth: true
          text: "Refresh Projects"
          enabled: connection.status == 2

          onClicked: projectsModel.refreshProjectsList()
      }
    }
  }

  function prepareCloudLogin() {
    if ( visible ) {
      if ( connection.status == QFieldCloudConnection.Disconnected ) {
        if ( connection.hasToken ) {
          username.text = connection.username;
          connection.login();

          connectionInformation.visible = true
          projects.visible = true
          connectionSettings.visible = false
        } else {
          username.text = connection.username

          connectionInformation.visible = false
          projects.visible = false
          connectionSettings.visible = true
        }
      } else {
        connectionInformation.visible = true
        projects.visible = true
        connectionSettings.visible = false
      }
    }
  }

  Component.onCompleted: {
    prepareCloudLogin()
  }

  onVisibleChanged: {
    prepareCloudLogin()
  }

  Keys.onReleased: {
    if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
      event.accepted = true
      finished()
    }
  }
}
