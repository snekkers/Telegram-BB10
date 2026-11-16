import bb.cascades 1.3
import "Ui.js" as Ui

Container {
    id: avatar
    property string title: ""
    property string chatId: ""
    preferredWidth: ui.du(8)
    preferredHeight: ui.du(8)
    minWidth: ui.du(8)
    maxWidth: ui.du(8)
    layout: DockLayout {}
    ImageView {
        imageSource: Ui.avatar(avatar.chatId)
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        scalingMethod: ScalingMethod.AspectFit
    }
    Label {
        text: Ui.initials(avatar.title)
        textFormat: TextFormat.Plain
        textStyle.color: Color.White
        textStyle.fontWeight: FontWeight.W500
        horizontalAlignment: HorizontalAlignment.Center
        verticalAlignment: VerticalAlignment.Center
    }
}
