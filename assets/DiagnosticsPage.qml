import bb.cascades 1.3
Page {
    objectName: "diagnosticsPage"
    actionBarVisibility: ChromeVisibility.Hidden
    onCreationCompleted: {_live.refreshDiagnostics();_telegram.refreshDiagnostics();}
    titleBar: AppHeader {caption: qsTr("Соединение и диагностика");compact: true}
    ScrollView {
        scrollViewProperties.scrollMode: ScrollMode.Vertical
        Container {
            background: Color.create(_prefs.surface)
            leftPadding: ui.du(2);rightPadding: ui.du(2);topPadding: ui.du(2)
            Label {text: _live.connectionStatus;multiline: true;textStyle.color: Color.create(_prefs.ink)}
            Label {text: _telegram.errorText;visible: text.length>0;multiline: true;textStyle.color: Color.create("#b3261e")}
            Button {text: qsTr("Восстановить соединение");enabled: !_telegram.busy;onClicked: {_live.flushDrafts();_telegram.reconnect();}}
            Label {text: qsTr("Сессия входа сохраняется. Отправки без подтверждения не повторяются автоматически.");multiline: true;textStyle.color: Color.create(_prefs.secondary)}
            Button {text: qsTr("Обновить показатели");onClicked: {_live.refreshDiagnostics();_telegram.refreshDiagnostics();}}
            Label {text: _live.diagnostics;multiline: true;textFormat: TextFormat.Plain;textStyle.color: Color.create(_prefs.ink)}
            Label {text: _telegram.diagnostics;multiline: true;textStyle.color: Color.create(_prefs.secondary)}
            Label {text: qsTr("Показатели относятся к текущему запуску. Содержимое сообщений, номера и пароли в отчёт не входят.");multiline: true;textStyle.color: Color.create(_prefs.secondary)}
        }
    }
}
