import bb.cascades 1.3

Page {
    id: loginPage
    objectName: "loginPage"
    property string phase: _telegram.phase

    titleBar: TitleBar { title: qsTr("Вход в Telegram") }

    // Remove one-time credentials from the view on every state transition.
    onPhaseChanged: {
        if (credential) credential.text = "";
        if (apiHash) apiHash.text = "";
    }

    ScrollView {
        scrollViewProperties.scrollMode: ScrollMode.Vertical
        Container {
            leftPadding: ui.du(2)
            rightPadding: ui.du(2)
            topPadding: ui.du(2)
            bottomPadding: ui.du(3)

            Label {
                text: _telegram.status
                textFormat: TextFormat.Plain
                multiline: true
            }
            Label {
                visible: _telegram.version.length > 0
                text: "TDLib " + _telegram.version
                textStyle.base: SystemDefaults.TextStyles.SmallText
            }
            Label {
                visible: _telegram.errorText.length > 0
                text: _telegram.errorText
                textFormat: TextFormat.Plain
                multiline: true
            }
            ActivityIndicator {
                visible: _telegram.busy
                running: _telegram.busy
                horizontalAlignment: HorizontalAlignment.Center
                preferredWidth: ui.du(6)
                preferredHeight: ui.du(6)
            }
            Button {
                visible: phase === "stopped" || phase === "failed"
                enabled: _telegram.available && !_telegram.busy
                text: qsTr("Запустить движок")
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: _telegram.start()
            }
            Container {
                visible: phase === "parameters"
                Label {
                    text: qsTr("Нужны ваши api_id и api_hash из API development tools на my.telegram.org. Они вводятся при каждом запуске движка.")
                    multiline: true
                }
                Label {
                    text: qsTr("Сессия сохраняется на этом устройстве. Остановка движка сохраняет вход в аккаунт.")
                    multiline: true
                    textStyle.base: SystemDefaults.TextStyles.SmallText
                }
                TextField {
                    id: apiId
                    hintText: "api_id"
                    enabled: !_telegram.busy
                    inputMode: TextFieldInputMode.NumbersAndPunctuation
                    input.flags: TextInputFlag.AutoCorrectionOff | TextInputFlag.PredictionOff | TextInputFlag.AutoCapitalizationOff
                }
                TextField {
                    id: apiHash
                    hintText: "api_hash"
                    enabled: !_telegram.busy
                    inputMode: TextFieldInputMode.Password
                    maximumLength: 32
                }
                Button {
                    text: qsTr("Подключиться")
                    enabled: !_telegram.busy && apiId.text.length > 0 && apiHash.text.length === 32
                    horizontalAlignment: HorizontalAlignment.Fill
                    onClicked: {
                        if (_telegram.configure(apiId.text, apiHash.text)) apiHash.text = "";
                    }
                }
            }
            Container {
                visible: phase === "phone" || phase === "code" || phase === "password"
                      || phase === "email" || phase === "emailCode"
                TextField {
                    id: credential
                    hintText: phase === "phone" ? "+79991234567"
                              : phase === "password" ? qsTr("Пароль")
                              : phase === "email" ? "name@example.com" : qsTr("Код подтверждения")
                    enabled: !_telegram.busy
                    maximumLength: 1024
                    inputMode: phase === "phone" ? TextFieldInputMode.PhoneNumber
                               : phase === "email" ? TextFieldInputMode.EmailAddress
                               : TextFieldInputMode.Password
                    input {
                        flags: TextInputFlag.AutoCorrectionOff | TextInputFlag.PredictionOff | TextInputFlag.AutoCapitalizationOff
                        submitKey: SubmitKey.Send
                        onSubmitted: {
                            if (!_telegram.busy && _telegram.submit(credential.text)) credential.text = "";
                        }
                    }
                }
                Button {
                    text: qsTr("Продолжить")
                    enabled: !_telegram.busy && credential.text.length > 0
                    horizontalAlignment: HorizontalAlignment.Fill
                    onClicked: {
                        if (_telegram.submit(credential.text)) credential.text = "";
                    }
                }
            }
            Label {
                visible: phase === "ready"
                text: qsTr("Список чатов и отправка сообщений появятся на следующем этапе. Чаты на главном экране остаются демонстрационными.")
                multiline: true
            }
            Button {
                visible: phase !== "stopped" && phase !== "failed"
                enabled: phase !== "closing"
                text: qsTr("Остановить движок")
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: {
                    credential.text = "";
                    apiHash.text = "";
                    _telegram.stop();
                }
            }
        }
    }
}
