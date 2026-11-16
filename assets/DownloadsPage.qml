import bb.cascades 1.3
Page {
    id: page
    objectName: "downloadsPage"
    actionBarVisibility: ChromeVisibility.Hidden
    property variant selected: ({})
    titleBar: AppHeader {caption: qsTr("Загрузки");detail: qsTr("Последние 64 файла");compact: true}
    Container {
        background: Color.create(_prefs.surface)
        ListView {
            dataModel: _live.downloads
            layoutProperties: StackLayoutProperties {spaceQuota: 1}
            function itemType(data,indexPath){return "download";}
            listItemComponents: [ListItemComponent {type: "download"
                StandardListItem {
                    title: ListItemData.name || qsTr("Файл")
                    description: ListItemData.status
                    status: Math.round((ListItemData.downloaded || 0)/1024)+" / "+Math.round((ListItemData.size || 0)/1024)+" КБ"
                }
            }]
            onTriggered: page.selected=dataModel.data(indexPath)
        }
        Label {text: page.selected.name || qsTr("Выберите файл для отмены или повторной загрузки");multiline: true;leftMargin: ui.du(2);rightMargin: ui.du(2)}
        Button {text: qsTr("Отменить загрузку");enabled: page.selected.fileId!==undefined;onClicked: {_live.cancelDownload(page.selected.fileId);page.selected=({});}}
        Button {text: qsTr("Повторить загрузку");enabled: page.selected.fileId!==undefined;onClicked: {_live.retryDownload(page.selected.fileId);page.selected=({});}}
        Label {text: _live.errorText;multiline: true;textStyle.color: Color.create(_prefs.secondary)}
    }
}
