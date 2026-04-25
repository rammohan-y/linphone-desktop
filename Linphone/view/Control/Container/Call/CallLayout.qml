import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQml.Models
import QtQuick.Controls.Basic as Control
import Linphone
import EnumsToStringCpp 1.0
import UtilsCpp 1.0
import SettingsCpp 1.0
import "qrc:/qt/qml/Linphone/view/Control/Tool/Helper/utils.js" as Utils

// =============================================================================

Item {
	id: mainItem
	property CallGui call
	property ConferenceGui conference: call && call.core.conference
	property bool callTerminatedByUser: false
	property bool callStarted: call? call.core.isStarted : false
	readonly property var callState: call?.core.state
	onCallStateChanged: if (callState === LinphoneEnums.CallState.End || callState === LinphoneEnums.CallState.Released) preview.visible = false
	property int conferenceLayout: call ? call.core.conferenceVideoLayout : LinphoneEnums.ConferenceLayout.ActiveSpeaker
	property int participantDeviceCount: conference ? conference.core.participantDeviceCount : -1
	property int lastConfLayoutBeforeSharing: -1
	property int _callLayoutAssignSeq: 0
	property real _callLayoutSourceSetAt: 0
	property string localAddress: call 
		? call.conference
			? call.conference.core.me.core.sipAddress
			: call.core.localAddress
		: ""
	onParticipantDeviceCountChanged: {
		setConferenceLayout()
	}
	Component.onCompleted: setConferenceLayout()
	onConferenceLayoutChanged: {
		console.log("CallLayout change : " +conferenceLayout)
		setConferenceLayout()
	}

	Connections {
		target: mainItem.conference? mainItem.conference.core : null
		function onIsScreenSharingEnabledChanged() {
			setConferenceLayout()
		}
		function onIsLocalScreenSharingChanged() {
			if (mainItem.conference.core.isLocalScreenSharing) {
				mainItem.lastConfLayoutBeforeSharing = mainItem.conferenceLayout
			}
			setConferenceLayout()
		}
	}

	function setConferenceLayout() {
		var scheduleT = Date.now()
		Qt.callLater(function() {
			var firedT = Date.now()
			var schedDelay = firedT - scheduleT
			console.log("[CallLoader-Debug] setConferenceLayout callLater_fired sched_delay_ms=", schedDelay, " callState=",
				mainItem.callState, " partDevCount=", mainItem.participantDeviceCount, " confLayout=",
				mainItem.conferenceLayout)
			var assignT0 = Date.now()
			callLayout.sourceComponent = undefined	// unload old view before opening the new view to avoid conflicts in Video UI.
			// If stop sharing screen, reset conference layout to the previous one
			if (mainItem.conference && !mainItem.conference.core.isLocalScreenSharing && mainItem.lastConfLayoutBeforeSharing !== -1) {
				mainItem.conferenceLayout = mainItem.lastConfLayoutBeforeSharing
				mainItem.lastConfLayoutBeforeSharing = -1
			}
			callLayout.sourceComponent = conference
				? conference.core.isScreenSharingEnabled || (mainItem.conferenceLayout == LinphoneEnums.ConferenceLayout.ActiveSpeaker && participantDeviceCount > 1)
					? activeSpeakerComponent
					: participantDeviceCount <= 1
						? waitingForOthersComponent
						: gridComponent
				: activeSpeakerComponent
			var assignDt = Date.now() - assignT0
			mainItem._callLayoutAssignSeq += 1
			mainItem._callLayoutSourceSetAt = Date.now()
			if (assignDt > 0)
				console.log("[CallLoader-Debug] setConferenceLayout source_reassigned assign_seq=",
					mainItem._callLayoutAssignSeq, " assign_js_ms=", assignDt)
		})
	}

	Text {
		id: callTerminatedText
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.top: parent.top
        anchors.topMargin: Utils.getSizeWithScreenRatio(25)
		z: 1
		visible: mainItem.callState === LinphoneEnums.CallState.End || mainItem.callState === LinphoneEnums.CallState.Error || mainItem.callState === LinphoneEnums.CallState.Released
		text: mainItem.conference
                //: "Vous avez quitté la conférence"
                ? qsTr("meeting_event_conference_destroyed")
                : mainItem.callTerminatedByUser
                    //: "Vous avez terminé l'appel"
                    ? qsTr("call_ended_by_user")
                    : mainItem.callStarted
                        //: "Votre correspondant a terminé l'appel"
                        ? qsTr("call_ended_by_remote")
						: call && call.core.lastErrorMessage || ""
		color: DefaultStyle.grey_0
		font {
            pixelSize: Utils.getSizeWithScreenRatio(22)
            weight: Utils.getSizeWithScreenRatio(300)
		}
	}
	
	Loader{
		id: callLayout
		anchors.fill: parent
		sourceComponent: mainItem.participantDeviceCount === 0
			? waitingForOthersComponent
			: activeSpeakerComponent
		onStatusChanged: {
			if (status === Loader.Ready && mainItem._callLayoutSourceSetAt > 0) {
				var t = Date.now() - mainItem._callLayoutSourceSetAt
				console.log("[CallLoader-Debug] callLayout Ready from_last_source_set_ms=", t, " seq=",
					mainItem._callLayoutAssignSeq, " callState=", mainItem.callState)
			} else
				console.log("[CallLoader-Debug] callLayout status=", status, " (not end-to-end timing) callState=",
					mainItem.callState, " seq=", mainItem._callLayoutAssignSeq)
		}
	}

	Sticker {
		id: preview
		qmlName: 'P'
		previewEnabled: true
        visible: (callLayout.sourceComponent === activeSpeakerComponent || callLayout.sourceComponent === waitingForOthersComponent)
		&& mainItem.callState !== LinphoneEnums.CallState.OutgoingProgress
        && mainItem.callState !== LinphoneEnums.CallState.OutgoingRinging
        && mainItem.callState !== LinphoneEnums.CallState.OutgoingInit
		&& !mainItem.conference?.core.isScreenSharingEnabled
		&& mainItem.participantDeviceCount <= 2
        height: Utils.getSizeWithScreenRatio(180)
        width: Utils.getSizeWithScreenRatio(300)
		anchors.right: mainItem.right
		anchors.bottom: mainItem.bottom
        anchors.rightMargin: Utils.getSizeWithScreenRatio(20)
        anchors.bottomMargin: Utils.getSizeWithScreenRatio(10)
		onVideoEnabledChanged: console.log("Preview : " +videoEnabled + " / " +visible +" / " +mainItem.call)
		property var accountObj: UtilsCpp.findLocalAccountByAddress(mainItem.localAddress)
        account: accountObj && accountObj.value || null
		call: mainItem.call
		displayAll: false
		displayPresence: false

		MovableMouseArea {
			id: previewMouseArea
			anchors.fill: parent
			movableArea: mainItem
            margin: Utils.getSizeWithScreenRatio(10)
			function resetPosition(){
				preview.anchors.right = mainItem.right
				preview.anchors.bottom = mainItem.bottom
				preview.anchors.rightMargin = previewMouseArea.margin
				preview.anchors.bottomMargin = previewMouseArea.margin
			}
			onVisibleChanged: if(!visible){
				resetPosition()
			}
			drag.target: preview
			onDraggingChanged: if(dragging) {
				preview.anchors.right = undefined
				preview.anchors.bottom = undefined
			}
			onRequestResetPosition: resetPosition()
		}
	}

	Component {
		id: waitingForOthersComponent
		Rectangle {
			color: DefaultStyle.grey_600
            radius: Utils.getSizeWithScreenRatio(15)
			ColumnLayout {
				anchors.centerIn: parent
                spacing: Utils.getSizeWithScreenRatio(22)
				width: waitText.implicitWidth
				Text {
					id: waitText
                    //: "En attente d'autres participants…"
                    text: qsTr("conference_call_empty")
                    Layout.preferredHeight: Utils.getSizeWithScreenRatio(67)
					Layout.alignment: Qt.AlignHCenter
					horizontalAlignment: Text.AlignHCenter
					color: DefaultStyle.grey_0
					font {
                        pixelSize: Utils.getSizeWithScreenRatio(30)
                        weight: Utils.getSizeWithScreenRatio(300)
					}
				}
				Item {
					Layout.fillWidth: true
					BigButton {
						color: pressed ? DefaultStyle.main2_200 : "transparent"
						borderColor: DefaultStyle.main2_400
						icon.source: AppIcons.shareNetwork
						contentImageColor: DefaultStyle.main2_400
                        //: "Partager le lien"
                        text: qsTr("conference_share_link_title")
						anchors.centerIn: parent
						textColor: DefaultStyle.main2_400
						onClicked: {
							if (mainItem.conference) {
								UtilsCpp.copyToClipboard(mainItem.conference.core.uri)
                                showInformationPopup(qsTr("copied"),
                                                     //: Le lien de la réunion a été copié dans le presse-papier
                                                     qsTr("information_popup_meeting_address_copied_to_clipboard"), true)
							}
						}
					}
				}
			}
		}
	}
	
	Component{
		id: activeSpeakerComponent
		ActiveSpeakerLayout{
			id: activeSpeaker
			Layout.fillWidth: true
			Layout.fillHeight: true
			call: mainItem.call

		}
	}
	Component{
		id: gridComponent
		CallGridLayout{
			Layout.fillWidth: true
			Layout.fillHeight: true
			call: mainItem.call
		}
	}
}
