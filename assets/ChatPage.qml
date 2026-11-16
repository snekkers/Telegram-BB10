import bb.cascades 1.3
import Berrygram 1.0
import "Ui.js" as Ui

Page {
    id: chatPage
    objectName: "chatPage"
    actionBarVisibility: ChromeVisibility.Hidden
    property bool liveChat: false
    property bool composerVisible: liveChat?_live.composerVisible:_chat.composerVisible
    property bool sceneReady: false
    property variant composerContext: liveChat?_live.composerContext:_chat.composerContext
    property string composerKey: ""
    property string beforeEditDraft: ""
    property int pendingNewCount: 0
    property int incomingSerial: liveChat?_live.incomingSerial:_chat.incomingSerial
    property int forwardSerial: liveChat?_live.forwardSerial:_chat.forwardSerial
    onCreationCompleted: sceneReady=true
    onComposerContextChanged: {
        if(!messageField)return;
        var mode=composerContext.mode || "";
        var key=mode+":"+(composerContext.messageId || "");
        if(key===composerKey)return;
        var editing=composerKey.indexOf("edit:")===0;
        composerKey=key;
        if(mode==="edit") { beforeEditDraft=messageField.text;messageField.text=composerContext.text || "";messageField.requestFocus(); }
        else if(editing) {messageField.text=liveChat?_live.draftText:beforeEditDraft;}
        else if(mode==="reply")messageField.requestFocus();
    }
    onIncomingSerialChanged: {
        if(!sceneReady)return;
        var source=liveChat?_live:_chat;
        if(navigation.top===chatPage && historyScroll.atEnd && pendingNewCount===0 && source.targetMessageId.length===0)messageList.scrollToPosition(ScrollPosition.End,ScrollAnimation.None);
        else pendingNewCount+=1;
    }
    onForwardSerialChanged: {
        var source=liveChat?_live:_chat;
        if(sceneReady && source.forwardSelection.state==="ready")navigation.openForward(liveChat);
    }
    property int scrollSerial: _live.scrollSerial
    onLiveChatChanged: {
        if (liveChat && messageField) {
            messageField.text = _live.draftText;
            if(composerVisible)messageField.requestFocus();
        }
    }
    shortcuts: [
        SystemShortcut {
            type: SystemShortcuts.JumpToBottom
            onTriggered: messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.Default)
        },
        SystemShortcut {
            type: SystemShortcuts.JumpToTop
            onTriggered: messageList.scrollToPosition(ScrollPosition.Beginning, ScrollAnimation.Default)
        },
        Shortcut {
            key: "r"
            enabled: chatPage.liveChat && !messageField.focused && _live.canLoadHistory
            onTriggered: _live.loadMoreHistory()
        }
    ]
    onScrollSerialChanged: {
        if (liveChat && messageList) scrollToTarget();
    }

    function submitDraft() {
        if (messageField.text.trim().length === 0 && (!liveChat || _live.attachmentName.length === 0)) return;
        if (liveChat && !_live.canSendSelected) return;
        if (!_ui.acceptSubmit(chatPage)) return;
        var editing=composerContext.mode==="edit";
        var accepted = liveChat ? _live.sendSelected(messageField.text) : _chat.sendDemoMessage(messageField.text);
        if (accepted && !editing) {
            messageField.text = "";
            messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.Default);
            messageField.requestFocus();
        }
    }
    function scrollToTarget() {
        var source = liveChat ? _live : _chat;
        if (source.targetMessageIndex >= 0) messageList.scrollToItem([source.targetMessageIndex], ScrollAnimation.None);
        else messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.None);
    }
    function focusComposer() {
        var source = liveChat ? _live : _chat;
        if (source.targetMessageId.length > 0) { scrollToTarget(); messageList.requestFocus(); }
        else {messageList.scrollToPosition(ScrollPosition.End,ScrollAnimation.None);if(composerVisible)messageField.requestFocus();else messageList.requestFocus();}
    }
    function showLatest() {
        if(liveChat && _live.targetMessageId.length>0)_live.latestMessages();
        else messageList.scrollToPosition(ScrollPosition.End,ScrollAnimation.None);
        pendingNewCount=0;
    }
    function nextUnread() {
        var id = _live.nextUnreadChat();
        if (id.length > 0 && _live.openChat(id)) {
            messageField.text = _live.draftText;
            messageField.requestFocus();
        }
    }

    titleBar: AppHeader {
        caption: chatPage.liveChat ? (_live.currentChatTitle || "Telegram") : _chat.currentChatTitle
        detail: chatPage.liveChat ? _live.currentChatStatus : _chat.currentChatStatus
        detailActive: chatPage.liveChat?_live.currentStatusActive:_chat.currentStatusActive
        avatarVisible: true
        avatarKey: chatPage.liveChat?_live.currentAvatarKey:_chat.currentAvatarKey
        compact: true
        menuVisible: true
        onMenuRequested: _ui.openMenu(chatPage)
    }
    actions: [
        ActionItem { objectName: "chatInfoAction"; title: qsTr("Сведения о чате"); imageSource: "asset:///icons/info-white.png"; onTriggered: navigation.openChatInfo(chatPage.liveChat) },
        ActionItem { title: qsTr("Документ"); imageSource: "asset:///icons/attach-white.png"; enabled: chatPage.liveChat && _live.canAttach && !_live.recording; onTriggered: _live.chooseAttachment("document") },
        ActionItem { title: qsTr("Фото"); imageSource: "asset:///icons/photo-white.png"; enabled: chatPage.liveChat && _live.canAttach && !_live.recording; onTriggered: _live.chooseAttachment("photo") },
        ActionItem { title: _live.recording ? qsTr("Остановить запись") : qsTr("Голосовое"); imageSource: "asset:///icons/voice-white.png"; enabled: chatPage.liveChat && _live.canAttach; onTriggered: _live.toggleRecording() },
        ActionItem {
            title: qsTr("Повторить историю")
            imageSource: "asset:///icons/history-white.png"
            enabled: chatPage.liveChat && _live.canLoadHistory && _live.errorText.length>0
            onTriggered: _live.loadMoreHistory()
        },
        ActionItem {
            title: qsTr("Вниз")
            imageSource: "asset:///icons/down-white.png"
            onTriggered: messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.Default)
        },
        ActionItem {
            title: qsTr("Непрочитанный")
            imageSource: "asset:///icons/unread-white.png"
            enabled: chatPage.liveChat && _live.ready
            onTriggered: chatPage.nextUnread()
            shortcuts: [ Shortcut { key: "Alt+u" } ]
        },
        ActionItem {
            title: qsTr("Отметить прочитанным")
            imageSource: "asset:///icons/read-white.png"
            enabled: chatPage.liveChat && _live.ready
            onTriggered: _live.markRead()
        },
        ActionItem { title: qsTr("Очистить копии"); imageSource: "asset:///icons/cleanup-white.png"; enabled: chatPage.liveChat && _live.ready; onTriggered: _live.cleanupUploads() }
    ]

    Container {
        id: chatContent
        objectName: "chatContent"
        property real measuredHeight: 0
        background: Color.create(Ui.canvas)
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        attachedObjects: [ LayoutUpdateHandler { onLayoutFrameChanged: chatContent.measuredHeight = layoutFrame.height } ]

        Label {
            visible: chatPage.liveChat && _live.loadingHistory
            text: qsTr("Загрузка истории…")
            textStyle.base: SystemDefaults.TextStyles.SmallText
            textStyle.color: Color.create(Ui.secondary)
            horizontalAlignment: HorizontalAlignment.Center
        }
        Button {
            visible: chatPage.liveChat && _live.targetMessageId.length > 0
            text: qsTr("К последним сообщениям")
            horizontalAlignment: HorizontalAlignment.Fill
            onClicked: _live.latestMessages()
        }
        Label {
            visible: chatPage.liveChat && _live.errorText.length > 0
            text: _live.errorText
            textFormat: TextFormat.Plain
            multiline: true
            leftMargin: ui.du(2)
            rightMargin: ui.du(2)
        }

        ListView {
            property variant avatarProvider: _avatars
            id: messageList
            objectName: "messageList"
            stickToEdgePolicy: ListViewStickToEdgePolicy.End
            property bool canReply: chatPage.composerVisible && (!chatPage.liveChat || _live.canSend)
            property bool editingContext: chatPage.composerContext.mode==="edit"
            property real measuredHeight: 0
            property real rowWidth: ui.du(72)
            attachedObjects: [
                LayoutUpdateHandler {
                    onLayoutFrameChanged: {messageList.rowWidth = layoutFrame.width;messageList.measuredHeight=layoutFrame.height;}
                },
                ListScrollStateHandler { id: historyScroll; objectName: "historyScroll"; onAtEndChanged: if(atEnd && navigation.top===chatPage)chatPage.pendingNewCount=0 },
                AutoPager {
                    objectName: "historyPager"
                    enabled: chatPage.liveChat && navigation.top === chatPage && _live.canLoadHistory && _live.errorText.length === 0
                    atEdge: historyScroll.atBeginning
                    contentCount: chatPage.liveChat ? _live.messageCount : 0
                    onRequestMore: _live.loadMoreHistory()
                }
            ]
            function downloadMedia(id) { _live.downloadMedia(id); }
            function openMedia(id) { _live.openMedia(id); }
            function copyText(text) { _ui.copyText(text); }
            function messageAction(id,action) { (chatPage.liveChat?_live:_chat).beginMessageAction(id,action); }
            dataModel: chatPage.liveChat ? _live.messages : _chat.messages
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }

            function itemType(data, indexPath) {
                return "message";
            }

            listItemComponents: [
                ListItemComponent {
                    type: "message"

                    Container {
                        id: messageItem
                        // A ListView delegate otherwise measures itself to its
                        // content, leaving no free space for right alignment.
                        preferredWidth: messageItem.ListItem.view.rowWidth
                        minWidth: preferredWidth
                        maxWidth: preferredWidth
                        horizontalAlignment: HorizontalAlignment.Fill
                        background: ListItemData.searchTarget === true ? Color.create("#fff0bb") : Color.Transparent
                        Label {
                            visible: ListItemData.searchTarget === true
                            text: qsTr("Найденное сообщение")
                            textStyle.base: SystemDefaults.TextStyles.SmallText
                            textStyle.color: Color.create(Ui.blue)
                            horizontalAlignment: HorizontalAlignment.Center
                        }
                        Label {
                            visible: ListItemData.startsDay === true
                            text: ListItemData.dateLabel || ""
                            textStyle.base: SystemDefaults.TextStyles.SmallText
                            textStyle.color: Color.create(Ui.secondary)
                            horizontalAlignment: HorizontalAlignment.Center
                            topMargin: ui.du(1.2); bottomMargin: ui.du(1)
                        }
                        Container {
                            visible: ListItemData.startsUnread === true
                            background: Color.create("#dce8ef")
                            horizontalAlignment: HorizontalAlignment.Fill
                            topPadding: ui.du(0.6); bottomPadding: ui.du(0.6)
                            topMargin: ui.du(0.5); bottomMargin: ui.du(0.7)
                            Label {
                                text: qsTr("Непрочитанные сообщения")
                                horizontalAlignment: HorizontalAlignment.Center
                                textStyle.base: SystemDefaults.TextStyles.SmallText
                                textStyle.color: Color.create(Ui.blue)
                            }
                        }
                        Container {
                            layout: DockLayout {}
                            horizontalAlignment: HorizontalAlignment.Fill
                            leftPadding: ListItemData.outgoing ? ui.du(9) : ui.du(1.2)
                            rightPadding: ListItemData.outgoing ? ui.du(1.2) : ui.du(9)
                            topPadding: ListItemData.joinedPrevious === true ? ui.du(0.15) : ui.du(0.65)
                            bottomPadding: ui.du(0.15)
                            Container {
                            background: ListItemData.outgoing ? outgoingPaint.imagePaint : incomingPaint.imagePaint
                            horizontalAlignment: ListItemData.outgoing ? HorizontalAlignment.Right : HorizontalAlignment.Left
                            maxWidth: Math.min(ui.du(58), messageItem.ListItem.view.rowWidth - ui.du(11))
                            leftPadding: ui.du(1.8); rightPadding: ui.du(1.8)
                            topPadding: ui.du(0.9); bottomPadding: ui.du(0.6)
                            contextActions: [
                                ActionSet {
                                    title: qsTr("Сообщение")
                            ActionItem { objectName: "replyAction-"+(ListItemData.messageId || ""); title: qsTr("Ответить"); imageSource: "asset:///icons/reply-white.png"; enabled: messageItem.ListItem.view.canReply && !messageItem.ListItem.view.editingContext && (ListItemData.messageId || "").length>0; onTriggered: messageItem.ListItem.view.messageAction(ListItemData.messageId,"reply") }
                            ActionItem { objectName: "forwardAction-"+(ListItemData.messageId || ""); title: qsTr("Переслать"); imageSource: "asset:///icons/forward-white.png"; enabled: !messageItem.ListItem.view.editingContext && (ListItemData.messageId || "").length>0; onTriggered: messageItem.ListItem.view.messageAction(ListItemData.messageId,"forward") }
                            ActionItem { objectName: "editAction-"+(ListItemData.messageId || ""); title: qsTr("Изменить"); imageSource: "asset:///icons/edit-white.png"; enabled: !messageItem.ListItem.view.editingContext && ListItemData.outgoing===true && ListItemData.textMessage===true; onTriggered: messageItem.ListItem.view.messageAction(ListItemData.messageId,"edit") }
                                    ActionItem {
                                        title: qsTr("Копировать текст")
                                        imageSource: "asset:///icons/copy-white.png"
                                        enabled: (ListItemData.text || "").length > 0
                                        onTriggered: messageItem.ListItem.view.copyText(ListItemData.text)
                                    }
                                }
                            ]
                            Label {
                                visible: ListItemData.forwarded===true
                                text: (ListItemData.forwardAuthor || "").length>0?qsTr("Переслано от ")+ListItemData.forwardAuthor:qsTr("Пересланное сообщение");textStyle.base: SystemDefaults.TextStyles.SmallText;textStyle.color: Color.create(Ui.blue)
                            }
                            Container {
                                visible: (ListItemData.replyQuote || "").length>0
                                background: Color.create("#e5edf2")
                                leftPadding: ui.du(1);rightPadding: ui.du(1);topPadding: ui.du(0.5);bottomPadding: ui.du(0.5)
                                Label { text: ListItemData.replyQuote || "";textFormat: TextFormat.Plain;textStyle.base: SystemDefaults.TextStyles.SmallText;textStyle.color: Color.create(Ui.blue);maxWidth: Math.min(ui.du(50),messageItem.ListItem.view.rowWidth-ui.du(19)) }
                            }
                            Label {
                                visible: ListItemData.showAuthor === true
                                text: ListItemData.author
                                textFormat: TextFormat.Plain
                                textStyle.base: SystemDefaults.TextStyles.SmallText
                                textStyle.fontWeight: FontWeight.W500
                                textStyle.color: Color.create(Ui.blue)
                                bottomMargin: ui.du(0.4)
                            }
                            EmojiLabel {
                                visible: (ListItemData.text || "").length > 0
                                text: ListItemData.text || ""
                                textWidth: Math.min(ui.du(54), messageItem.ListItem.view.rowWidth - ui.du(15))
                                topMargin: 0; bottomMargin: 0
                            }
                            ImageView {
                                visible: ListItemData.media !== undefined && ListItemData.media.kind === "photo" && ListItemData.media.localUrl.length > 0
                                imageSource: visible ? ListItemData.media.localUrl : ""
                                preferredWidth: ui.du(42); preferredHeight: ui.du(32)
                                scalingMethod: ScalingMethod.AspectFit
                                accessibility.name: qsTr("Фото из сообщения")
                            }
                            Button {
                                visible: ListItemData.media !== undefined && ListItemData.media.fileId !== undefined
                                enabled: visible && !ListItemData.media.downloading
                                text: visible && ListItemData.media.path.length > 0 ? qsTr("Открыть вложение") : qsTr("Скачать вложение")
                                onClicked: {
                                    if (ListItemData.media.path.length > 0) messageItem.ListItem.view.openMedia(ListItemData.messageId);
                                    else messageItem.ListItem.view.downloadMedia(ListItemData.messageId);
                                }
                            }
                            Label {
                                text: (ListItemData.edited === true ? qsTr("изменено · ") : "") + ListItemData.time
                                    + ((ListItemData.delivery || "").length > 0 ? " · " + ListItemData.delivery : "")
                                textFormat: TextFormat.Plain
                                multiline: true
                                textStyle.base: SystemDefaults.TextStyles.SmallText
                                textStyle.color: Color.create(ListItemData.outgoing ? "#698475" : Ui.secondary)
                                horizontalAlignment: HorizontalAlignment.Right
                                topMargin: ui.du(0.2); bottomMargin: 0
                            }
                            }
                        }
                        attachedObjects: [
                            ImagePaintDefinition { id: incomingPaint; imageSource: "asset:///ui/bubble-in.amd"; repeatPattern: RepeatPattern.Fill },
                            ImagePaintDefinition { id: outgoingPaint; imageSource: "asset:///ui/bubble-out.amd"; repeatPattern: RepeatPattern.Fill }
                        ]
                    }
                }
            ]
        }
        Label { visible: chatPage.liveChat && _live.recoveryWarning.length > 0; text: _live.recoveryWarning; multiline: true; textFormat: TextFormat.Plain; textStyle.base: SystemDefaults.TextStyles.SmallText }
        Label { visible: chatPage.liveChat && _live.mediaError.length > 0; text: _live.mediaError; multiline: true; textFormat: TextFormat.Plain }
        Label { visible: chatPage.liveChat && _live.recording; text: qsTr("Идёт запись · до 5 минут"); textStyle.base: SystemDefaults.TextStyles.SmallText }
        Container {
            visible: chatPage.liveChat && _live.attachmentName.length > 0
            leftPadding: ui.du(1); rightPadding: ui.du(1)
            Label { text: _live.attachmentName; textFormat: TextFormat.Plain; textStyle.base: SystemDefaults.TextStyles.SmallText }
            Container {
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                Button { text: qsTr("Предпросмотр"); enabled: !_live.recording; onClicked: _live.previewAttachment() }
                Button { text: qsTr("Убрать"); onClicked: _live.cancelAttachment() }
            }
        }
        Button {
            visible: chatPage.liveChat && _live.hasFailedAttachment && _live.attachmentName.length === 0
            text: qsTr("Восстановить вложение")
            onClicked: { _live.recoverAttachment(); messageField.text = _live.draftText; }
        }
        Button {
            visible: chatPage.liveChat && _live.failedDraft.length > 0
            enabled: messageField.text.length === 0
            text: _live.recoveryWarning.length > 0 ? qsTr("Вернуть текст после проверки истории") : qsTr("Вернуть неотправленный текст")
            horizontalAlignment: HorizontalAlignment.Fill
            onClicked: messageField.text = _live.takeFailedDraft()
        }

        Button {
            objectName: "newMessagesButton"
            visible: chatPage.pendingNewCount>0
            enabled: chatPage.composerContext.mode!=="edit"
            text: qsTr("Новые сообщения") + " · " + chatPage.pendingNewCount
            horizontalAlignment: HorizontalAlignment.Center
            onClicked: chatPage.showLatest()
        }
        Container {
            visible: chatPage.composerVisible && (chatPage.composerContext.mode || "").length>0
            background: Color.create("#e5edf2")
            leftPadding: ui.du(2);rightPadding: ui.du(1);topPadding: ui.du(0.5);bottomPadding: ui.du(0.5)
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            Container {
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                Label { text: chatPage.composerContext.mode==="edit"?qsTr("Редактирование"):qsTr("Ответ") + " · " + (chatPage.composerContext.author || "");textStyle.base: SystemDefaults.TextStyles.SmallText;textStyle.color: Color.create(Ui.blue) }
                Label { text: chatPage.composerContext.preview || "";textFormat: TextFormat.Plain;textStyle.base: SystemDefaults.TextStyles.SmallText;textStyle.color: Color.create(Ui.secondary) }
            }
            Button { objectName: "cancelComposerButton";text: "×";preferredWidth: ui.du(6);minWidth: ui.du(6);maxWidth: ui.du(6);accessibility.name: qsTr("Отменить ответ или редактирование");enabled: !chatPage.liveChat || !_live.sendBusy;onClicked: (chatPage.liveChat?_live:_chat).cancelComposer() }
        }

        Container {
            id: composer
            objectName: "composer"
            visible: chatPage.composerVisible
            property real measuredTop: 0
            property real measuredHeight: 0
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            background: Color.White
            leftPadding: ui.du(1.2)
            rightPadding: ui.du(0.7)
            topPadding: ui.du(0.7)
            bottomPadding: ui.du(0.7)
            attachedObjects: [
                LayoutUpdateHandler {
                    onLayoutFrameChanged: { composer.measuredTop = layoutFrame.y; composer.measuredHeight = layoutFrame.height; }
                }
            ]
            ImageButton {
                objectName: "attachmentButton"
                preferredWidth: ui.du(6.8); preferredHeight: ui.du(6.8)
                minWidth: preferredWidth; maxWidth: preferredWidth
                verticalAlignment: VerticalAlignment.Bottom
                defaultImageSource: "asset:///icons/attach-normal.png"
                pressedImageSource: "asset:///icons/attach-pressed.png"
                disabledImageSource: "asset:///icons/attach-disabled.png"
                enabled: chatPage.liveChat && _live.canAttach && !_live.recording
                opacity: enabled ? 1 : 0.45
                accessibility.name: qsTr("Прикрепить фото, документ или голосовое сообщение")
                onClicked: _ui.openMenu(chatPage)
            }

            TextArea {
                id: messageField
                objectName: "messageField"
                property real measuredHeight: 0
                hintText: chatPage.liveChat ? _live.composerHint : qsTr("Сообщение · демо")
                enabled: chatPage.composerVisible
                inputMode: TextAreaInputMode.Chat
                autoSize.maxLineCount: 4
                scrollMode: TextAreaScrollMode.Stiff
                verticalAlignment: VerticalAlignment.Bottom
                shortcuts: [
                    Shortcut { objectName: "sendShortcut"; key: "Enter"; onTriggered: chatPage.submitDraft() },
                    Shortcut { objectName: "newlineShortcut"; key: "Shift+Enter"; onTriggered: messageField.editor.insertPlainText("\n") }
                ]
                attachedObjects: [ LayoutUpdateHandler { onLayoutFrameChanged: messageField.measuredHeight = layoutFrame.height } ]
                onTextChanged: {
                    if (chatPage.liveChat) _live.setDraft(text);
                }
                layoutProperties: StackLayoutProperties {
                    spaceQuota: 1
                }
                input {
                    submitKey: SubmitKey.Send
                    submitKeyFocusBehavior: SubmitKeyFocusBehavior.Keep
                    onSubmitted: {
                        if (!chatPage.liveChat || _live.canSendSelected) chatPage.submitDraft();
                    }
                }
            }

            ImageButton {
                objectName: "sendButton"
                preferredWidth: ui.du(7.2); preferredHeight: ui.du(6.8)
                minWidth: preferredWidth; maxWidth: preferredWidth
                verticalAlignment: VerticalAlignment.Bottom
                defaultImageSource: "asset:///icons/send-normal.png"
                pressedImageSource: "asset:///icons/send-pressed.png"
                disabledImageSource: "asset:///icons/send-disabled.png"
                accessibility.name: chatPage.composerContext.mode==="edit"?qsTr("Сохранить изменения"):qsTr("Отправить сообщение")
                enabled: (messageField.text.trim().length > 0 || (chatPage.liveChat && _live.attachmentName.length > 0)) && (!chatPage.liveChat || _live.canSendSelected)
                opacity: enabled ? 1 : 0.45
                onClicked: chatPage.submitDraft()
            }
        }
    }
}
