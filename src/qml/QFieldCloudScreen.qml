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

    onProjectDownloaded: failed ? displayToast( qsTr( "Project %1 failed to download" ).arg( projectName ) ) :
                                  displayToast( qsTr( "Project %1 successfully downloaded, it's now available to open" ).arg( projectName ) );
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
                refreshProjectsListBtn.forceActiveFocus()
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
                  console.log(username.text)
                  console.log(password.text)
                  connection.login()

                  connectionInformation.visible = true
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
      Layout.topMargin: 0
      spacing: 2

      Rectangle {
          Layout.fillWidth: true
          Layout.fillHeight: true
          color: "white"
          border.color: "lightgray"
          border.width: 1

          ListView {
              id: table
              anchors.fill: parent

              model: projectsModel

              delegate: Rectangle {
                  id: rectangle
                  property string projectOwner: Owner
                  property string projectName: Name
                  property string projectLocalPath: LocalPath
                  width: parent.width
                  height: line.height
                  color: "transparent"

                  Row {
                      id: line
                      Layout.fillWidth: true
                      leftPadding: 6 * dp
                      rightPadding: 10 * dp
                      topPadding: 9 * dp
                      bottomPadding: 3 * dp
                      spacing: 0

                      Image {
                          id: type
                          anchors.verticalCenter: inner.verticalCenter
                          source: connection.status != QFieldCloudConnection.LoggedIn ? Theme.getThemeIcon('ic_cloud_project_offline_48dp') : Status == QFieldCloudProjectsModel.Status.LocalOnly ? Theme.getThemeIcon('ic_cloud_project_localonly_48dp') : LocalPath != '' ? Theme.getThemeIcon('ic_cloud_project_48dp') : Theme.getThemeIcon('ic_cloud_project_download_48dp')
                          sourceSize.width: 80 * dp
                          sourceSize.height: 80 * dp
                          width: 40 * dp
                          height: 40 * dp
                      }
                      ColumnLayout {
                          id: inner
                          width: rectangle.width - type.width - 10 * dp
                          Text {
                              id: projectTitle
                              topPadding: 5 * dp
                              leftPadding: 3 * dp
                              text: Name
                              font.pointSize: Theme.tipFont.pointSize
                              font.underline: true
                              color: Theme.mainColor
                              wrapMode: Text.WordWrap
                              Layout.fillWidth: true
                          }
                          Text {
                              id: projectNote
                              leftPadding: 3 * dp
                              text: connection.status != QFieldCloudConnection.LoggedIn ? qsTr( '(Available locally)' ) : Description + ' ' + ( LocalPath != '' ? Status == QFieldCloudProjectsModel.Status.LocalOnly ? qsTr( '(Available locally, missing on cloud)' ) : qsTr( '(Available locally)' ) : Status == 1 ? qsTr( '(Downloading...)' ) : '' )
                              visible: text != ""
                              font.pointSize: Theme.tipFont.pointSize - 2
                              font.italic: true
                              wrapMode: Text.WordWrap
                              Layout.fillWidth: true
                          }
                      }
                  }
              }

              MouseArea {
                property Item pressedItem
                anchors.fill: parent
                onClicked: {
                  var item = table.itemAt(mouse.x, mouse.y)
                  if (item) {
                    if (item.projectLocalPath != '') {
                      // project available locally, open it
                      console.log(item.projectLocalPath)
                    } else {
                      // fetch remote project
                      projectsModel.download( item.projectOwner, item.projectName )
                    }
                  }
                }
                onPressed: {
                  var item = table.itemAt(mouse.x, mouse.y)
                  if (item) {
                    pressedItem = item.children[0].children[1].children[0];
                    pressedItem.color = "#5a8725"
                  }
                }
                onCanceled: {
                  if (pressedItem) {
                    pressedItem.color = Theme.mainColor
                    pressedItem = null
                  }
                }
                onReleased: {
                  if (pressedItem) {
                    pressedItem.color = Theme.mainColor
                    pressedItem = null
                  }
                }
              }
          }
      }

      QfButton {
          id: refreshProjectsListBtn
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
