import bb.cascades 1.3
Page {
    id: page
    objectName: "schedulePage"
    signal chosen(variant when)
    actionBarVisibility: ChromeVisibility.Hidden
    titleBar: AppHeader {caption: qsTr("Отложенная отправка");detail: qsTr("Местное время телефона");compact: true}
    Container {
        background: Color.create(_prefs.surface)
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        leftPadding: ui.du(2);rightPadding: ui.du(2);topPadding: ui.du(2)
        Label {text: qsTr("Выберите дату и время отправки");multiline: true;textStyle.color: Color.create(_prefs.ink)}
        DateTimePicker {id: sendTime;objectName: "schedulePicker";mode: DateTimePickerMode.DateTime;value: new Date(Date.now()+3600000)}
        Button {objectName: "confirmScheduleButton";text: qsTr("Запланировать");onClicked: page.chosen(sendTime.value)}
        Label {text: _live.errorText;multiline: true;textStyle.color: Color.create(_prefs.secondary)}
    }
}
