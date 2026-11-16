import bb.cascades 1.3
import "Ui.js" as Ui

TitleBar {
    id: header
    property string caption: "Berrygram"
    property string detail: ""
    kind: TitleBarKind.FreeForm
    kindProperties: FreeFormTitleBarKindProperties {
        Container {
            background: Color.create(Ui.blue)
            preferredHeight: ui.du(9)
            leftPadding: ui.du(2.4)
            rightPadding: ui.du(2.4)
            topPadding: ui.du(1)
            bottomPadding: ui.du(1)
            Label {
                text: header.caption
                textFormat: TextFormat.Plain
                textStyle.color: Color.White
                textStyle.base: SystemDefaults.TextStyles.TitleText
                bottomMargin: 0
            }
            Label {
                visible: header.detail.length > 0
                text: header.detail
                textFormat: TextFormat.Plain
                textStyle.base: SystemDefaults.TextStyles.SmallText
                textStyle.color: Color.create("#e5f0f8")
                topMargin: 0
            }
        }
    }
}
