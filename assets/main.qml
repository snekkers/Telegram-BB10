import bb.cascades 1.3
import Berrygram 1.0
import "Ui.js" as Ui

NavigationPane {
    id: navigation
    property bool live: _live.ready
    property bool demo: false
    property string authPhase: _telegram.phase
    property bool autoConfigAttempted: false
    property bool loginStarted: false
    property bool previewWelcome: false
    property bool restoringSession: !previewWelcome && !live && _telegram.available && _telegram.errorText.length === 0 && _telegram.hasSavedConfiguration && (authPhase==="stopped" || authPhase==="starting" || authPhase==="parameters")
    property string chatCategory: "personal"
    function applyCategory() { _live.setChatCategory(chatCategory); _chat.setChatCategory(chatCategory); }
    property bool archiveView: false
    function setArchive(value) { archiveView=value; _live.setArchiveView(value); _chat.setArchiveView(value); }
    function openChatInfo(isLive) {
        var source=isLive?_live:_chat; source.loadChatInfo();
        var page=infoPageDefinition.createObject();
        if(page) {
            page.liveChat=isLive;
            page.openMessage.connect(function(chatId,messageId) { navigation.pop(); source.openChatAt(chatId,messageId); });
            navigation.push(page);
        }
    }
    function openForward(isLive) {
        var source=isLive?_live:_chat;source.setRecipientQuery("");
        var page=forwardPageDefinition.createObject();if(page){page.liveChat=isLive;navigation.push(page);}
    }
    onCreationCompleted: {
        applyCategory()
        _live.configureAutoDownloads(_prefs.autoDownloadWifi,_prefs.autoDownloadMobile)
        if(_telegram.available && _telegram.phase==="stopped")_telegram.start()
        else applyAuthorization()
    }
    onAuthPhaseChanged: applyAuthorization()
    function applyAuthorization(){
        if(authPhase==="starting")autoConfigAttempted=false
        else if(authPhase==="parameters"){
            if(_telegram.hasSavedConfiguration){
                if(!_telegram.busy && !autoConfigAttempted){
                    autoConfigAttempted=true
                    if(!_telegram.useSavedConfiguration())openLogin()
                }
            }else if(loginStarted)openLogin()
        }else if(authPhase==="phone" || authPhase==="code" || authPhase==="password" || authPhase==="email" || authPhase==="emailCode" || authPhase==="failed" || authPhase==="unsupported")openLogin()
        else if(authPhase==="ready" && navigation.top && navigation.top.objectName==="loginPage")navigation.pop()
    }
    function openReactions(id){if(!_live.beginReactions(id))return;var page=reactionPageDefinition.createObject();if(page)navigation.push(page);else _live.cancelReactions();}
    function openSchedule(target){var page=schedulePageDefinition.createObject();if(page){page.chosen.connect(function(when){if(target.scheduleDraft(when))navigation.pop();});navigation.push(page);}}
    function previewFeature(name){
        if(!demo || _live.ready)return;
        var page;
        if(name==="voice"){if(!_voice.openCheckSample())return;page=voiceDefinition.createObject();}
        else if(name==="diagnostics")page=diagnosticsDefinition.createObject();
        else if(name==="downloads")page=downloadsDefinition.createObject();
        else if(name==="mute")page=muteDefinition.createObject();
        else if(name==="scheduled")page=scheduledDefinition.createObject();
        else if(name==="scheduled-editor"){page=scheduledEditorDefinition.createObject();if(page)page.row=({messageId:"7100",text:"Отложенное сообщение · демо",textMessage:true,sendDate:Math.floor(Date.now()/1000)+3600,sendTime:"Демо"});}
        else page=name==="schedule"?schedulePageDefinition.createObject():reactionPageDefinition.createObject();
        if(page)navigation.push(page);
    }
    function openVoice(id){if(!_voice.open(id))return;var page=voiceDefinition.createObject();if(page)navigation.push(page);else _voice.close();}
    function openDiagnostics(){var page=diagnosticsDefinition.createObject();if(page)navigation.push(page);}
    function openDownloads(){var page=downloadsDefinition.createObject();if(page)navigation.push(page);}
    function openMute(){var page=muteDefinition.createObject();if(page)navigation.push(page);}
    function openScheduled(){var page=scheduledDefinition.createObject();if(page)navigation.push(page);}
    function openScheduledEditor(row){var page=scheduledEditorDefinition.createObject();if(page){page.row=row;navigation.push(page);}}
    function openSettings(){var page=settingsPageDefinition.createObject();if(page)navigation.push(page);}
    function openEmoji(target) {
        var page=emojiPageDefinition.createObject();if(page){page.picked.connect(function(symbol){target.insertEmoji(symbol);});navigation.push(page);}
    }
    function openPhoto(id,isLive) {
        if(!_media.openPhoto(id,isLive))return;
        var page=photoPageDefinition.createObject();if(page)navigation.push(page);else _media.closePhoto();
    }
    function openSearch() {
        if (!live && !demo) return;
        _search.begin(!live);
        var page = searchPageDefinition.createObject();
        if (page) { page.demoSearch = !live; page.openResult.connect(function(chatId, messageId) { navigation.openChatById(chatId, messageId); }); navigation.push(page); }
    }
    function openContacts() {
        if(!live && !demo)return;
        var source=live?_live:_chat;source.setContactQuery("");source.setContactSection(0);source.loadContacts();
        var page=contactsPageDefinition.createObject();if(page){page.livePage=live;navigation.push(page);}
    }
    function openLogin() {
        loginStarted=true
        if(navigation.top && navigation.top.objectName==="loginPage")return
        var page = loginPageDefinition.createObject();
        if (page) {
            page.closeRequested.connect(function() { navigation.pop(); });
            navigation.push(page);
            page.beginLogin();
        }
    }
    function openChatById(chatId, messageId) {
        var source = navigation.live ? _live : _chat;
        if (messageId ? source.openChatAt(chatId, messageId) : source.openChat(chatId)) {
            var page = chatPageDefinition.createObject();
            if (page) { page.liveChat = navigation.live; navigation.push(page); }
            else source.closeChat();
        }
    }
    Page {
        id: chatsPage
        objectName: "chatsPage"
        actionBarVisibility: ChromeVisibility.Hidden
        titleBar: AppHeader {
            visibility: navigation.live || navigation.demo ? ChromeVisibility.Visible : ChromeVisibility.Hidden
            caption: navigation.live || navigation.demo ? (navigation.archiveView?qsTr("Архив"):qsTr("Чаты")) : "Telegram"
            detail: navigation.live ? (_notifications.unread>0?qsTr("Непрочитанных: ")+_notifications.unread+" · "+_live.connectionStatus:_live.connectionStatus) : navigation.demo ? qsTr("Демо · без подключения") : qsTr("Telegram для BlackBerry Classic")
            compact: navigation.live || navigation.demo
            composeVisible: navigation.live || navigation.demo
            onComposeRequested: navigation.openContacts()
            menuVisible: navigation.live || navigation.demo
            onMenuRequested: _ui.openMenu(chatsPage)
        }
        shortcuts: [
            SystemShortcut { type: SystemShortcuts.Search; enabled: (navigation.live || navigation.demo) && navigation.top===chatsPage; onTriggered: navigation.openSearch() },
            Shortcut { key: "s"; enabled: (navigation.live || navigation.demo) && navigation.top===chatsPage; onTriggered: navigation.openSearch() },
            Shortcut { key: "n";enabled: (navigation.live || navigation.demo) && navigation.top===chatsPage;onTriggered: navigation.openContacts() },
            Shortcut { key: "u"; enabled: navigation.live; onTriggered: navigation.openChatById(_live.nextUnreadChat()) }
        ]
        actions: [
            ActionItem {objectName: "preferencesAction";title: qsTr("Настройки");imageSource: "asset:///icons/settings-white.png";onTriggered: navigation.openSettings()},
            ActionItem {objectName: "newMessageAction";title: qsTr("Новое сообщение");imageSource: "asset:///icons/edit-white.png";enabled: navigation.live || navigation.demo;onTriggered: navigation.openContacts()},
            ActionItem { objectName: "globalSearchAction"; title: qsTr("Поиск"); imageSource: "asset:///icons/search-white.png"; enabled: (navigation.live || navigation.demo) && navigation.top===chatsPage; onTriggered: navigation.openSearch() },
            ActionItem { objectName: "archiveAction"; title: navigation.archiveView?qsTr("К чатам"):qsTr("Архив"); imageSource: "asset:///icons/archive-white.png"; enabled: navigation.live || navigation.demo; onTriggered: navigation.setArchive(!navigation.archiveView) },
            ActionItem { title: navigation.live ? qsTr("Подключение") : qsTr("Войти в Telegram"); imageSource: "asset:///icons/settings-white.png"; onTriggered: navigation.live ? navigation.openDiagnostics() : navigation.openLogin() },
            ActionItem { title: qsTr("Выйти из демо"); enabled: navigation.demo && !navigation.live; onTriggered: navigation.demo = false }
        ]
        Container {
            background: Color.create(_prefs.surface)
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            Container {
                objectName: "welcomeScreen"
                visible: !navigation.live && !navigation.demo
                background: Color.White
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
                layoutProperties: StackLayoutProperties {spaceQuota: 1}
                leftPadding: ui.du(4);rightPadding: ui.du(4);topPadding: ui.du(3);bottomPadding: ui.du(3)
                Container {layoutProperties: StackLayoutProperties {spaceQuota: 1}}
                ImageView {imageSource: "asset:///icon.png";scalingMethod: ScalingMethod.AspectFit;preferredWidth: ui.du(20);preferredHeight: ui.du(20);horizontalAlignment: HorizontalAlignment.Center;bottomMargin: ui.du(3)}
                Label {text: "Telegram";textStyle.fontSize: FontSize.PointValue;textStyle.fontSizeValue: 14;textStyle.fontWeight: FontWeight.W600;textStyle.color: Color.Black;horizontalAlignment: HorizontalAlignment.Center}
                Label {text: navigation.restoringSession?qsTr("Открываем ваши чаты…"):qsTr("Быстрый и безопасный обмен сообщениями.");multiline: true;textStyle.color: Color.Black;textStyle.textAlign: TextAlign.Center;horizontalAlignment: HorizontalAlignment.Fill;topMargin: ui.du(1)}
                Label {objectName: "startupError";visible: _telegram.errorText.length>0;text: _telegram.errorText;textFormat: TextFormat.Plain;multiline: true;textStyle.color: Color.create("#bc3e36");horizontalAlignment: HorizontalAlignment.Fill;bottomMargin: ui.du(1.5)}
                Container {layoutProperties: StackLayoutProperties {spaceQuota: 1}}
                ActivityIndicator {visible: navigation.restoringSession;running: visible;preferredWidth: ui.du(5);preferredHeight: ui.du(5);horizontalAlignment: HorizontalAlignment.Center}
                Button {objectName: "startMessagingButton";visible: !navigation.restoringSession;text: _telegram.errorText.length>0?qsTr("Повторить подключение"):qsTr("Начать общение");appearance: ControlAppearance.Primary;preferredHeight: ui.du(7.5);horizontalAlignment: HorizontalAlignment.Fill;enabled: _telegram.available && !_telegram.busy;onClicked: {if(_telegram.errorText.length>0 && _telegram.hasSavedConfiguration)_telegram.reconnect();else navigation.openLogin()}}
            }
            Container {
                visible: navigation.live || navigation.demo
                horizontalAlignment: HorizontalAlignment.Fill
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                Container {
                    leftPadding: ui.du(1.6); rightPadding: ui.du(1.6); topPadding: ui.du(1); bottomPadding: ui.du(0.5)
                    SegmentedControl {
                        objectName: "chatTabs"
                        horizontalAlignment: HorizontalAlignment.Fill
                        Option { text: qsTr("Личные") + " · " + (navigation.live ? _live.personalCount : _chat.personalCount); value: "personal"; selected: true }
                        Option { text: qsTr("Группы и каналы") + " · " + (navigation.live ? _live.communityCount : _chat.communityCount); value: "community" }
                        onSelectedValueChanged: { navigation.chatCategory = selectedValue; navigation.applyCategory(); }
                    }
                }
                Label { visible: navigation.live && _live.errorText.length > 0; text: _live.errorText; textFormat: TextFormat.Plain; multiline: true; leftMargin: ui.du(2); rightMargin: ui.du(2); textStyle.color: Color.create("#a74132") }
                ListView {
            property variant avatarProvider: _avatars
            property variant preferences: _prefs
                    id: chatList
                    objectName: "chatList"
                    dataModel: navigation.live ? _live.chats : _chat.chats
                    layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                    stickToEdgePolicy: ListViewStickToEdgePolicy.Beginning
                    attachedObjects: [
                        ListScrollStateHandler { id: chatScroll; objectName: "chatScroll" },
                        AutoPager {
                            objectName: "chatPager"
                            enabled: _live.foreground && navigation.live && navigation.top === chatsPage && _live.canLoadChats && _live.errorText.length === 0
                            atEdge: chatScroll.atEnd || _live.chatCount === 0
                            contentCount: _live.chatCount
                            onRequestMore: _live.loadMoreChats()
                        }
                    ]
                    function itemType(data, indexPath) { return "chat"; }
                    function pinChat(id,value) { (navigation.live?_live:_chat).setPinned(id,value); }
                    function archiveChat(id,value) { (navigation.live?_live:_chat).setArchived(id,value); }
                    listItemComponents: [ ListItemComponent { type: "chat"; ChatRow {
                        id: chatItem
                        row: ListItemData
                        contextActions: [ ActionSet {
                            title: ListItemData.title
                            ActionItem { title: ListItemData.pinned?qsTr("Открепить"):qsTr("Закрепить"); imageSource: "asset:///icons/pin-white.png"; onTriggered: chatItem.ListItem.view.pinChat(ListItemData.chatId,!ListItemData.pinned) }
                            ActionItem { title: ListItemData.archived?qsTr("Из архива"):qsTr("В архив"); imageSource: "asset:///icons/archive-white.png"; onTriggered: chatItem.ListItem.view.archiveChat(ListItemData.chatId,!ListItemData.archived) }
                        } ]
                    } } ]
                    onTriggered: navigation.openChatById(dataModel.data(indexPath).chatId)
                }
                Label {
                    visible: (navigation.live ? !_live.loadingChats && _live.chatCount === 0 : _chat.chatCount === 0)
                    text: navigation.chatCategory === "personal" ? qsTr("Пока нет личных чатов") : qsTr("Пока нет групп и каналов")
                    textStyle.color: Color.create(_prefs.secondary); horizontalAlignment: HorizontalAlignment.Center
                }
                Label { visible: navigation.live && _live.loadingChats; text: qsTr("Загрузка…"); textStyle.color: Color.create(_prefs.secondary); horizontalAlignment: HorizontalAlignment.Center }
                Button { visible: navigation.live && _live.errorText.length > 0 && _live.canLoadChats; text: qsTr("Повторить загрузку"); horizontalAlignment: HorizontalAlignment.Fill; onClicked: _live.loadMoreChats() }
            }
        }
    }
    attachedObjects: [
        ComponentDefinition {id: voiceDefinition;source: "VoicePage.qml"},
        ComponentDefinition {id: diagnosticsDefinition;source: "DiagnosticsPage.qml"},
        ComponentDefinition {id: downloadsDefinition;source: "DownloadsPage.qml"},
        ComponentDefinition {id: muteDefinition;source: "MutePage.qml"},
        ComponentDefinition {id: scheduledDefinition;source: "ScheduledMessagesPage.qml"},
        ComponentDefinition {id: scheduledEditorDefinition;source: "ScheduledEditorPage.qml"},
        ComponentDefinition {id: schedulePageDefinition;source: "SchedulePage.qml"}, ComponentDefinition {id: reactionPageDefinition;source: "ReactionPage.qml"}, ComponentDefinition {id: settingsPageDefinition;source: "SettingsPage.qml"}, ComponentDefinition {id: emojiPageDefinition;source: "EmojiPage.qml"}, ComponentDefinition {id: photoPageDefinition;source: "PhotoPage.qml"}, ComponentDefinition { id: chatPageDefinition; source: "ChatPage.qml" }, ComponentDefinition { id: loginPageDefinition; source: "LoginPage.qml" }, ComponentDefinition { id: searchPageDefinition; source: "SearchPage.qml" }, ComponentDefinition { id: infoPageDefinition; source: "ChatInfoPage.qml" }, ComponentDefinition { id: forwardPageDefinition; source: "ForwardPage.qml" }, ComponentDefinition {id: contactsPageDefinition;source: "ContactsPage.qml"} ]
    onPushTransitionEnded: {
        if ((page.objectName === "chatPage" || page.objectName === "searchPage" || page.objectName === "contactsPage") && page.focusComposer !== undefined) page.focusComposer();
    }
    onPopTransitionEnded: {
        if (page.objectName === "chatPage") { page.saveViewState(); if (page.liveChat) _live.closeChat(); else _chat.closeChat(); }
        if (page.objectName === "searchPage") _search.cancel();
        if (page.objectName === "contactsPage") (page.livePage?_live:_chat).cancelContactOpen();
        if (page.objectName === "forwardPage") (page.liveChat?_live:_chat).setRecipientQuery("");
        if(page.objectName==="emojiPage"){_emoji.release();if(navigation.top && navigation.top.objectName==="chatPage")navigation.top.focusAfterEmoji();}
        if(page.objectName==="photoPage")_media.closePhoto();
        if(page.objectName==="voicePage")_voice.close();
        if(page.objectName==="reactionPage")_live.cancelReactions();
        if(page.objectName==="loginPage" && navigation.authPhase!=="ready")navigation.autoConfigAttempted=false;
        if(page.objectName==="scheduledMessagesPage")_live.closeScheduled();
        page.destroy();
    }
}
