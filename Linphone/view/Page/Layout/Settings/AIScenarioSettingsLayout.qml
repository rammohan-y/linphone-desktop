
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
			title: "AI Scenarios",
			subTitle: "Define reusable call scripts linked to an AI vendor",
			contentComponent: scenarioListComponent
		}
	]
	onSave: {
		SettingsCpp.save()
	}
	onUndo: SettingsCpp.undo()

	property int editingScenarioIndex: -1

	Component {
		id: scenarioListComponent
		ColumnLayout {
			spacing: Utils.getSizeWithScreenRatio(10)

			Repeater {
				model: SettingsCpp.aiScenarios
				delegate: Rectangle {
					Layout.fillWidth: true
					height: scenarioRow.implicitHeight + Utils.getSizeWithScreenRatio(16)
					radius: Utils.getSizeWithScreenRatio(8)
					border.color: DefaultStyle.grey_200
					border.width: 1
					color: "transparent"

					RowLayout {
						id: scenarioRow
						anchors.fill: parent
						anchors.margins: Utils.getSizeWithScreenRatio(8)

						ColumnLayout {
							Layout.fillWidth: true
							spacing: 2
							Text {
								text: modelData.name || "Unnamed Scenario"
								font: Typography.p2l
								color: DefaultStyle.main2_600
							}
							Text {
								property var agentNames: SettingsCpp.getAgentNames()
								text: {
									var idx = modelData.agentIndex || 0
									var name = (agentNames && idx < agentNames.length) ? agentNames[idx] : "No vendor"
									return "Vendor: " + name
								}
								font.pixelSize: Typography.p1.pixelSize
								font.italic: true
								color: DefaultStyle.main2_400
							}
						}

						RoundButton {
							style: ButtonStyle.noBackground
							icon.source: AppIcons.pencil
							onClicked: {
								mainItem.editingScenarioIndex = index
								scenarioDialog.scenarioData = Object.assign({}, modelData)
								scenarioDialog.open()
							}
						}
						RoundButton {
							style: ButtonStyle.noBackground
							icon.source: AppIcons.trashCan
							onClicked: {
								SettingsCpp.removeAiScenario(index)
								SettingsCpp.save()
							}
						}
					}
				}
			}

			MediumButton {
				Layout.topMargin: Utils.getSizeWithScreenRatio(10)
				style: ButtonStyle.secondary
				text: "+ Add Scenario"
				onClicked: {
					mainItem.editingScenarioIndex = -1
					scenarioDialog.scenarioData = {
						"name": "",
						"agentIndex": 0,
						"systemPrompt": ""
					}
					scenarioDialog.open()
				}
			}

			Control.Popup {
				id: scenarioDialog
				anchors.centerIn: parent
				modal: true
				width: Utils.getSizeWithScreenRatio(500)
				height: scenarioDialogContent.implicitHeight + Utils.getSizeWithScreenRatio(40)
				padding: Utils.getSizeWithScreenRatio(20)
				property var scenarioData: ({})

				onOpened: {
					scenarioNameField.text = scenarioData.name || ""
					agentComboBox.currentIndex = scenarioData.agentIndex || 0
					scenarioPromptArea.text = scenarioData.systemPrompt || ""
				}

				background: Rectangle {
					radius: Utils.getSizeWithScreenRatio(12)
					color: DefaultStyle.grey_0
					border.color: DefaultStyle.grey_200
					border.width: 1
				}

				contentItem: ColumnLayout {
					id: scenarioDialogContent
					spacing: Utils.getSizeWithScreenRatio(12)

					Text {
						text: mainItem.editingScenarioIndex >= 0 ? "Edit Scenario" : "New Scenario"
						font: Typography.h4
						color: DefaultStyle.main2_600
					}

					TextField {
						id: scenarioNameField
						Layout.fillWidth: true
						placeholderText: "Scenario name (e.g. Book Dentist)"
					}

					ColumnLayout {
						Layout.fillWidth: true
						spacing: Utils.getSizeWithScreenRatio(4)
						Text {
							text: "AI Vendor"
							font: Typography.p2l
							color: DefaultStyle.main2_600
						}
						ComboBox {
							id: agentComboBox
							Layout.fillWidth: true
							Layout.preferredHeight: Utils.getSizeWithScreenRatio(49)
							model: SettingsCpp.getAgentNames()
							oneLine: true
						}
					}

					ColumnLayout {
						Layout.fillWidth: true
						spacing: Utils.getSizeWithScreenRatio(4)
						Text {
							text: "System Prompt"
							font: Typography.p2l
							color: DefaultStyle.main2_600
						}
						Control.ScrollView {
							Layout.fillWidth: true
							Layout.preferredHeight: Utils.getSizeWithScreenRatio(120)
							Control.TextArea {
								id: scenarioPromptArea
								wrapMode: Text.Wrap
								placeholderText: "e.g. You are calling Dr. Sharma's clinic to book a general checkup..."
								font.pixelSize: Typography.p1.pixelSize
								color: DefaultStyle.main2_600
								background: Rectangle {
									radius: Utils.getSizeWithScreenRatio(8)
									border.color: DefaultStyle.grey_200
									border.width: 1
									color: "transparent"
								}
							}
						}
					}

					RowLayout {
						Layout.fillWidth: true
						Item { Layout.fillWidth: true }
						MediumButton {
							style: ButtonStyle.secondary
							text: "Cancel"
							onClicked: scenarioDialog.close()
						}
						MediumButton {
							style: ButtonStyle.main
							text: "Save"
							onClicked: {
								var scenario = {
									"name": scenarioNameField.text,
									"agentIndex": agentComboBox.currentIndex,
									"systemPrompt": scenarioPromptArea.text
								}
								if (mainItem.editingScenarioIndex >= 0) {
									SettingsCpp.updateAiScenario(mainItem.editingScenarioIndex, scenario)
								} else {
									SettingsCpp.addAiScenario(scenario)
								}
								SettingsCpp.save()
								scenarioDialog.close()
							}
						}
					}
				}
			}
		}
	}
}
