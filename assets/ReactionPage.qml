import bb.cascades 1.3
import "Ui.js" as Ui
Page {
    id: page
    objectName: "reactionPage"
    actionBarVisibility: ChromeVisibility.Hidden
    property variant state: _live.reactionState
    property variant options: state.options || []
    titleBar: AppHeader {caption: qsTr("Реакции");detail: qsTr("Выберите реакцию");compact: true}
    ScrollView {
        scrollViewProperties.scrollMode: ScrollMode.Vertical
        Container {
            background: Color.create(_prefs.surface)
            leftPadding: ui.du(2);rightPadding: ui.du(2);topPadding: ui.du(1)
            Label {visible: page.state.state==="loading";text: qsTr("Загрузка реакций…");textStyle.color: Color.create(_prefs.secondary)}
            Label {text: page.state.status || "";multiline: true;textStyle.color: Color.create(_prefs.secondary)}
            Container {layout: StackLayout {orientation: LayoutOrientation.LeftToRight}
                horizontalAlignment: HorizontalAlignment.Center
                ReactionOption {option: page.options[0];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[1];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[2];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[3];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[4];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[5];onSelected: _live.chooseReaction(emoji)}
            }
            Container {layout: StackLayout {orientation: LayoutOrientation.LeftToRight}
                horizontalAlignment: HorizontalAlignment.Center
                ReactionOption {option: page.options[6];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[7];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[8];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[9];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[10];onSelected: _live.chooseReaction(emoji)}
                ReactionOption {option: page.options[11];onSelected: _live.chooseReaction(emoji)}
            }
            Label {visible: page.state.state==="sending";text: qsTr("Отправка…");textStyle.color: Color.create(_prefs.secondary)}
            Button {objectName: "reactionPeopleButton";text: qsTr("Кто поставил реакции");enabled: !page.state.peopleLoading;onClicked: _live.loadReactors(false)}
            Label {text: page.state.peopleError || "";multiline: true;textStyle.color: Color.create(_prefs.secondary)}
            EmojiLabel {text: page.state.names || "";ink: _prefs.ink;textWidth: ui.du(76)}
            Button {text: qsTr("Следующие");visible: (page.state.nextOffset || "").length>0;enabled: !page.state.peopleLoading;onClicked: _live.loadReactors(true)}
        }
    }
}
