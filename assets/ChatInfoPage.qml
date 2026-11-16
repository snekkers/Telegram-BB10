import bb.cascades 1.3
import Berrygram 1.0
import "Ui.js" as Ui
Page {
    id: infoPage
    objectName: "chatInfoPage"
    actionBarVisibility: ChromeVisibility.Hidden
    property bool liveChat: false
    property int section: 0
    property variant source: liveChat?_live:_chat
    property variant detail: source.chatInfo
    signal openMessage(string chatId,string messageId)
    titleBar: AppHeader { caption: qsTr("Сведения о чате");detail: infoPage.liveChat?(infoPage.detail.title || ""):qsTr("Демо · изменения локальные");compact: true }
    Container {
        background: Color.White
        SegmentedControl {
            objectName: "infoTabs"
            leftMargin: ui.du(1.5);rightMargin: ui.du(1.5);topMargin: ui.du(1)
            Option {text: qsTr("О чате");value: 0;selected: true}
            Option {text: qsTr("Участники");value: 1}
            Option {text: qsTr("Фото");value: 2}
            Option {text: qsTr("Файлы");value: 3}
            onSelectedValueChanged: {infoPage.section=selectedValue;infoPage.source.setInfoSection(infoPage.section);}
        }
        ScrollView {
            visible: infoPage.section===0
            layoutProperties: StackLayoutProperties {spaceQuota: 1}
            Container {
                leftPadding: ui.du(2);rightPadding: ui.du(2);topPadding: ui.du(2);bottomPadding: ui.du(2)
                Avatar {imageProvider: _avatars;title: infoPage.detail.title || "";chatId: infoPage.detail.chatId || "";avatarKey: infoPage.detail.avatarKey || "";horizontalAlignment: HorizontalAlignment.Center}
                Label {text: infoPage.detail.title || "";textFormat: TextFormat.Plain;textStyle.base: SystemDefaults.TextStyles.TitleText;horizontalAlignment: HorizontalAlignment.Center}
                Label {visible: infoPage.detail.group===true;text: (infoPage.detail.channel?qsTr("Подписчиков: "):qsTr("Участников: "))+(infoPage.detail.memberCount || "…");textStyle.color: Color.create(Ui.secondary)}
                Label {text: infoPage.detail.description || qsTr("Нет описания");textFormat: TextFormat.Plain;multiline: true}
                Button {objectName: "notificationsButton";horizontalAlignment: HorizontalAlignment.Fill;enabled: infoPage.detail.notificationsKnown===true;text: !enabled?qsTr("Уведомления: загрузка…"):infoPage.detail.notificationsOn?qsTr("Уведомления включены"):qsTr("Уведомления выключены");onClicked: infoPage.source.setNotifications(!infoPage.detail.notificationsOn)}
                Button {objectName: "pinChatButton";horizontalAlignment: HorizontalAlignment.Fill;text: infoPage.detail.pinned?qsTr("Открепить чат"):qsTr("Закрепить чат");onClicked: infoPage.source.setPinned(infoPage.detail.chatId,!infoPage.detail.pinned)}
                Button {objectName: "archiveChatButton";horizontalAlignment: HorizontalAlignment.Fill;text: infoPage.detail.archived?qsTr("Вернуть из архива"):qsTr("В архив");onClicked: infoPage.source.setArchived(infoPage.detail.chatId,!infoPage.detail.archived)}
            }
        }
        ListView {
            property variant avatarProvider: _avatars
            id: infoList
            objectName: "infoList"
            visible: infoPage.section>0
            dataModel: infoPage.source.infoRows
            layoutProperties: StackLayoutProperties {spaceQuota: 1}
            stickToEdgePolicy: ListViewStickToEdgePolicy.Beginning
            attachedObjects: [
                ListScrollStateHandler {id: infoScroll},
                AutoPager {
                    objectName: "infoPager"
                    enabled: infoPage.liveChat && navigation.top===infoPage && infoPage.section>0 && _live.canLoadInfo && _live.infoError.length===0
                    atEdge: infoScroll.atEnd || _live.infoCount===0
                    contentCount: _live.infoCount
                    onRequestMore: _live.loadMoreInfo()
                }
            ]
            function itemType(data,indexPath){return "row";}
            listItemComponents: [ ListItemComponent {type: "row";ChatRow {row: ListItemData}} ]
            onTriggered: {
                var row=dataModel.data(indexPath);
                if(infoPage.section>1 && (row.messageId || "").length>0)infoPage.openMessage(infoPage.detail.chatId,row.messageId);
            }
        }
        Label {visible: infoPage.source.infoLoading;text: qsTr("Загрузка…");textStyle.color: Color.create(Ui.secondary);horizontalAlignment: HorizontalAlignment.Center}
        Label {visible: infoPage.section>0 && !infoPage.source.infoLoading && infoPage.source.infoCount===0 && infoPage.source.infoError.length===0;text: infoPage.section===1?qsTr("Список участников пуст"):qsTr("Здесь пока нет общих медиа");horizontalAlignment: HorizontalAlignment.Center;textStyle.color: Color.create(Ui.secondary)}
        Label {visible: infoPage.source.infoError.length>0;text: infoPage.source.infoError;textFormat: TextFormat.Plain;multiline: true;leftMargin: ui.du(2);rightMargin: ui.du(2)}
        Label {visible: infoPage.liveChat && _live.errorText.length>0;text: _live.errorText;textFormat: TextFormat.Plain;multiline: true;leftMargin: ui.du(2);rightMargin: ui.du(2)}
        Button {visible: infoPage.source.infoError.length>0 && !infoPage.source.infoLoading && (infoPage.section===0 || infoPage.source.canLoadInfo);text: qsTr("Повторить");horizontalAlignment: HorizontalAlignment.Fill;onClicked: {if(infoPage.section===0)infoPage.source.loadChatInfo();else infoPage.source.loadMoreInfo();}}
    }
}
