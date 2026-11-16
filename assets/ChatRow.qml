import bb.cascades 1.3
import "Ui.js" as Ui

CustomListItem {
    id: item
    property variant row
    dividerVisible: true
    highlightAppearance: HighlightAppearance.Full
    preferredHeight: ui.du(10.4)
    Container {
        layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
        leftPadding: ui.du(2)
        rightPadding: ui.du(2)
        topPadding: ui.du(1.1)
        bottomPadding: ui.du(1.1)
        Avatar { title: item.row.title; chatId: item.row.chatId;avatarKey: item.row.avatarKey || "";imageProvider: item.ListItem.view?item.ListItem.view.avatarProvider:null; rightMargin: ui.du(1.8) }
        Container {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            topPadding: ui.du(0.1)
            Container {
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                horizontalAlignment: HorizontalAlignment.Fill
                Label {
                    text: item.row.title
                    textFormat: TextFormat.Plain
                    textStyle.color: Color.create(Ui.ink)
                    textStyle.fontWeight: (item.row.unreadLabel || "").length > 0 ? FontWeight.W600 : FontWeight.W400
                    layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                }
                ImageView {
                    visible: item.row.pinned === true
                    imageSource: "asset:///icons/pin-blue.png"
                    preferredWidth: ui.du(2.5); preferredHeight: ui.du(2.5)
                    verticalAlignment: VerticalAlignment.Center
                    accessibility.name: qsTr("Закреплённый чат")
                }
                Label {
                    text: item.row.time || ""
                    textFormat: TextFormat.Plain
                    textStyle.base: SystemDefaults.TextStyles.SmallText
                    textStyle.color: Color.create(Ui.secondary)
                    verticalAlignment: VerticalAlignment.Center
                }
            }
            Container {
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                horizontalAlignment: HorizontalAlignment.Fill
                Label {
                    text: item.row.typingText || item.row.preview || qsTr("Нет сообщений")
                    multiline: false
                    textFormat: TextFormat.Plain
                    textStyle.color: Color.create(item.row.statusActive?Ui.blue:Ui.secondary)
                    textStyle.base: SystemDefaults.TextStyles.SmallText
                    layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                }
                Container {
                    visible: (item.row.unreadLabel || "").length > 0
                    background: badge.imagePaint
                    minWidth: ui.du(3.4)
                    preferredHeight: ui.du(3.4)
                    leftPadding: ui.du(0.8)
                    rightPadding: ui.du(0.8)
                    layout: DockLayout {}
                    Label {
                        text: item.row.unreadLabel || ""
                        textStyle.color: Color.White
                        textStyle.base: SystemDefaults.TextStyles.SmallText
                        horizontalAlignment: HorizontalAlignment.Center
                        verticalAlignment: VerticalAlignment.Center
                    }
                }
            }
        }
    }
    attachedObjects: [ ImagePaintDefinition { id: badge; imageSource: "asset:///ui/badge.amd"; repeatPattern: RepeatPattern.Fill } ]
}
