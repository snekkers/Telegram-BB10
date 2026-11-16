import bb.cascades 1.3
import Berrygram 1.0
import "Ui.js" as Ui
Page {
    id: page
    objectName: "emojiPage"
    signal picked(string symbol)
    actionBarVisibility: ChromeVisibility.Hidden
    onCreationCompleted: _emoji.setCategory(1)
    titleBar: AppHeader { caption: qsTr("Emoji");detail: qsTr("Выберите для вставки");compact: true }
    Container {
        background: Color.create(_prefs.surface)
        DropDown {
            objectName: "emojiCategory"
            title: qsTr("Категория")
            Option {text: qsTr("Недавние");value: 0}
            Option {text: qsTr("Смайлики");value: 1;selected: true}
            Option {text: qsTr("Жесты и сердца");value: 2}
            Option {text: qsTr("Природа");value: 3}
            Option {text: qsTr("Еда");value: 4}
            Option {text: qsTr("Предметы и символы");value: 5}
            Option {text: qsTr("Флаги");value: 6}
            onSelectedValueChanged: if(selectedValue!==undefined)_emoji.setCategory(selectedValue)
        }
        ListView {
            objectName: "emojiList"
            dataModel: _emoji.entries
            layoutProperties: StackLayoutProperties {spaceQuota: 1}
            layout: GridListLayout {columnCount: 6;cellAspectRatio: 1}
            listItemComponents: [ListItemComponent {type: "";Container {
                layout: DockLayout {}
                EmojiLabel {text: ListItemData.symbol;pixelSize: ui.du(5);textWidth: ui.du(8);horizontalAlignment: HorizontalAlignment.Center;verticalAlignment: VerticalAlignment.Center}
            }}]
            onTriggered: {var symbol=dataModel.data(indexPath).symbol;_emoji.remember(symbol);page.picked(symbol);navigation.pop();}
        }
        Container {
            layout: StackLayout {orientation: LayoutOrientation.LeftToRight}
            horizontalAlignment: HorizontalAlignment.Center
            Button {text: "‹";preferredWidth: ui.du(12);minWidth: preferredWidth;maxWidth: preferredWidth;enabled: _emoji.canPrevious;onClicked: _emoji.movePage(-1)}
            Label {text: _emoji.position;minWidth: ui.du(9);textStyle.textAlign: TextAlign.Center;verticalAlignment: VerticalAlignment.Center}
            Button {text: "›";preferredWidth: ui.du(12);minWidth: preferredWidth;maxWidth: preferredWidth;enabled: _emoji.canNext;onClicked: _emoji.movePage(1)}
        }
    }
}
