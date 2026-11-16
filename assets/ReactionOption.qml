import bb.cascades 1.3
import "Ui.js" as Ui
Container {
    id: optionView
    property variant option
    signal selected(string emoji)
    visible: option!==undefined && option!==null && (option.emoji || "").length>0
    preferredWidth: ui.du(10);preferredHeight: ui.du(8)
    background: option && option.chosen?Color.create("#c4dfef"):Color.Transparent
    layout: DockLayout {}
    EmojiLabel {text: optionView.option?optionView.option.emoji:"";textWidth: ui.du(7);pixelSize: ui.du(4);horizontalAlignment: HorizontalAlignment.Center;verticalAlignment: VerticalAlignment.Center}
    Label {visible: optionView.option && optionView.option.premium;text: "★";textStyle.color: Color.create(Ui.blue);horizontalAlignment: HorizontalAlignment.Right}
    gestureHandlers: [TapHandler {onTapped: if(optionView.option && !optionView.option.premium)optionView.selected(optionView.option.emoji)}]
}
