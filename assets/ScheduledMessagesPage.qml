import bb.cascades 1.3
Page {
    objectName: "scheduledMessagesPage"
    actionBarVisibility: ChromeVisibility.Hidden
    onCreationCompleted: _live.loadScheduled()
    titleBar: AppHeader {caption: qsTr("Отложенные сообщения");compact: true}
    Container {
        background: Color.create(_prefs.surface)
        Button {text: qsTr("Обновить список");enabled: !_live.scheduledBusy;onClicked: _live.loadScheduled()}
        Label {text: _live.scheduledStatus;multiline: true;leftMargin: ui.du(2);textStyle.color: Color.create(_prefs.secondary)}
        ListView {
            dataModel: _live.scheduledMessages
            layoutProperties: StackLayoutProperties {spaceQuota: 1}
            function itemType(data,indexPath){return "message";}
            listItemComponents: [ListItemComponent {type: "message"
                StandardListItem {title: ListItemData.title;description: ListItemData.sendTime}
            }]
            onTriggered: navigation.openScheduledEditor(dataModel.data(indexPath))
        }
    }
}
