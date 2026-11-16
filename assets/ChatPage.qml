import bb.cascades 1.3

Page {
    id: chatPage
    objectName: "chatPage"
    property bool liveChat: false
    property int scrollSerial: _live.scrollSerial
    onLiveChatChanged: {
        if (liveChat && messageField) {
            messageField.text = _live.draftText;
            messageField.requestFocus();
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
        if (liveChat && messageList) messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.None);
    }

    function submitDraft() {
        var accepted = liveChat ? _live.sendSelected(messageField.text) : _chat.sendDemoMessage(messageField.text);
        if (accepted) {
            messageField.text = "";
            messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.Default);
            messageField.requestFocus();
        }
    }
    function nextUnread() {
        var id = _live.nextUnreadChat();
        if (id.length > 0 && _live.openChat(id)) {
            messageField.text = _live.draftText;
            messageField.requestFocus();
        }
    }

    titleBar: TitleBar {
        title: chatPage.liveChat ? (_live.currentChatTitle || "Telegram") : _chat.currentChatTitle
    }
    actions: [
        ActionItem { title: qsTr("Прикрепить файл"); enabled: chatPage.liveChat && _live.canAttach && !_live.recording; onTriggered: _live.chooseAttachment("document") },
        ActionItem { title: qsTr("Отправить фото"); enabled: chatPage.liveChat && _live.canAttach && !_live.recording; onTriggered: _live.chooseAttachment("photo") },
        ActionItem { title: _live.recording ? qsTr("Завершить запись") : qsTr("Записать голос"); enabled: chatPage.liveChat && _live.canAttach; onTriggered: _live.toggleRecording() },
        ActionItem {
            title: qsTr("Ранее")
            enabled: chatPage.liveChat && _live.canLoadHistory
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: _live.loadMoreHistory()
        },
        ActionItem {
            title: qsTr("Вниз")
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.Default)
        },
        ActionItem {
            title: qsTr("Следующий непрочитанный")
            enabled: chatPage.liveChat && _live.ready
            onTriggered: chatPage.nextUnread()
            shortcuts: [ Shortcut { key: "Alt+u" } ]
        },
        ActionItem {
            title: qsTr("Отметить прочитанным")
            enabled: chatPage.liveChat && _live.ready
            onTriggered: _live.markRead()
        }
    ]

    Container {
        horizontalAlignment: HorizontalAlignment.Fill

        Label {
            text: chatPage.liveChat ? (_live.loadingHistory ? qsTr("Загрузка истории…") : _live.connectionStatus)
                                    : qsTr("Демо · сообщения остаются в приложении")
            textStyle.base: SystemDefaults.TextStyles.SmallText
            horizontalAlignment: HorizontalAlignment.Center
            topMargin: ui.du(1)
            bottomMargin: ui.du(1)
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
            id: messageList
            objectName: "messageList"
            function downloadMedia(id) { _live.downloadMedia(id); }
            function openMedia(id) { _live.openMedia(id); }
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
                        leftPadding: ui.du(2)
                        rightPadding: ui.du(2)
                        topPadding: ui.du(1.5)
                        bottomPadding: ui.du(1.5)
                        horizontalAlignment: HorizontalAlignment.Fill

                        Label {
                            text: ListItemData.author + " · " + ListItemData.time
                            textFormat: TextFormat.Plain
                            textStyle.base: SystemDefaults.TextStyles.SmallText
                            horizontalAlignment: ListItemData.outgoing
                                ? HorizontalAlignment.Right : HorizontalAlignment.Left
                        }

                        Label {
                            text: ListItemData.text
                            textFormat: TextFormat.Plain
                            multiline: true
                            horizontalAlignment: ListItemData.outgoing
                                ? HorizontalAlignment.Right : HorizontalAlignment.Left
                        }
                        Label {
                            visible: ListItemData.delivery !== undefined && ListItemData.delivery.length > 0
                            text: ListItemData.delivery || ""
                            textFormat: TextFormat.Plain
                            textStyle.base: SystemDefaults.TextStyles.SmallText
                            horizontalAlignment: HorizontalAlignment.Right
                        }
                        ImageView {
                            visible: ListItemData.media !== undefined && ListItemData.media.kind === "photo" && ListItemData.media.localUrl.length > 0
                            imageSource: visible ? ListItemData.media.localUrl : ""
                            preferredWidth: ui.du(45)
                            preferredHeight: ui.du(35)
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
                            visible: ListItemData.edited === true
                            text: qsTr("Изменено")
                            textStyle.base: SystemDefaults.TextStyles.SmallText
                        }

                        Divider {}
                    }
                }
            ]
        }
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
            text: qsTr("Вернуть неотправленный текст")
            horizontalAlignment: HorizontalAlignment.Fill
            onClicked: messageField.text = _live.takeFailedDraft()
        }

        Container {
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            leftPadding: ui.du(1)
            rightPadding: ui.du(1)
            topPadding: ui.du(1)
            bottomPadding: ui.du(1)

            TextField {
                id: messageField
                objectName: "messageField"
                hintText: chatPage.liveChat ? _live.composerHint : qsTr("Демонстрационное сообщение")
                enabled: !chatPage.liveChat || _live.canSendSelected
                onTextChanged: {
                    if (chatPage.liveChat) _live.setDraft(text);
                }
                layoutProperties: StackLayoutProperties {
                    spaceQuota: 1
                }
                input {
                    submitKey: SubmitKey.Send
                    onSubmitted: {
                        if (!chatPage.liveChat || _live.canSendSelected) chatPage.submitDraft();
                    }
                }
            }

            Button {
                text: qsTr("Отправить")
                enabled: (messageField.text.trim().length > 0 || (chatPage.liveChat && _live.attachmentName.length > 0)) && (!chatPage.liveChat || _live.canSendSelected)
                onClicked: chatPage.submitDraft()
            }
        }
    }
}
