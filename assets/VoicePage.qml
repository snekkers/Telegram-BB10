import bb.cascades 1.3
Page {
    id: page
    objectName: "voicePage"
    actionBarVisibility: ChromeVisibility.Hidden
    property bool voicePlaying: _voice.playing
    property bool voiceLoading: _voice.loading
    property real voicePosition: _voice.position
    property real voiceDuration: _voice.duration
    property real voiceSpeed: _voice.speed
    property string voiceStatus: _voice.status
    function checkAction(action,value){if(action==="seek")_voice.seek(value);else if(action==="speed")_voice.setSpeed(value);else if(action==="toggle")_voice.toggle();}
    titleBar: AppHeader {caption: qsTr("Голосовое сообщение");compact: true}
    function time(ms){var seconds=Math.floor(ms/1000);return Math.floor(seconds/60)+":"+(seconds%60<10?"0":"")+(seconds%60);}
    Container {
        background: Color.create(_prefs.surface)
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        leftPadding: ui.du(2);rightPadding: ui.du(2);topPadding: ui.du(4)
        Label {text: _voice.loading?qsTr("Подготовка звука…"):page.time(_voice.position)+" / "+page.time(_voice.duration);horizontalAlignment: HorizontalAlignment.Center;textStyle.color: Color.create(_prefs.ink)}
        Slider {
            id: progress
            fromValue: 0
            toValue: Math.max(1,_voice.duration)
            value: _voice.position
            enabled: _voice.available
            onValueChanged: if(enabled && Math.abs(value-_voice.position)>500)_voice.seek(value)
        }
        Button {text: _voice.playing?qsTr("Пауза"):qsTr("Воспроизвести");enabled: _voice.available;horizontalAlignment: HorizontalAlignment.Fill;onClicked: _voice.toggle()}
        Container {
            layout: StackLayout {orientation: LayoutOrientation.LeftToRight}
            Button {text: "−15 с";enabled: _voice.available;onClicked: _voice.seek(_voice.position-15000);layoutProperties: StackLayoutProperties {spaceQuota: 1}}
            Button {text: "+15 с";enabled: _voice.available;onClicked: _voice.seek(_voice.position+15000);layoutProperties: StackLayoutProperties {spaceQuota: 1}}
        }
        Container {
            layout: StackLayout {orientation: LayoutOrientation.LeftToRight}
            Button {text: _voice.speed===1?"✓ 1×":"1×";enabled: _voice.available;onClicked: _voice.setSpeed(1);layoutProperties: StackLayoutProperties {spaceQuota: 1}}
            Button {text: _voice.speed===1.5?"✓ 1,5×":"1,5×";enabled: _voice.available;onClicked: _voice.setSpeed(1.5);layoutProperties: StackLayoutProperties {spaceQuota: 1}}
            Button {text: _voice.speed===2?"✓ 2×":"2×";enabled: _voice.available;onClicked: _voice.setSpeed(2);layoutProperties: StackLayoutProperties {spaceQuota: 1}}
        }
        Label {text: _voice.status;multiline: true;textStyle.color: Color.create(_prefs.secondary)}
    }
}
