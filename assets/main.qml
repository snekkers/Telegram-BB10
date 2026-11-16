import bb.cascades 1.3
import "Ui.js" as Ui

NavigationPane {
    id: navigation
    property bool live: _live.ready
    property bool demo: false
    property bool showSearch: false
    function openLogin() {
        var page = loginPageDefinition.createObject();
        if (page) {
            page.closeRequested.connect(function() { navigation.pop(); });
            navigation.push(page);
        }
    }
    function openChatById(chatId) {
        var source = navigation.live ? _live : _chat;
        if (source.openChat(chatId)) {
            var page = chatPageDefinition.createObject();
            if (page) { page.liveChat = navigation.live; navigation.push(page); }
            else source.closeChat();
        }
    }
    Page {
        actionBarVisibility: navigation.live || navigation.demo ? ChromeVisibility.Visible : ChromeVisibility.Hidden
        titleBar: AppHeader {
            caption: navigation.live || navigation.demo ? qsTr("Чаты") : "Berrygram"
            detail: navigation.live ? _live.connectionStatus : navigation.demo ? qsTr("Демо · без подключения") : qsTr("Telegram для BlackBerry Classic")
            compact: navigation.live || navigation.demo
        }
        shortcuts: [
            SystemShortcut { type: SystemShortcuts.Search; enabled: navigation.live; onTriggered: { navigation.showSearch = true; chatSearch.requestFocus(); } },
            Shortcut { key: "s"; enabled: navigation.live && !chatSearch.focused; onTriggered: { navigation.showSearch = true; chatSearch.requestFocus(); } },
            Shortcut { key: "u"; enabled: navigation.live && !chatSearch.focused; onTriggered: navigation.openChatById(_live.nextUnreadChat()) },
            Shortcut { key: "Escape"; enabled: navigation.showSearch; onTriggered: { navigation.showSearch = false; chatSearch.text = ""; chatList.requestFocus(); } }
        ]
        actions: [
            ActionItem { title: qsTr("Поиск"); imageSource: "asset:///icons/search-white.png"; enabled: navigation.live; ActionBar.placement: ActionBarPlacement.OnBar; onTriggered: { navigation.showSearch = !navigation.showSearch; if (navigation.showSearch) chatSearch.requestFocus(); else chatSearch.text = ""; } },
            ActionItem { title: navigation.live ? qsTr("Подключение") : qsTr("Войти в Telegram"); imageSource: "asset:///icons/settings-white.png"; ActionBar.placement: ActionBarPlacement.InOverflow; onTriggered: navigation.openLogin() },
            ActionItem { title: qsTr("Выйти из демо"); enabled: navigation.demo && !navigation.live; onTriggered: { navigation.demo = false; navigation.showSearch = false; } }
        ]
        Container {
            background: Color.White
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            Container {
                visible: !navigation.live && !navigation.demo
                leftPadding: ui.du(4); rightPadding: ui.du(4)
                topPadding: ui.du(6); bottomPadding: ui.du(3)
                Avatar { title: "Berrygram"; chatId: "berrygram"; horizontalAlignment: HorizontalAlignment.Center; bottomMargin: ui.du(2) }
                Label { text: "Berrygram"; textStyle.base: SystemDefaults.TextStyles.BigText; textStyle.color: Color.create(Ui.ink); horizontalAlignment: HorizontalAlignment.Center }
                Label {
                    text: qsTr("Ваши чаты. Знакомый Telegram.\nНа клавиатуре BlackBerry.")
                    multiline: true; textStyle.color: Color.create(Ui.secondary); textStyle.textAlign: TextAlign.Center
                    horizontalAlignment: HorizontalAlignment.Fill
                    bottomMargin: ui.du(3)
                }
                Button { text: qsTr("Начать общение"); horizontalAlignment: HorizontalAlignment.Fill; onClicked: navigation.openLogin() }
                Button { text: qsTr("Посмотреть интерфейс"); horizontalAlignment: HorizontalAlignment.Fill; onClicked: navigation.demo = true }
            }
            Container {
                visible: navigation.live || navigation.demo
                horizontalAlignment: HorizontalAlignment.Fill
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                Container {
                    visible: navigation.showSearch && navigation.live
                    leftPadding: ui.du(1.6); rightPadding: ui.du(1.6); topPadding: ui.du(1); bottomPadding: ui.du(1)
                    TextField { id: chatSearch; hintText: qsTr("Поиск чатов"); inputMode: TextFieldInputMode.Text; onTextChanged: _live.setSearchQuery(text) }
                }
                Label { visible: navigation.live && _live.errorText.length > 0; text: _live.errorText; textFormat: TextFormat.Plain; multiline: true; leftMargin: ui.du(2); rightMargin: ui.du(2); textStyle.color: Color.create("#a74132") }
                ListView {
                    id: chatList
                    objectName: "chatList"
                    dataModel: navigation.live ? _live.chats : _chat.chats
                    layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                    function itemType(data, indexPath) { return "chat"; }
                    listItemComponents: [ ListItemComponent { type: "chat"; ChatRow { row: ListItemData } } ]
                    onTriggered: navigation.openChatById(dataModel.data(indexPath).chatId)
                }
                Label { visible: navigation.live && !_live.loadingChats && _live.chatCount === 0; text: chatSearch.text.length > 0 ? qsTr("Ничего не найдено") : qsTr("Пока нет загруженных чатов"); textStyle.color: Color.create(Ui.secondary); horizontalAlignment: HorizontalAlignment.Center }
                Button { visible: navigation.live && _live.canLoadChats; text: _live.loadingChats ? qsTr("Загрузка…") : qsTr("Загрузить ещё"); enabled: !_live.loadingChats; horizontalAlignment: HorizontalAlignment.Fill; onClicked: _live.loadMoreChats() }
            }
        }
    }
    attachedObjects: [ ComponentDefinition { id: chatPageDefinition; source: "ChatPage.qml" }, ComponentDefinition { id: loginPageDefinition; source: "LoginPage.qml" } ]
    onPushTransitionEnded: {
        if (page.objectName === "chatPage" && page.focusComposer !== undefined) page.focusComposer();
    }
    onPopTransitionEnded: {
        if (page.objectName === "chatPage") { if (page.liveChat) _live.closeChat(); else _chat.closeChat(); }
        page.destroy();
    }
}
