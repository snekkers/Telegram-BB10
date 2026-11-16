import bb.cascades 1.3

Page {
    id: chatPage

    function submitDraft() {
        if (_chat.sendDemoMessage(messageField.text)) {
            messageField.text = "";
            messageList.scrollToPosition(ScrollPosition.End, ScrollAnimation.Default);
            messageField.requestFocus();
        }
    }

    titleBar: TitleBar {
        title: _chat.currentChatTitle
    }

    Container {
        horizontalAlignment: HorizontalAlignment.Fill

        Label {
            text: qsTr("Демо · сообщения остаются в приложении")
            textStyle.base: SystemDefaults.TextStyles.SmallText
            horizontalAlignment: HorizontalAlignment.Center
            topMargin: ui.du(1)
            bottomMargin: ui.du(1)
        }

        ListView {
            id: messageList
            objectName: "messageList"
            dataModel: _chat.messages
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

                        Divider {}
                    }
                }
            ]
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
                hintText: qsTr("Демонстрационное сообщение")
                layoutProperties: StackLayoutProperties {
                    spaceQuota: 1
                }
                input {
                    submitKey: SubmitKey.Send
                    onSubmitted: chatPage.submitDraft()
                }
            }

            Button {
                text: qsTr("Отправить")
                enabled: messageField.text.trim().length > 0
                onClicked: chatPage.submitDraft()
            }
        }
    }
}
