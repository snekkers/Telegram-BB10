import bb.cascades 1.3
Page {
    id: page
    objectName: "mutePage"
    property string status: ""
    actionBarVisibility: ChromeVisibility.Hidden
    titleBar: AppHeader {caption: qsTr("Отключить уведомления");compact: true}
    function morning(){var date=new Date();date.setHours(8,0,0,0);if(date.getTime()<=Date.now())date.setDate(date.getDate()+1);return date;}
    ScrollView {
        scrollViewProperties.scrollMode: ScrollMode.Vertical
        Container {
            background: Color.create(_prefs.surface)
            leftPadding: ui.du(2);rightPadding: ui.du(2);topPadding: ui.du(2)
            Button {text: qsTr("На 1 час");onClicked: page.status=_live.muteFor(3600)?qsTr("Запрос отправлен"):qsTr("Не удалось отключить уведомления")}
            Button {text: qsTr("До 8:00 утра");onClicked: page.status=_live.muteUntil(page.morning())?qsTr("Запрос отправлен"):qsTr("Не удалось отключить уведомления")}
            DateTimePicker {id: until;mode: DateTimePickerMode.DateTime;value: new Date(Date.now()+3600000)}
            Button {text: qsTr("До выбранного времени");onClicked: page.status=_live.muteUntil(until.value)?qsTr("Запрос отправлен"):qsTr("Выберите будущее время в пределах года")}
            Button {text: qsTr("Включить уведомления");onClicked: page.status=_live.muteFor(0)?qsTr("Запрос отправлен"):qsTr("Не удалось включить уведомления")}
            Label {text: page.status;multiline: true;textStyle.color: Color.create(_prefs.secondary)}
            Label {text: _live.errorText;multiline: true;textStyle.color: Color.create("#b3261e")}
        }
    }
}
