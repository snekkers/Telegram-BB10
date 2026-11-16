import bb.cascades 1.3
import "Ui.js" as Ui

Page {
    id: loginPage
    objectName: "loginPage"
    property string phase: _telegram.phase
    property bool showApi: false
    property bool showDetails: false
    signal closeRequested()
    titleBar: AppHeader { caption: qsTr("Вход в Telegram"); detail: qsTr("Berrygram для BlackBerry") }

    function heading() {
        if (phase === "phone") return qsTr("Ваш номер телефона");
        if (phase === "code" || phase === "emailCode") return qsTr("Код подтверждения");
        if (phase === "password") return qsTr("Облачный пароль");
        if (phase === "email") return qsTr("Адрес электронной почты");
        if (phase === "ready") return qsTr("Вы вошли в Telegram");
        if (phase === "parameters") return qsTr("Подключение к Telegram");
        return qsTr("Добро пожаловать");
    }
    function submitCredential() {
        if (!_telegram.busy && _telegram.submit(credential.text)) credential.text = "";
    }
    onCreationCompleted: {
        if (phase === "stopped") _telegram.start();
    }
    onPhaseChanged: {
        if (credential) credential.text = "";
        if (apiHash) apiHash.text = "";
    }
    actions: [
        ActionItem {
            title: qsTr("Отключиться")
            enabled: phase !== "stopped" && phase !== "closing"
            onTriggered: { credential.text = ""; apiHash.text = ""; _live.flushDrafts(); _telegram.stop(); }
        }
    ]
    ScrollView {
        scrollViewProperties.scrollMode: ScrollMode.Vertical
        Container {
            background: Color.create(_prefs.surface)
            leftPadding: ui.du(3.2)
            rightPadding: ui.du(3.2)
            topPadding: ui.du(4)
            bottomPadding: ui.du(3)
            Label {
                text: loginPage.heading()
                textStyle.base: SystemDefaults.TextStyles.BigText
                textStyle.color: Color.create(_prefs.ink)
                multiline: true
            }
            Label {
                text: phase === "phone" ? qsTr("Введите номер с кодом страны, например +7. Код придёт способом, который выберет Telegram.")
                    : phase === "password" ? qsTr("Введите пароль двухэтапной проверки вашего аккаунта.")
                    : phase === "parameters" ? qsTr("Выберите подключение для этой сборки. Затем можно войти по номеру телефона.")
                    : phase === "ready" ? qsTr("Теперь можно открыть ваши чаты.") : _telegram.status
                textFormat: TextFormat.Plain
                textStyle.color: Color.create(_prefs.secondary)
                multiline: true
                bottomMargin: ui.du(2)
            }
            Container {
                visible: _telegram.errorText.length > 0
                background: Color.create("#fff0ec")
                leftPadding: ui.du(1.5); rightPadding: ui.du(1.5)
                topPadding: ui.du(1); bottomPadding: ui.du(1)
                Label { text: _telegram.errorText; textFormat: TextFormat.Plain; multiline: true; textStyle.color: Color.create("#a74132") }
            }
            ActivityIndicator { visible: _telegram.busy; running: visible; horizontalAlignment: HorizontalAlignment.Center; preferredWidth: ui.du(5); preferredHeight: ui.du(5) }
            Button {
                visible: phase === "stopped" || phase === "failed"
                enabled: _telegram.available && !_telegram.busy
                text: qsTr("Продолжить")
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: _telegram.start()
            }
            Container {
                visible: phase === "parameters"
                Button {
                    visible: _telegram.hasSavedConfiguration
                    text: qsTr("Продолжить подключение")
                    enabled: !_telegram.busy
                    horizontalAlignment: HorizontalAlignment.Fill
                    onClicked: _telegram.useSavedConfiguration()
                }
                Container {
                    visible: _telegram.testConfigurationAvailable && !_telegram.hasSavedConfiguration
                    Button {
                        text: qsTr("Тестовое подключение")
                        enabled: !_telegram.busy
                        horizontalAlignment: HorizontalAlignment.Fill
                        onClicked: _telegram.useTestConfiguration()
                    }
                    Label {
                        text: qsTr("Официальные тестовые ключи Telegram. Подходят только для разработки; сервер может ограничить вход. Для обычного использования нужны ключи Berrygram.")
                        textStyle.base: SystemDefaults.TextStyles.SmallText
                        textStyle.color: Color.create(_prefs.secondary)
                        multiline: true
                    }
                }
                Button {
                    text: loginPage.showApi ? qsTr("Скрыть настройки подключения") : qsTr("Свои параметры API")
                    enabled: !_telegram.busy
                    horizontalAlignment: HorizontalAlignment.Fill
                    onClicked: loginPage.showApi = !loginPage.showApi
                }
                Container {
                    visible: loginPage.showApi
                    Label { text: qsTr("Параметры приложения с my.telegram.org/apps. Это разовая настройка этой сборки."); multiline: true; textStyle.base: SystemDefaults.TextStyles.SmallText; textStyle.color: Color.create(_prefs.secondary) }
                    TextField {
                        id: apiId
                        hintText: "api_id"
                        enabled: !_telegram.busy
                        inputMode: TextFieldInputMode.NumbersAndPunctuation
                        input.flags: TextInputFlag.AutoCorrectionOff | TextInputFlag.PredictionOff | TextInputFlag.AutoCapitalizationOff
                    }
                    TextField { id: apiHash; hintText: "api_hash"; enabled: !_telegram.busy; inputMode: TextFieldInputMode.Password; maximumLength: 32 }
                    CheckBox { id: rememberApi; text: qsTr("Запомнить на этом устройстве"); checked: true; enabled: !_telegram.busy }
                    Button {
                        text: qsTr("Подключиться")
                        enabled: !_telegram.busy && apiId.text.length > 0 && apiHash.text.length === 32
                        horizontalAlignment: HorizontalAlignment.Fill
                        onClicked: {
                            var accepted = rememberApi.checked ? _telegram.configureAndRemember(apiId.text, apiHash.text) : _telegram.configure(apiId.text, apiHash.text);
                            if (accepted) apiHash.text = "";
                        }
                    }
                    Button { visible: _telegram.hasSavedConfiguration; text: qsTr("Удалить сохранённые параметры"); enabled: !_telegram.busy; onClicked: _telegram.forgetConfiguration() }
                }
            }
            Container {
                visible: phase === "phone" || phase === "code" || phase === "password" || phase === "email" || phase === "emailCode"
                TextField {
                    id: credential
                    hintText: phase === "phone" ? "+7 999 123 45 67" : phase === "password" ? qsTr("Пароль") : phase === "email" ? "name@example.com" : qsTr("Код")
                    enabled: !_telegram.busy
                    maximumLength: 1024
                    inputMode: phase === "phone" ? TextFieldInputMode.PhoneNumber : phase === "email" ? TextFieldInputMode.EmailAddress : TextFieldInputMode.Password
                    input {
                        flags: TextInputFlag.AutoCorrectionOff | TextInputFlag.PredictionOff | TextInputFlag.AutoCapitalizationOff
                        submitKey: SubmitKey.Next
                        onSubmitted: loginPage.submitCredential()
                    }
                }
                Button { text: qsTr("Продолжить"); enabled: !_telegram.busy && credential.text.trim().length > 0; horizontalAlignment: HorizontalAlignment.Fill; onClicked: loginPage.submitCredential() }
            }
            Button { visible: phase === "ready"; text: qsTr("Открыть чаты"); horizontalAlignment: HorizontalAlignment.Fill; onClicked: loginPage.closeRequested() }
            Button { text: loginPage.showDetails ? qsTr("Скрыть сведения") : qsTr("Сведения о подключении"); horizontalAlignment: HorizontalAlignment.Fill; onClicked: loginPage.showDetails = !loginPage.showDetails }
            Container {
                visible: loginPage.showDetails
                Label { text: "Berrygram 0.11.0" + (_telegram.version.length > 0 ? " · TDLib " + _telegram.version : ""); textStyle.base: SystemDefaults.TextStyles.SmallText; textStyle.color: Color.create(_prefs.secondary); multiline: true }
                Label { text: qsTr("Сессия хранится на устройстве. Отключение сохраняет вход в аккаунт."); textStyle.base: SystemDefaults.TextStyles.SmallText; textStyle.color: Color.create(_prefs.secondary); multiline: true }
            }
        }
    }
}
