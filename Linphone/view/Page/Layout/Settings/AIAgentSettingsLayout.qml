
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Control
import SettingsCpp 1.0
import Linphone
import UtilsCpp
import 'qrc:/qt/qml/Linphone/view/Style/buttonStyle.js' as ButtonStyle
import "qrc:/qt/qml/Linphone/view/Control/Tool/Helper/utils.js" as Utils

AbstractSettingsLayout {
	id: mainItem
	width: parent?.width
	contentModel: [
		{
			title: "AI Vendors",
			subTitle: "Configure AI vendor profiles for automated calls",
			contentComponent: agentListComponent
		}
	]
	onSave: {
		SettingsCpp.save()
	}
	onUndo: SettingsCpp.undo()

	property int editingAgentIndex: -1

	Component {
		id: agentListComponent
		ColumnLayout {
			spacing: Utils.getSizeWithScreenRatio(10)

			Repeater {
				model: SettingsCpp.aiAgents
				delegate: Rectangle {
					Layout.fillWidth: true
					height: agentRow.implicitHeight + Utils.getSizeWithScreenRatio(16)
					radius: Utils.getSizeWithScreenRatio(8)
					border.color: DefaultStyle.grey_200
					border.width: 1
					color: "transparent"

					RowLayout {
						id: agentRow
						anchors.fill: parent
						anchors.margins: Utils.getSizeWithScreenRatio(8)

						ColumnLayout {
							Layout.fillWidth: true
							spacing: 2
							Text {
								text: modelData.name || "Unnamed Vendor"
								font: Typography.p2l
								color: DefaultStyle.main2_600
							}
							Text {
								text: modelData.provider + " — " + modelData.model
								font.pixelSize: Typography.p1.pixelSize
								font.italic: true
								color: DefaultStyle.main2_400
							}
						}

						RoundButton {
							style: ButtonStyle.noBackground
							icon.source: AppIcons.pencil
							onClicked: {
								mainItem.editingAgentIndex = index
								agentDialog.agentData = Object.assign({}, modelData)
								agentDialog.open()
							}
						}
						RoundButton {
							style: ButtonStyle.noBackground
							icon.source: AppIcons.trashCan
							onClicked: {
								SettingsCpp.removeAiAgent(index)
								SettingsCpp.save()
							}
						}
					}
				}
			}

			MediumButton {
				Layout.topMargin: Utils.getSizeWithScreenRatio(10)
				style: ButtonStyle.secondary
				text: "+ Add Vendor"
				onClicked: {
					mainItem.editingAgentIndex = -1
					agentDialog.agentData = {
						"name": "",
						"provider": "gemini",
						"apiKey": "",
						"model": "gemini-2.0-flash-live-001",
						"voice": "Puck",
						"language": "en-US"
					}
					agentDialog.open()
				}
			}

			Control.Popup {
				id: agentDialog
				anchors.centerIn: parent
				modal: true
				width: Utils.getSizeWithScreenRatio(450)
				height: agentDialogContent.implicitHeight + Utils.getSizeWithScreenRatio(40)
				padding: Utils.getSizeWithScreenRatio(20)
				property var agentData: ({})

				onOpened: {
					agentNameField.text = agentData.name || ""
					agentApiKeyField.text = agentData.apiKey || ""
					agentModelField.text = agentData.model || ""
					agentVoiceField.text = agentData.voice || ""
					agentLanguageField.text = agentData.language || ""
					testResultText.text = ""
					testButton.testing = false
				}

				background: Rectangle {
					radius: Utils.getSizeWithScreenRatio(12)
					color: DefaultStyle.grey_0
					border.color: DefaultStyle.grey_200
					border.width: 1
				}

				contentItem: ColumnLayout {
					id: agentDialogContent
					spacing: Utils.getSizeWithScreenRatio(12)

					Text {
						text: mainItem.editingAgentIndex >= 0 ? "Edit Vendor" : "New Vendor"
						font: Typography.h4
						color: DefaultStyle.main2_600
					}

					TextField {
						id: agentNameField
						Layout.fillWidth: true
						placeholderText: "Display name (e.g. Gemini Flash)"
					}
					TextField {
						id: agentApiKeyField
						Layout.fillWidth: true
						placeholderText: "API Key"
						hidden: true
					}
					TextField {
						id: agentModelField
						Layout.fillWidth: true
						placeholderText: "Model (e.g. gemini-2.0-flash-live-001)"
					}
					TextField {
						id: agentVoiceField
						Layout.fillWidth: true
						placeholderText: "Voice (e.g. Puck)"
					}
					TextField {
						id: agentLanguageField
						Layout.fillWidth: true
						placeholderText: "Language (e.g. en-US)"
					}

					RowLayout {
						Layout.fillWidth: true
						spacing: Utils.getSizeWithScreenRatio(8)
						MediumButton {
							id: testButton
							style: ButtonStyle.secondary
							text: testButton.testing ? "Testing..." : "Test Connection"
							property bool testing: false
							enabled: !testing && agentApiKeyField.text.length > 0
							onClicked: {
								testButton.testing = true
								testResultText.text = ""
								var agent = {
									"name": agentNameField.text,
									"provider": "gemini",
									"apiKey": agentApiKeyField.text,
									"model": agentModelField.text || "gemini-2.0-flash-live-001",
									"voice": agentVoiceField.text || "Puck",
									"language": agentLanguageField.text || "en-US"
								}
								if (mainItem.editingAgentIndex >= 0) {
									SettingsCpp.updateAiAgent(mainItem.editingAgentIndex, agent)
									SettingsCpp.testAiAgent(mainItem.editingAgentIndex)
								} else {
									SettingsCpp.addAiAgent(agent)
									mainItem.editingAgentIndex = SettingsCpp.aiAgents.length - 1
									SettingsCpp.testAiAgent(mainItem.editingAgentIndex)
								}
							}
						}
						Text {
							id: testResultText
							Layout.fillWidth: true
							font.pixelSize: Typography.p1.pixelSize
							wrapMode: Text.Wrap
						}
						Connections {
							target: SettingsCpp
							function onAiAgentTestResult(success, message) {
								testButton.testing = false
								testResultText.text = message
								testResultText.color = success ? DefaultStyle.success_500_main : DefaultStyle.danger_500_main
							}
						}
					}

					RowLayout {
						Layout.fillWidth: true
						Item { Layout.fillWidth: true }
						MediumButton {
							style: ButtonStyle.secondary
							text: "Cancel"
							onClicked: agentDialog.close()
						}
						MediumButton {
							style: ButtonStyle.main
							text: "Save"
							onClicked: {
								var agent = {
									"name": agentNameField.text,
									"provider": "gemini",
									"apiKey": agentApiKeyField.text,
									"model": agentModelField.text || "gemini-2.0-flash-live-001",
									"voice": agentVoiceField.text || "Puck",
									"language": agentLanguageField.text || "en-US"
								}
								if (mainItem.editingAgentIndex >= 0) {
									SettingsCpp.updateAiAgent(mainItem.editingAgentIndex, agent)
								} else {
									SettingsCpp.addAiAgent(agent)
								}
								SettingsCpp.save()
								agentDialog.close()
							}
						}
					}
				}
			}
		}
	}
}
