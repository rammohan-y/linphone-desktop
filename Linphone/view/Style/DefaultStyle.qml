pragma Singleton
import QtQuick
import Linphone
import SettingsCpp

QtObject {

	property var currentTheme: Themes.themes.hasOwnProperty(SettingsCpp.themeMainColor)
							  ? Themes.themes[SettingsCpp.themeMainColor]
							  : Themes.themes["purple"]
    property var main1_100: currentTheme.main100
    property var main1_200: currentTheme.main200
    property var main1_300: currentTheme.main300
    property var main1_500_main: currentTheme.main500
    property var main1_600: currentTheme.main600
    property var main1_700: currentTheme.main700

    property var main2_0: "#FAF8FF"
    property var main2_100: "#F0EBF8"
    property var main2_200: "#E2DAF0"
    property var main2_300: "#C5BAD9"
    property var main2_400: "#9E93B5"
    property var main2_500_main: "#6E6587"
    property var main2_600: "#534B74"
    property var main2_700: "#3D3560"
    property var main2_800: "#2B234D"
    property var main2_900: "#2D2848"

    property var grey_0: "#EEEAF6"
    property var grey_100: "#E6E0F2"
    property var grey_200: "#D8D0E8"
    property var grey_300: "#BDB5CD"
    property var grey_400: "#8D86A0"
    property var grey_500: "#4D495A"
    property var grey_600: "#2D2A3A"
    property var grey_850: "#CCC5DA"
    property var grey_900: "#0A0810"
    property var grey_1000: "#000000"

    property var warning_600: "#DBB820"
    property var warning_700: "#AF9308"
    property var danger_500_main: "#DD5F5F"
    property var warning_500_main: "#FFDC2E"
    property var danger_700: "#9E3548"
    property var danger_900: "#723333"
    property var success_500_main: "#4FAE80"
    property var success_700: "#377d71"
    property var success_900: "#1E4C53"
    property var info_500_main: "#4AA8FF"
    property var info_800_main: "#02528D"

    property var vue_meter_light_green: "#6FF88D"
    property var vue_meter_dark_green: "#00D916"

    property real defaultHeight: 1007.0
    property real defaultWidth: 1512.0
    property real maxDp: 0.98
    property real dp: Math.min((Screen.width/Screen.height)/(defaultWidth/defaultHeight), maxDp)

    onDpChanged: {
        console.log("Screen ratio changed", dp)
        AppCpp.setScreenRatio(dp)
    }

    // Warning: Qt 6.8.1 (current version) and previous versions, Qt only support COLRv0 fonts. Don't try to use v1.
    property string emojiFont: "Noto Color Emoji"
    property string flagFont: "Noto Color Emoji"
    property string defaultFont: "Noto Sans"

    property var numericPadPressedButtonColor: "#F0EBF8"

    property var groupCallButtonColor: "#F0EBF8"

    property var placeholders: '#CACACA'	// No name in design
    
}
