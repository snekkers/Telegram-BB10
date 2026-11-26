import bb.cascades 1.3

Page {
    id: loginPage
    objectName: "loginPage"
    actionBarVisibility: ChromeVisibility.Hidden
    property string previewPhase: typeof _authPreview !== "undefined" ? _authPreview.objectName : ""
    property bool previewing: previewPhase.length > 0
    property string phase: previewing ? previewPhase : _telegram.phase
    property bool showApi: false
    property bool loginActive: false
    signal closeRequested()
    function heading() {
        if (phase === "loggingOut") return qsTr("Выход из аккаунта");
        if (phase === "password") return qsTr("Ваш пароль");
        if (phase === "code" || phase === "emailCode") return qsTr("Код подтверждения");
        if (phase === "email") return qsTr("Ваша почта");
        if (phase === "failed" || phase === "unsupported") return qsTr("Подключение к Telegram");
        return qsTr("Ваш номер телефона");
    }
    function explanation() {
        if (phase === "loggingOut" || phase === "starting") return _telegram.status;
        if (phase === "password") return qsTr("Включена двухэтапная проверка.\nВаш аккаунт защищён дополнительным паролем.");
        if (phase === "code") return qsTr("Введите код, полученный от Telegram.");
        if (phase === "emailCode") return qsTr("Введите код из письма Telegram.");
        if (phase === "email") return qsTr("Telegram запрашивает адрес электронной почты для подтверждения входа.");
        return qsTr("Выберите страну и введите\nсвой номер телефона.");
    }
    function submitCredential() {
        if (previewing || _telegram.busy) return;
        if (phase === "phone") {
            var raw = phoneDigits.text.trim();
            var number = raw.charAt(0) === "+" ? raw : countryCode.text + raw;
            if (_telegram.submit(number)) phoneDigits.text = "";
        } else if (_telegram.submit(credential.text)) credential.text = "";
    }
    function chooseConnection() {
        if (loginActive && !previewing && phase === "parameters" && !_telegram.busy && !_telegram.hasSavedConfiguration && _telegram.testConfigurationAvailable) _telegram.useTestConfiguration();
    }
    function beginLogin() {
        loginActive=true;
        if (phase === "stopped") _telegram.start();
        else chooseConnection();
    }
    onPhaseChanged: {
        chooseConnection();
        if (credential) credential.text = "";
        if (apiHash) apiHash.text = "";
        if (!previewing && phase === "phone" && phoneDigits) phoneDigits.requestFocus();
    }
    ScrollView {
        scrollViewProperties.scrollMode: ScrollMode.Vertical
        Container {
            background: Color.White
            horizontalAlignment: HorizontalAlignment.Fill
            leftPadding: ui.du(4); rightPadding: ui.du(4)
            topPadding: ui.du(1.5); bottomPadding: ui.du(3)
            Label {
                text: qsTr("‹ Назад")
                textStyle.color: Color.create("#0088ee")
                textStyle.base: SystemDefaults.TextStyles.SmallText
                bottomMargin: ui.du(1)
                gestureHandlers: [TapHandler {onTapped: if(!_telegram.busy || loginPage.previewing)loginPage.closeRequested()}]
            }
            ImageView {
                imageSource: phase === "password" ? "asset:///ui/login-password.png" : phase === "code" || phase === "emailCode" || phase === "email" ? "asset:///ui/login-code.png" : "asset:///ui/login-phone.png"
                scalingMethod: ScalingMethod.AspectFit
                preferredWidth: ui.du(11); preferredHeight: ui.du(11)
                horizontalAlignment: HorizontalAlignment.Center
                bottomMargin: ui.du(1.5)
            }
            Label {
                text: loginPage.heading()
                textStyle.fontSize: FontSize.PointValue
                textStyle.fontSizeValue: 11
                textStyle.fontWeight: FontWeight.W600
                textStyle.color: Color.Black
                textStyle.textAlign: TextAlign.Center
                horizontalAlignment: HorizontalAlignment.Fill
                multiline: true
                bottomMargin: ui.du(1)
            }
            Label {
                text: loginPage.explanation()
                textStyle.color: Color.create("#202020")
                textStyle.textAlign: TextAlign.Center
                horizontalAlignment: HorizontalAlignment.Fill
                multiline: true
                textFormat: TextFormat.Plain
                bottomMargin: ui.du(3)
            }
            Container {
                visible: phase === "phone" || phase === "parameters"
                horizontalAlignment: HorizontalAlignment.Fill
                DropDown {
                    objectName: "phoneCountry"
                    title: qsTr("Страна")
                    enabled: !_telegram.busy
                    Option {text: qsTr("Россия");value: "+7";selected: true}
                    Option {text: qsTr("Казахстан");value: "+7"}
                    Option {text: qsTr("Беларусь");value: "+375"}
                    Option {text: qsTr("Украина");value: "+380"}
                    Option {text: qsTr("Армения");value: "+374"}
                    Option {text: qsTr("Грузия");value: "+995"}
                    Option {text: qsTr("Другой код страны");value: "+"}
                    onSelectedValueChanged: if(countryCode)countryCode.text=selectedValue
                }
                Container {
                    layout: StackLayout {orientation: LayoutOrientation.LeftToRight}
                    topMargin: ui.du(1)
                    TextField {
                        id: countryCode
                        objectName: "phoneCountryCode"
                        text: "+7"
                        preferredWidth: ui.du(12);maxWidth: ui.du(12)
                        maximumLength: 5
                        enabled: !_telegram.busy
                        inputMode: TextFieldInputMode.PhoneNumber
                        input.flags: TextInputFlag.AutoCorrectionOff | TextInputFlag.PredictionOff | TextInputFlag.AutoCapitalizationOff
                        accessibility.name: qsTr("Код страны")
                    }
                    TextField {
                        id: phoneDigits
                        objectName: "phoneNumber"
                        hintText: "000 000 00 00"
                        layoutProperties: StackLayoutProperties {spaceQuota: 1}
                        maximumLength: 32
                        enabled: phase === "phone" && !_telegram.busy
                        inputMode: TextFieldInputMode.PhoneNumber
                        input {
                            flags: TextInputFlag.AutoCorrectionOff | TextInputFlag.PredictionOff | TextInputFlag.AutoCapitalizationOff
                            submitKey: SubmitKey.Next
                            onSubmitted: loginPage.submitCredential()
                        }
                        accessibility.name: qsTr("Номер телефона")
                    }
                }
            }
            TextField {
                id: credential
                objectName: "loginCredential"
                visible: phase === "code" || phase === "password" || phase === "email" || phase === "emailCode"
                hintText: phase === "password" ? qsTr("Пароль") : phase === "email" ? "name@example.com" : qsTr("Код подтверждения")
                enabled: !_telegram.busy
                maximumLength: 1024
                inputMode: phase === "password" ? TextFieldInputMode.Password : phase === "email" ? TextFieldInputMode.EmailAddress : TextFieldInputMode.NumbersAndPunctuation
                input {
                    flags: TextInputFlag.AutoCorrectionOff | TextInputFlag.PredictionOff | TextInputFlag.AutoCapitalizationOff
                    submitKey: SubmitKey.Next
                    onSubmitted: loginPage.submitCredential()
                }
            }
            Label {
                visible: !loginPage.previewing && _telegram.errorText.length > 0
                text: _telegram.errorText
                multiline: true
                textFormat: TextFormat.Plain
                textStyle.color: Color.create("#bc3e36")
            }
            ActivityIndicator {visible: !loginPage.previewing && _telegram.busy;running: visible;preferredWidth: ui.du(4);preferredHeight: ui.du(4);horizontalAlignment: HorizontalAlignment.Center}
            Button {
                objectName: "loginContinue"
                visible: phase === "phone" || credential.visible
                text: qsTr("Продолжить")
                appearance: ControlAppearance.Primary
                preferredHeight: ui.du(7.5)
                topMargin: ui.du(3)
                horizontalAlignment: HorizontalAlignment.Fill
                enabled: !loginPage.previewing && !_telegram.busy && (phase === "phone" ? countryCode.text.length>1 && phoneDigits.text.replace(/[^0-9]/g, "").length>=5 : credential.visible && credential.text.length>0)
                onClicked: loginPage.submitCredential()
            }
            Button {
                visible: !loginPage.previewing && (phase === "failed" || phase === "stopped")
                text: qsTr("Подключиться снова")
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: _telegram.start()
            }
            Label {
                visible: !loginPage.previewing && phase === "parameters" && (!_telegram.testConfigurationAvailable || !_telegram.busy)
                text: loginPage.showApi ? qsTr("Скрыть настройки") : qsTr("Настройки подключения")
                textStyle.color: Color.create("#0088ee")
                textStyle.base: SystemDefaults.TextStyles.SmallText
                horizontalAlignment: HorizontalAlignment.Center
                topMargin: ui.du(2)
                gestureHandlers: [TapHandler {onTapped: loginPage.showApi=!loginPage.showApi}]
            }
            Container {
                visible: !loginPage.previewing && phase === "parameters" && (loginPage.showApi || !_telegram.testConfigurationAvailable)
                Label {text: qsTr("Параметры вашего приложения с my.telegram.org/apps. Они сохранятся на этом устройстве.");multiline: true;textStyle.base: SystemDefaults.TextStyles.SmallText;textStyle.color: Color.create("#75838e")}
                TextField {id: apiId;hintText: "api_id";inputMode: TextFieldInputMode.NumbersAndPunctuation;enabled: !_telegram.busy}
                TextField {id: apiHash;hintText: "api_hash";inputMode: TextFieldInputMode.Password;maximumLength: 32;enabled: !_telegram.busy}
                Button {text: qsTr("Подключиться");enabled: !_telegram.busy && apiId.text.length>0 && apiHash.text.length===32;onClicked: if(_telegram.configureAndRemember(apiId.text,apiHash.text))apiHash.text=""}
            }
        }
    }
}
