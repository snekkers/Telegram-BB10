import bb.cascades 1.3
SceneCover {
    content: Container {
        background: Color.create("#5288b7")
        layout: DockLayout {}
        Container {
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            Label {text: "Telegram";textStyle.color: Color.White;textStyle.fontSize: FontSize.Large}
            Label {text: _notifications.unread>0?qsTr("Непрочитанных: ")+_notifications.unread:qsTr("Нет новых сообщений");textStyle.color: Color.White;textStyle.fontSize: FontSize.Small;multiline: true}
        }
    }
}
