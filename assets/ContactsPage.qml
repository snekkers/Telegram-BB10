import bb.cascades 1.3
import "Ui.js" as Ui
Page {
    id: contactsPage
    objectName: "contactsPage"
    actionBarVisibility: ChromeVisibility.Hidden
    property bool livePage: false
    property bool waiting: false
    property variant source: livePage?_live:_chat
    property string errorText: source.contactsError
    onErrorTextChanged: if(errorText.length>0)waiting=false
    function focusComposer(){contactSearch.requestFocus();}
    function openContactChat(chatId) {
        waiting=false;
        if(navigation.top!==contactsPage || (livePage && !_live.ready))return;
        navigation.openChatById(chatId);
    }
    titleBar: AppHeader {caption: qsTr("Новое сообщение");detail: contactsPage.livePage?qsTr("Контакты Telegram"):qsTr("Демо · вымышленные контакты");compact: true;menuVisible: true;onMenuRequested: _ui.openMenu(contactsPage)}
    shortcuts: [Shortcut {key: "Escape";onTriggered: navigation.pop()}]
    actions: [ActionItem {objectName: "refreshContactsAction";title: qsTr("Обновить контакты");imageSource: "asset:///icons/history-white.png";enabled: !contactsPage.source.contactsLoading;onTriggered: contactsPage.source.loadContacts()}]
    Container {
        background: Color.White
        Container {
            leftPadding: ui.du(1.5);rightPadding: ui.du(1.5);topPadding: ui.du(1)
            TextField {id: contactSearch;objectName: "contactSearch";hintText: qsTr("Имя или @username");enabled: !contactsPage.waiting && !contactsPage.source.openingContact;onTextChanged: contactsPage.source.setContactQuery(text)}
            SegmentedControl {
                objectName: "contactTabs"
                enabled: !contactsPage.waiting && !contactsPage.source.openingContact
                Option {text: qsTr("Контакты");value: 0;selected: true}
                Option {text: qsTr("Недавние");value: 1}
                onSelectedValueChanged: contactsPage.source.setContactSection(selectedValue)
            }
        }
        Label {visible: contactsPage.source.contactsLoading || contactsPage.waiting || contactsPage.source.openingContact;text: contactsPage.waiting || contactsPage.source.openingContact?qsTr("Открываем диалог…"):qsTr("Загрузка контактов…");textStyle.color: Color.create(Ui.secondary);horizontalAlignment: HorizontalAlignment.Center}
        Label {visible: contactsPage.errorText.length>0;text: contactsPage.errorText;textFormat: TextFormat.Plain;multiline: true;leftMargin: ui.du(2);rightMargin: ui.du(2)}
        ListView {
            property variant avatarProvider: _avatars
            id: contactsList
            objectName: "contactsList"
            enabled: !contactsPage.waiting && !contactsPage.source.openingContact && (!contactsPage.livePage || _live.ready)
            dataModel: contactsPage.source.contacts
            layoutProperties: StackLayoutProperties {spaceQuota: 1}
            stickToEdgePolicy: ListViewStickToEdgePolicy.Beginning
            function itemType(data,indexPath){return "contact";}
            listItemComponents: [ListItemComponent {type: "contact";ChatRow {row: ListItemData}}]
            onTriggered: {
                var row=dataModel.data(indexPath);
                if(contactsPage.source.openContact(row.userId,contactsPage))contactsPage.waiting=true;
            }
        }
        Label {visible: !contactsPage.source.contactsLoading && contactsPage.source.contactCount===0;text: qsTr("Контакты не найдены");textStyle.color: Color.create(Ui.secondary);horizontalAlignment: HorizontalAlignment.Center}
        Button {visible: contactsPage.errorText.length>0 && !contactsPage.source.contactsLoading;text: qsTr("Повторить");horizontalAlignment: HorizontalAlignment.Fill;onClicked: contactsPage.source.loadContacts()}
    }
}
