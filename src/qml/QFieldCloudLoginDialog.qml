import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

import org.qfield 1.0
import Theme 1.0
import "."

Page {

    property QFieldCloudConnection connection: QFieldCloudConnection
    {
        url: "http://dev.qfield.cloud"

        onLoginFailed: {
            displayToast( "Login failed: " + reason )
        }
    }

  header: ToolBar {
    id: toolbar
    height: 48 * dp
    visible: true

    anchors {
      top: parent.top
      left: parent.left
      right: parent.right
    }

    background: Rectangle {
      color: Theme.mainColor
    }

    RowLayout {
      anchors.fill: parent
      Layout.margins: 0

      Button {
        id: enterButton

        Layout.alignment: Qt.AlignTop | Qt.AlignLeft

        width: 48*dp
        height: 48*dp
        clip: true
        bgcolor: Theme.darkGray

        iconSource: Theme.getThemeIcon( 'ic_check_white_48dp' )

        onClicked: {
          enter(username.text, password.text)
          username.text=''
          password.text=''
        }
      }

      Label {
        id: titleLabel

        text: "Login information"
        font: Theme.strongFont
        color: "#FFFFFF"
        elide: Label.ElideRight
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        Layout.fillWidth: true
      }

      Button {
        id: cancelButton

        Layout.alignment: Qt.AlignTop | Qt.AlignRight

        width: 49*dp
        height: 48*dp
        clip: true
        bgcolor: Theme.darkGray

        iconSource: Theme.getThemeIcon( 'ic_close_white_24dp' )

        onClicked: {
          cancel()
        }
      }
    }
  }

  ColumnLayout {
    anchors.fill: parent
    Layout.fillWidth: true
    Layout.fillHeight: true

    spacing: 2
    anchors {
        margins: 4 * dp
        topMargin: 52 * dp // Leave space for the toolbar
    }

    Text {
      Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
      Layout.preferredHeight: font.height + 20 * dp
      text: connection
      font: Theme.strongFont
    }

    Text {
      id: usernamelabel
      Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
      Layout.preferredHeight: font.height
      text: qsTr( "Username" )
      font: Theme.defaultFont
    }

    TextField {
      id: username
      Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
      Layout.preferredWidth: Math.max( parent.width / 2, usernamelabel.width )
      Layout.preferredHeight: font.height + 20 * dp
      font: Theme.defaultFont

      background: Rectangle {
        y: username.height - height - username.bottomPadding / 2
        implicitWidth: 120 * dp
        height: username.activeFocus ? 2 * dp : 1 * dp
        color: username.activeFocus ? "#4CAF50" : "#C8E6C9"
      }
    }

    Item {
        // spacer item
        height: 35 * dp
    }

    Text {
      id: passwordlabel
      Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
      Layout.preferredHeight: font.height
      text: qsTr( "Password" )
      font: Theme.defaultFont
    }

    TextField {
      id: password
      echoMode: TextInput.Password
      Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
      Layout.preferredWidth: Math.max( parent.width / 2, usernamelabel.width )
      Layout.preferredHeight: font.height + 20 * dp
      height: font.height + 20 * dp
      font: Theme.defaultFont

      background: Rectangle {
        y: password.height - height - password.bottomPadding / 2
        implicitWidth: 120 * dp
        height: password.activeFocus ? 2 * dp : 1 * dp
        color: password.activeFocus ? "#4CAF50" : "#C8E6C9"
      }
    }

    Label {
        text: "Login Status " + connection.status
    }


    Button {
        text: "Logout"
        bgcolor: "white"
        visible: connection.status == 1

        onClicked: {
            connection.logout()
        }
    }

    Button {
        text: "Login"
        bgcolor: "white"
        visible: connection.status == 0

        onClicked: {
            connection.username = username.text
            connection.password = password.text
            connection.login()
        }
    }

    Button {
        text: "Refresh Projects"
        bgcolor: "white"
        enabled: connection.status == 1

        onClicked: {
            projectsModel.refreshProjectsList()
        }
    }

    QFieldCloudProjectsModel {
        id: projectsModel

        cloudConnection: connection

        onWarning: displayToast( message )
    }
/*
    Item {
        // spacer item
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
*/
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
  }

  onVisibleChanged: {
      if (visible) {
          username.forceActiveFocus();
      }
  }
}
