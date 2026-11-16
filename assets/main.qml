import bb.cascades 1.3

NavigationPane {
    id: navigation

    Page {
        titleBar: TitleBar {
            title: "Berrygram"
        }

        Container {
            horizontalAlignment: HorizontalAlignment.Fill

            Label {
                text: qsTr("Демонстрационные чаты")
                textStyle.base: SystemDefaults.TextStyles.SmallText
                horizontalAlignment: HorizontalAlignment.Center
                topMargin: ui.du(2)
                bottomMargin: ui.du(1)
            }

            ListView {
                id: chatList
                objectName: "chatList"
                dataModel: _chat.chats
                layoutProperties: StackLayoutProperties {
                    spaceQuota: 1
                }

                function itemType(data, indexPath) {
                    return "chat";
                }

                listItemComponents: [
                    ListItemComponent {
                        type: "chat"
                        StandardListItem {
                            title: ListItemData.title
                            description: ListItemData.preview
                        }
                    }
                ]

                onTriggered: {
                    var row = dataModel.data(indexPath);
                    if (_chat.openChat(row.chatId)) {
                        var chatPage = chatPageDefinition.createObject();
                        if (chatPage) {
                            navigation.push(chatPage);
                        } else {
                            _chat.closeChat();
                        }
                    }
                }
            }
        }
    }

    attachedObjects: [
        ComponentDefinition {
            id: chatPageDefinition
            source: "ChatPage.qml"
        }
    ]

    onPopTransitionEnded: {
        page.destroy();
        _chat.closeChat();
    }
}
