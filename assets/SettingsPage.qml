import bb.cascades 1.3
import bb.system 1.2
import "Ui.js" as Ui
Page {
    id: page
    objectName: "settingsPage"
    property bool currentDark: _prefs.dark
    onCreationCompleted: if(_live.ready)_live.refreshStorage()
    actionBarVisibility: ChromeVisibility.Hidden
    titleBar: AppHeader {caption: qsTr("Настройки");compact: true}
    ScrollView {
        scrollViewProperties.scrollMode: ScrollMode.Vertical
        Container {
            background: Color.create(_prefs.surface)
            leftPadding: ui.du(2);rightPadding: ui.du(2)
            Label {visible: _prefs.error.length>0;text: _prefs.error;multiline: true;textStyle.color: Color.create("#b3261e")}
            Button {text: qsTr("Соединение и диагностика");onClicked: navigation.openDiagnostics()}
            Button {text: qsTr("Загрузки");onClicked: navigation.openDownloads()}
            Label {text: qsTr("Автозагрузка фото и голосовых до 5 МБ");multiline: true}
            CheckBox {text: qsTr("По Wi-Fi");checked: _prefs.autoDownloadWifi;onCheckedChanged: {_prefs.autoDownloadWifi=checked;_live.configureAutoDownloads(_prefs.autoDownloadWifi,_prefs.autoDownloadMobile);}}
            CheckBox {text: qsTr("По мобильной сети");checked: _prefs.autoDownloadMobile;onCheckedChanged: {_prefs.autoDownloadMobile=checked;_live.configureAutoDownloads(_prefs.autoDownloadWifi,_prefs.autoDownloadMobile);}}
            Label {text: qsTr("Автозагрузка работает в открытом чате. Документы скачиваются вручную.");multiline: true;textStyle.color: Color.create(_prefs.secondary)}
            CheckBox {objectName: "darkSetting";text: qsTr("Тёмная тема");checked: _prefs.dark;onCheckedChanged: _prefs.dark=checked}
            CheckBox {objectName: "previewSetting";text: qsTr("Скрывать текст уведомлений");checked: _prefs.hidePreview;onCheckedChanged: _prefs.hidePreview=checked}
            CheckBox {objectName: "enterSetting";text: qsTr("Отправлять по Enter");checked: _prefs.sendOnEnter;onCheckedChanged: _prefs.sendOnEnter=checked}
            Label {visible: !_prefs.sendOnEnter;text: qsTr("Отправка: Alt+Enter или кнопка");textStyle.color: Color.create(Ui.secondary)}
            Label {text: qsTr("Размер текста сообщений")}
            DropDown {
                objectName: "fontSetting"
                selectedIndex: _prefs.textSize
                Option {text: qsTr("Небольшой")}
                Option {text: qsTr("Обычный")}
                Option {text: qsTr("Крупный")}
                onSelectedIndexChanged: if(selectedIndex>=0)_prefs.textSize=selectedIndex
            }
            Label {text: qsTr("Кеш фотографий")}
            Label {text: _media.cacheDescription;multiline: true;textStyle.color: Color.create(_prefs.secondary)}
            DropDown {
                selectedIndex: _prefs.photoCacheMb===8?0:_prefs.photoCacheMb===64?2:1
                Option {text: qsTr("До 8 МБ")}
                Option {text: qsTr("До 24 МБ")}
                Option {text: qsTr("До 64 МБ")}
                onSelectedIndexChanged: {if(selectedIndex>=0){_prefs.photoCacheMb=selectedIndex===0?8:selectedIndex===2?64:24;_media.setCacheLimit(_prefs.photoCacheMb);}}
            }
            Button {text: qsTr("Очистить кеш фотографий");onClicked: _media.clearCache()}
            Label {visible: _live.ready;text: _live.storageSummary;multiline: true;textStyle.color: Color.create(Ui.secondary)}
            Button {visible: _live.ready;text: qsTr("Обновить сведения о файлах");onClicked: _live.refreshStorage()}
            Label {visible: _live.ready;text: _live.cacheSummary;multiline: true;textStyle.color: Color.create(_prefs.secondary)}
            Button {visible: _live.ready;text: qsTr("Очистить старые отправленные копии");onClicked: _live.cleanupUploads()}
            Label {
                visible: _telegram.phase === "ready" && _telegram.errorText.length > 0
                text: _telegram.errorText
                multiline: true
                textStyle.color: Color.create("#b3261e")
            }
            Button {
                objectName: "logOutButton"
                text: _telegram.busy ? qsTr("Выход из аккаунта…") : qsTr("Выйти из аккаунта")
                visible: _telegram.phase === "ready"
                enabled: !_telegram.busy
                onClicked: logOutDialog.show()
            }
        }
    }
    attachedObjects: [
        SystemDialog {
            id: logOutDialog
            title: qsTr("Выйти из аккаунта?")
            body: qsTr("Для повторного входа понадобится номер телефона и подтверждение от Telegram.")
            confirmButton.label: qsTr("Выйти")
            cancelButton.label: qsTr("Отмена")
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection) _telegram.logOut()
        }
    ]
}
