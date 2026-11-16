import bb.cascades 1.3
Page {
    id: page
    objectName: "scheduledEditorPage"
    actionBarVisibility: ChromeVisibility.Hidden
    property variant row: ({})
    property bool confirming: false
    property string localError: ""
    property string chatId: _live.currentChatId
    onRowChanged: {body.text=row.text || "";if(row.sendDate>0)sendTime.value=new Date(row.sendDate*1000);}
    titleBar: AppHeader {caption: qsTr("Отложенное сообщение");compact: true}
    ScrollView {
        scrollViewProperties.scrollMode: ScrollMode.Vertical
        Container {
            background: Color.create(_prefs.surface)
            leftPadding: ui.du(2);rightPadding: ui.du(2);topPadding: ui.du(2)
            Label {text: page.row.sendTime || "";textStyle.color: Color.create(_prefs.secondary)}
            TextArea {id: body;enabled: page.row.textMessage===true && !_live.scheduledBusy;autoSize.maxLineCount: 6;hintText: qsTr("Текст сообщения")}
            Button {text: qsTr("Сохранить текст");visible: page.row.textMessage===true;enabled: !_live.scheduledBusy;onClicked: if(!_live.saveScheduledText(page.row.messageId,body.text))page.localError=qsTr("Не удалось начать изменение. Обновите список.")}
            DateTimePicker {id: sendTime;mode: DateTimePickerMode.DateTime;value: new Date(Date.now()+3600000)}
            Button {text: qsTr("Изменить время");enabled: !_live.scheduledBusy;onClicked: if(!_live.reschedule(page.row.messageId,sendTime.value))page.localError=qsTr("Проверьте время и обновите список.")}
            Button {text: qsTr("Отменить отправку");enabled: !_live.scheduledBusy;onClicked: page.confirming=true}
            Container {
                visible: page.confirming
                Label {text: qsTr("Удалить сообщение из отложенных?");multiline: true}
                Button {text: qsTr("Да, отменить отправку");enabled: !_live.scheduledBusy;onClicked: {if(_live.cancelScheduled(page.row.messageId))page.confirming=false;else page.localError=qsTr("Обновите список и повторите.");}}
                Button {text: qsTr("Оставить сообщение");onClicked: page.confirming=false}
            }
            Label {text: page.localError;visible: text.length>0;multiline: true;textStyle.color: Color.create("#b3261e")}
            Label {text: _live.scheduledStatus;multiline: true;textStyle.color: Color.create(_prefs.secondary)}
        }
    }
}
