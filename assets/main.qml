import bb.cascades 1.3

NavigationPane {
    id: navigation
    property bool live: _live.ready

    Page {
        titleBar: TitleBar {
            title: "Berrygram"
        }
        actions: [
            ActionItem {
                title: _live.ready ? qsTr("Сессия Telegram") : qsTr("Вход в Telegram")
                ActionBar.placement: ActionBarPlacement.OnBar
                onTriggered: {
                    var loginPage = loginPageDefinition.createObject();
                    if (loginPage) navigation.push(loginPage);
                }
            }
        ]

        Container {
            horizontalAlignment: HorizontalAlignment.Fill

            Label {
                text: navigation.live ? _live.connectionStatus : qsTr("Демонстрационные чаты")
                textStyle.base: SystemDefaults.TextStyles.SmallText
                horizontalAlignment: HorizontalAlignment.Center
                topMargin: ui.du(2)
                bottomMargin: ui.du(1)
            }

            Label {
                visible: navigation.live && _live.errorText.length > 0
                text: _live.errorText
                textFormat: TextFormat.Plain
                multiline: true
                leftMargin: ui.du(2)
                rightMargin: ui.du(2)
            }

            ListView {
                id: chatList
                objectName: "chatList"
                dataModel: navigation.live ? _live.chats : _chat.chats
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
                            status: ListItemData.unreadLabel || ""
                        }
                    }
                ]

                onTriggered: {
                    var row = dataModel.data(indexPath);
                    var source = navigation.live ? _live : _chat;
                    if (source.openChat(row.chatId)) {
                        var chatPage = chatPageDefinition.createObject();
                        if (chatPage) {
                            chatPage.liveChat = navigation.live;
                            navigation.push(chatPage);
                        } else {
                            source.closeChat();
                        }
                    }
                }
            }
            Label {
                visible: navigation.live && !_live.loadingChats && _live.chatCount === 0
                text: qsTr("Чаты ещё не загружены. Нажмите «Загрузить ещё чаты».")
                textFormat: TextFormat.Plain
                multiline: true
                leftMargin: ui.du(2)
                rightMargin: ui.du(2)
            }
            Button {
                visible: navigation.live
                text: _live.loadingChats ? qsTr("Загрузка…") : qsTr("Загрузить ещё чаты")
                enabled: _live.canLoadChats
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: _live.loadMoreChats()
            }
        }
    }

    attachedObjects: [
        ComponentDefinition {
            id: chatPageDefinition
            source: "ChatPage.qml"
        },
        ComponentDefinition {
            id: loginPageDefinition
            source: "LoginPage.qml"
        }
    ]

    onPopTransitionEnded: {
        if (page.objectName === "chatPage") {
            if (page.liveChat) _live.closeChat();
            else _chat.closeChat();
        }
        page.destroy();
    }
}
