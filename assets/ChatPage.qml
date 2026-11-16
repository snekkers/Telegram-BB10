import bb.cascades 1.3

Page {
    id: chatPage
    objectName: "chatPage"
    property bool liveChat: false
    property int scrollSerial: _live.scrollSerial
    onScrollSerialChanged: {
        if (liveChat && messageList) messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.None);
    }

    function submitDraft() {
        var accepted = liveChat ? _live.sendText(messageField.text) : _chat.sendDemoMessage(messageField.text);
        if (accepted) {
            messageField.text = "";
            messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.Default);
            messageField.requestFocus();
        }
    }

    titleBar: TitleBar {
        title: chatPage.liveChat ? (_live.currentChatTitle || "Telegram") : _chat.currentChatTitle
    }
    actions: [
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
                enabled: !chatPage.liveChat || _live.canSend
                layoutProperties: StackLayoutProperties {
                    spaceQuota: 1
                }
                input {
                    submitKey: SubmitKey.Send
                    onSubmitted: {
                        if (!chatPage.liveChat || _live.canSend) chatPage.submitDraft();
                    }
                }
            }

            Button {
                text: qsTr("Отправить")
                enabled: messageField.text.trim().length > 0 && (!chatPage.liveChat || _live.canSend)
                onClicked: chatPage.submitDraft()
            }
        }
    }
}
