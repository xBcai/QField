import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import org.qfield 1.0
import Theme 1.0

Popup {
  id: popup

  padding: 0

  Page {
    anchors.fill: parent

    header: PageHeader {
      title: qsTr("QFieldCloud Sync")

      showApplyButton: false
      showCancelButton: {
        return true
      }

      onCancel: {
        popup.close()
      }
    }

    ScrollView {
      padding: 20
      ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
      ScrollBar.vertical.policy: ScrollBar.AsNeeded
      contentWidth: cloudGrid.width
      contentHeight: cloudGrid.height
      anchors.fill: parent
      clip: true

      GridLayout {
        id: cloudGrid
        width: parent.parent.width

        columns: 1
        columnSpacing: 2
        rowSpacing: 10

        Text {
            id: welcomeText
            text: qsTr('Welcome, ') + '<strong>' + cloudConnection.username + '</strong>'
            font: Theme.secondaryTitleFont
            fontSizeMode: Text.VerticalFit
            wrapMode: Text.WordWrap
        }

        Image {
          id: qfieldcloudLogo
          source: "qrc:/images/qfieldcloud-logo.png"
          Layout.bottomMargin: 20
          Layout.alignment: Qt.AlignHCenter
          fillMode: Image.PreserveAspectFit
          sourceSize.width: parent.width * 0.4 * screen.devicePixelRatio
        }

        Text {
          id: descriptionText
          font: Theme.defaultFont
          text: qsTr('The easiest way to transfer you project from QGIS to your device!')
          wrapMode: Text.WordWrap
          horizontalAlignment: Text.AlignHCenter
          Layout.bottomMargin: 20
          Layout.fillWidth: true
        }

        Text {
          id: chooseText
          font: Theme.tipFont
          text: qsTr('Choose an action:')
          wrapMode: Text.WordWrap
          horizontalAlignment: Text.AlignHCenter
          Layout.bottomMargin: 10
          Layout.fillWidth: true
        }

        QfButton {
          id: syncButton
          Layout.fillWidth: true
          font: Theme.defaultFont
          text: "Sync!"

          onClicked: cloudProjectsModel.uploadProject(cloudProjectsModel.currentCloudProjectId)
        }

        Text {
          id: syncText
          font: Theme.tipFont
          color: Theme.gray
          text: qsTr('Synchronize the whole project with all modified features and download the freshly updated project with all the applied changes from QFieldCLoud.')
          wrapMode: Text.WordWrap
          horizontalAlignment: Text.AlignHCenter
          Layout.bottomMargin: 20
          Layout.fillWidth: true
        }

        QfButton {
          id: pushButton
          Layout.fillWidth: true
          font: Theme.defaultFont
          text: "Push"

          onClicked: {
            console.log()
          }
        }

        Text {
          id: pushText
          font: Theme.tipFont
          color: Theme.gray
          text: qsTr('Save internet bandwidth by only pushing the local features and pictures to the cloud, without updating the whole project.')
          wrapMode: Text.WordWrap
          horizontalAlignment: Text.AlignHCenter
          Layout.bottomMargin: 20
          Layout.fillWidth: true
        }

        QfButton {
          id: discardButton
          Layout.fillWidth: true
          font: Theme.defaultFont
          text: "Discard local changes"

          onClicked: {
            console.log()
          }
        }

        Text {
          id: discardText
          font: Theme.tipFont
          color: Theme.gray
          text: qsTr('Revert all modified features in the cloud layers. You cannot restore those changes!')
          wrapMode: Text.WordWrap
          horizontalAlignment: Text.AlignHCenter
          Layout.bottomMargin: 10
          Layout.fillWidth: true
        }
      }
    }
  }

  Connections {
    target: cloudConnection

    function onLoginChanged() {
      if(cloudConnection.status === QFieldCloudConnection.LoggedIn) {
        welcomeText.text = qsTr('Welcome, ') + '<strong>' + cloudConnection.username + '</strong>'
      } else {
        visible = false
        displayToast(qsTr('Not logged in'))
      }
    }
  }

  function show() {
    visible = cloudConnection.status === QFieldCloudConnection.LoggedIn

    if (!visible)
      displayToast(qsTr('Not logged in'))
  }
}
