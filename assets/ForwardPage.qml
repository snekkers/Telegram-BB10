import bb.cascades 1.3
import Berrygram 1.0
import "Ui.js" as Ui
Page {
    id: forwardPage
    objectName: "forwardPage"
    actionBarVisibility: ChromeVisibility.Hidden
    property bool liveChat: false
    property variant source: liveChat?_live:_chat
    property variant selection: source.forwardSelection
    property string targetId: ""
    property string targetTitle: ""
    titleBar: AppHeader {caption: qsTr("Переслать");detail: forwardPage.liveChat?qsTr("Выберите получателя"):qsTr("Демо · без отправки в Telegram");compact: true}
    Container {
        background: Color.create(_prefs.surface)
        Container {
            leftPadding: ui.du(2);rightPadding: ui.du(2);topPadding: ui.du(1)
            Label {text: forwardPage.selection.preview || "";textFormat: TextFormat.Plain;textStyle.base: SystemDefaults.TextStyles.SmallText;textStyle.color: Color.create(_prefs.secondary)}
            TextField {objectName: "recipientSearch";hintText: qsTr("Поиск получателя");enabled: forwardPage.selection.state==="ready";onTextChanged: forwardPage.source.setRecipientQuery(text)}
        }
        ListView {
            property variant avatarProvider: _avatars
            property variant preferences: _prefs
            objectName: "recipientList"
            enabled: forwardPage.selection.state==="ready"
            dataModel: forwardPage.source.recipients
            layoutProperties: StackLayoutProperties {spaceQuota: 1}
            stickToEdgePolicy: ListViewStickToEdgePolicy.Beginning
            attachedObjects: [
                ListScrollStateHandler {id: recipientScroll},
                AutoPager {
                    enabled: _live.foreground && forwardPage.liveChat && navigation.top===forwardPage && forwardPage.selection.state==="ready" && _live.canLoadRecipientChats && _live.errorText.length===0
                    atEdge: recipientScroll.atEnd || forwardPage.source.recipientCount===0
                    contentCount: forwardPage.source.recipientCount
                    onRequestMore: _live.loadMoreRecipientChats()
                }
            ]
            function itemType(data,indexPath){return "chat";}
            listItemComponents: [ ListItemComponent {type: "chat";ChatRow {row: ListItemData}} ]
            onTriggered: {var row=dataModel.data(indexPath);forwardPage.targetId=row.chatId;forwardPage.targetTitle=row.title;}
        }
        Label {visible: forwardPage.liveChat && _live.loadingChats;text: qsTr("Загрузка…");textStyle.color: Color.create(_prefs.secondary);horizontalAlignment: HorizontalAlignment.Center}
        Label {visible: forwardPage.source.recipientCount===0 && !forwardPage.source.canLoadRecipientChats;text: qsTr("Получатели не найдены");textStyle.color: Color.create(_prefs.secondary);horizontalAlignment: HorizontalAlignment.Center}
        Label {visible: forwardPage.targetId.length>0;text: qsTr("Кому: ")+forwardPage.targetTitle;textFormat: TextFormat.Plain;leftMargin: ui.du(2)}
        Label {visible: (forwardPage.selection.status || "").length>0;text: forwardPage.selection.status || "";textFormat: TextFormat.Plain;multiline: true;leftMargin: ui.du(2);rightMargin: ui.du(2)}
        Button {
            objectName: "forwardConfirmButton"
            visible: forwardPage.selection.state==="ready"
            enabled: forwardPage.targetId.length>0
            text: qsTr("Переслать")
            horizontalAlignment: HorizontalAlignment.Fill
            onClicked: forwardPage.source.forwardTo(forwardPage.targetId)
        }
        Button {visible: forwardPage.selection.state!=="ready" && forwardPage.selection.state!=="sending";text: qsTr("Готово");horizontalAlignment: HorizontalAlignment.Fill;onClicked: navigation.pop()}
    }
}
