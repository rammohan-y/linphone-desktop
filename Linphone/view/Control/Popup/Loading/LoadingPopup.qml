import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Linphone
import 'qrc:/qt/qml/Linphone/view/Style/buttonStyle.js' as ButtonStyle
import "qrc:/qt/qml/Linphone/view/Control/Tool/Helper/utils.js" as Utils

Popup {
	id: mainItem
	property string text
	property bool cancelButtonVisible: false
	property var callback
	property double __callLoaderOpenT0: 0
	modal: true
	closePolicy: Control.Popup.NoAutoClose
	anchors.centerIn: parent
	onOpened: {
		__callLoaderOpenT0 = Date.now()
		console.log("[CallLoader-Debug] LoadingPopup opened text=\"", mainItem.text, "\"")
	}
	onClosed: {
		var w = (Date.now() - __callLoaderOpenT0)
		console.log("[CallLoader-Debug] LoadingPopup closed text=\"", mainItem.text, "\" was_visible_ms=", w)
	}
    padding: Utils.getSizeWithScreenRatio(20)
	underlineColor: DefaultStyle.main1_500_main
    radius: Utils.getSizeWithScreenRatio(15)
	// onAboutToShow: width = contentText.implicitWidth
	contentItem: ColumnLayout {
        spacing: Utils.getSizeWithScreenRatio(15)
		// width: childrenRect.width
		// height: childrenRect.height
		BusyIndicator{
			Layout.alignment: Qt.AlignHCenter
			width: Utils.getSizeWithScreenRatio(33)
			height: width
            Layout.preferredWidth: width
            Layout.preferredHeight: width
		}
		Text {
			id: contentText
			Layout.alignment: Qt.AlignHCenter
			Layout.fillWidth: true
			Layout.fillHeight: true
			text: mainItem.text
            font.pixelSize: Utils.getSizeWithScreenRatio(14)
		}
		MediumButton {
			visible: mainItem.cancelButtonVisible
			Layout.alignment: Qt.AlignHCenter
            text: qsTr("cancel")
			style: ButtonStyle.main
			onClicked: {
				if (callback) mainItem.callback()
				mainItem.close()
			}
		}
	}
}
