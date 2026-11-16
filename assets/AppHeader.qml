import bb.cascades 1.3
import "Ui.js" as Ui

TitleBar {
    id: header
    property string caption: "Telegram"
    property string detail: ""
    property bool compact: false
    property bool menuVisible: false
    property bool composeVisible: false
    property bool avatarVisible: false
    property string avatarKey: ""
    property bool detailActive: false
    signal composeRequested()
    signal menuRequested()
    visibility: ChromeVisibility.Visible
    scrollBehavior: TitleBarScrollBehavior.Sticky
    kind: TitleBarKind.FreeForm
    kindProperties: FreeFormTitleBarKindProperties {
        Container {
            background: Color.create(Ui.blue)
            preferredHeight: ui.du(header.compact ? 7.2 : 9)
            leftPadding: ui.du(2.4)
            rightPadding: ui.du(2.4)
            topPadding: ui.du(header.compact ? 0.3 : 1)
            bottomPadding: ui.du(header.compact ? 0.3 : 1)
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            Avatar {imageProvider: _avatars;visible: header.avatarVisible;title: header.caption;chatId: header.avatarKey;avatarKey: header.avatarKey;size: ui.du(5.6);rightMargin: ui.du(1.3);verticalAlignment: VerticalAlignment.Center}
            Container {
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                verticalAlignment: VerticalAlignment.Center
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
                    textStyle.color: Color.create(header.detailActive?"#ddffe0":"#e5f0f8")
                    topMargin: 0
                }
            }
            ImageButton {
                objectName: "newChatButton"
                visible: header.composeVisible
                preferredWidth: ui.du(6.2);preferredHeight: ui.du(6.2);minWidth: preferredWidth;maxWidth: preferredWidth
                verticalAlignment: VerticalAlignment.Center
                defaultImageSource: "asset:///icons/edit-white.png";pressedImageSource: "asset:///icons/more-disabled.png"
                accessibility.name: qsTr("Новое сообщение")
                onClicked: header.composeRequested()
            }
            ImageButton {
                objectName: "chatMenuButton"
                visible: header.menuVisible
                preferredWidth: ui.du(6.2); preferredHeight: ui.du(6.2)
                minWidth: preferredWidth; maxWidth: preferredWidth
                verticalAlignment: VerticalAlignment.Center
                defaultImageSource: "asset:///icons/more-white.png"
                pressedImageSource: "asset:///icons/more-disabled.png"
                accessibility.name: qsTr("Действия чата")
                onClicked: header.menuRequested()
            }
        }
    }
}
