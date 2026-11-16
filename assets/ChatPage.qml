import bb.cascades 1.3
import "Ui.js" as Ui

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

    titleBar: AppHeader {
        caption: chatPage.liveChat ? (_live.currentChatTitle || "Telegram") : _chat.currentChatTitle
        detail: chatPage.liveChat ? _live.connectionStatus : qsTr("Демо · сообщения остаются на устройстве")
    }
    actions: [
        ActionItem { title: qsTr("Очистить старые копии"); enabled: chatPage.liveChat && _live.ready; onTriggered: _live.cleanupUploads() },
        ActionItem { ActionBar.placement: ActionBarPlacement.OnBar; title: qsTr("Прикрепить"); enabled: chatPage.liveChat && _live.canAttach && !_live.recording; onTriggered: _live.chooseAttachment("document") },
        ActionItem { title: qsTr("Отправить фото"); enabled: chatPage.liveChat && _live.canAttach && !_live.recording; onTriggered: _live.chooseAttachment("photo") },
        ActionItem { title: _live.recording ? qsTr("Завершить запись") : qsTr("Записать голос"); enabled: chatPage.liveChat && _live.canAttach; onTriggered: _live.toggleRecording() },
        ActionItem {
            title: qsTr("Ранее")
            enabled: chatPage.liveChat && _live.canLoadHistory
            onTriggered: _live.loadMoreHistory()
        },
        ActionItem {
            title: qsTr("Вниз")
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
        background: Color.create(Ui.canvas)
        horizontalAlignment: HorizontalAlignment.Fill

        Label {
            visible: chatPage.liveChat && _live.loadingHistory
            text: qsTr("Загрузка истории…")
            textStyle.base: SystemDefaults.TextStyles.SmallText
            textStyle.color: Color.create(Ui.secondary)
            horizontalAlignment: HorizontalAlignment.Center
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
            property real rowWidth: ui.du(72)
            attachedObjects: [
                LayoutUpdateHandler {
                    onLayoutFrameChanged: messageList.rowWidth = layoutFrame.width
                }
            ]
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
                        // A ListView delegate otherwise measures itself to its
                        // content, leaving no free space for right alignment.
                        preferredWidth: messageItem.ListItem.view.rowWidth
                        minWidth: preferredWidth
                        maxWidth: preferredWidth
                        layout: DockLayout {}
                        leftPadding: ListItemData.outgoing ? ui.du(9) : ui.du(1.4)
                        rightPadding: ListItemData.outgoing ? ui.du(1.4) : ui.du(9)
                        topPadding: ui.du(0.5)
                        bottomPadding: ui.du(0.5)
                        horizontalAlignment: HorizontalAlignment.Fill
                        Container {
                            background: ListItemData.outgoing ? outgoingPaint.imagePaint : incomingPaint.imagePaint
                            horizontalAlignment: ListItemData.outgoing ? HorizontalAlignment.Right : HorizontalAlignment.Left
                            maxWidth: Math.min(ui.du(56), messageItem.ListItem.view.rowWidth - ui.du(12))
                            leftPadding: ui.du(1.8); rightPadding: ui.du(1.8)
                            topPadding: ui.du(1.2); bottomPadding: ui.du(1)
                            Label {
                                visible: !ListItemData.outgoing
                                text: ListItemData.author
                                textFormat: TextFormat.Plain
                                textStyle.base: SystemDefaults.TextStyles.SmallText
                                textStyle.fontWeight: FontWeight.W500
                                textStyle.color: Color.create(Ui.blue)
                                bottomMargin: ui.du(0.4)
                            }
                            Label {
                                visible: (ListItemData.text || "").length > 0
                                text: ListItemData.text
                                textFormat: TextFormat.Plain
                                multiline: true
                                textStyle.color: Color.create(Ui.ink)
                                maxWidth: ui.du(51)
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
                                textStyle.color: Color.create("#698475")
                                horizontalAlignment: HorizontalAlignment.Right
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

        Container {
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            background: Color.White
            leftPadding: ui.du(1.2)
            rightPadding: ui.du(1.2)
            topPadding: ui.du(1)
            bottomPadding: ui.du(1)

            TextField {
                id: messageField
                objectName: "messageField"
                hintText: chatPage.liveChat ? _live.composerHint : qsTr("Сообщение · демо")
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
                preferredWidth: ui.du(20)
                maxWidth: ui.du(20)
                text: qsTr("Отправить")
                enabled: (messageField.text.trim().length > 0 || (chatPage.liveChat && _live.attachmentName.length > 0)) && (!chatPage.liveChat || _live.canSendSelected)
                onClicked: chatPage.submitDraft()
            }
        }
    }
}
