import bb.cascades 1.3
import Berrygram 1.0
import "Ui.js" as Ui
Container {
    id: avatar
    property string title: ""
    property string chatId: ""
    property variant imageProvider: null
    property string avatarKey: ""
    property real size: ui.du(8)
    preferredWidth: size
    preferredHeight: size
    minWidth: size
    maxWidth: size
    minHeight: size
    maxHeight: size
    layout: DockLayout {}
    attachedObjects: [ AvatarImage {id: picture;provider: avatar.imageProvider;key: avatar.avatarKey} ]
    ImageView {
        imageSource: picture.source.length>0?picture.source:Ui.avatar(avatar.chatId)
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        scalingMethod: ScalingMethod.AspectFit
    }
    Label {
        visible: picture.source.length===0
        text: Ui.initials(avatar.title)
        textFormat: TextFormat.Plain
        textStyle.color: Color.White
        textStyle.fontWeight: FontWeight.W500
        horizontalAlignment: HorizontalAlignment.Center
        verticalAlignment: VerticalAlignment.Center
    }
}
