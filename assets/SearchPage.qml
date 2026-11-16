import bb.cascades 1.3
import Berrygram 1.0
import "Ui.js" as Ui

Page {
    id: searchPage
    objectName: "searchPage"
    actionBarVisibility: ChromeVisibility.Hidden
    signal openResult(string chatId, string messageId)
    property int mode: 0
    property bool demoSearch: true
    property int filter: 0
    function refresh() { _search.search(query.text, mode, filter); }
    function focusComposer() { query.requestFocus(); }
    function activateRow(row) {
        if (row.recentQuery !== undefined) { query.text = row.recentQuery; return; }
        _search.remember(); openResult(row.chatId, row.messageId || "");
    }
    titleBar: AppHeader {
        caption: qsTr("Поиск")
        detail: searchPage.demoSearch ? qsTr("Демо · результаты вымышлены") : qsTr("Во всех чатах Telegram")
        compact: true
    }
    shortcuts: [ Shortcut { key: "Escape"; onTriggered: { if (query.text.length > 0) query.text = ""; else navigation.pop(); } } ]
    Container {
        background: Color.create(_prefs.surface)
        Container {
            leftPadding: ui.du(1.5); rightPadding: ui.du(1.5); topPadding: ui.du(1); bottomPadding: ui.du(0.5)
            Container {
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                TextField {
                    id: query
                    objectName: "searchQuery"
                    hintText: searchPage.mode === 1 ? qsTr("Имя, @username или канал") : searchPage.mode === 2 ? qsTr("Текст сообщения") : qsTr("Название или @username")
                    inputMode: TextFieldInputMode.Text
                    layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                    onTextChanged: searchPage.refresh()
                    input { submitKey: SubmitKey.Search; onSubmitted: { _search.remember(); searchPage.refresh(); } }
                }
            }
            SegmentedControl {
                objectName: "searchTabs"
                horizontalAlignment: HorizontalAlignment.Fill
                Option { text: qsTr("Мои чаты"); value: 0; selected: true }
                Option { text: qsTr("Глобально"); value: 1 }
                Option { text: qsTr("Сообщения"); value: 2 }
                onSelectedValueChanged: { searchPage.mode = selectedValue; searchPage.refresh(); }
            }
            SegmentedControl {
                objectName: "searchFilters"
                visible: searchPage.mode === 2
                horizontalAlignment: HorizontalAlignment.Fill
                Option { text: qsTr("Все"); value: 0; selected: true }
                Option { text: qsTr("Фото"); value: 1 }
                Option { text: qsTr("Файлы"); value: 2 }
                Option { text: qsTr("Ссылки"); value: 3 }
                onSelectedValueChanged: { searchPage.filter = selectedValue; searchPage.refresh(); }
            }
        }
        Label {
            visible: query.text.trim().length === 0 && !(searchPage.mode === 2 && searchPage.filter > 0)
            text: _search.count > 0 ? qsTr("Недавние запросы") : searchPage.mode === 1 ? qsTr("Люди и боты по имени пользователя, публичные группы и каналы") : searchPage.mode === 2 ? qsTr("Ищите текст во всей переписке или выберите тип вложения") : qsTr("Поиск по личным чатам, группам и каналам")
            multiline: true; textFormat: TextFormat.Plain
            textStyle.base: SystemDefaults.TextStyles.SmallText; textStyle.color: Color.create(_prefs.secondary)
            leftMargin: ui.du(2); rightMargin: ui.du(2)
        }
        Label { visible: _search.loading; text: qsTr("Поиск…"); textStyle.color: Color.create(_prefs.secondary); leftMargin: ui.du(2) }
        Label { visible: _search.errorText.length > 0; text: _search.errorText; textFormat: TextFormat.Plain; multiline: true; textStyle.color: Color.create("#a74132"); leftMargin: ui.du(2); rightMargin: ui.du(2) }
        ListView {
            property variant avatarProvider: _avatars
            property variant preferences: _prefs
            id: resultList
            objectName: "searchResults"
            dataModel: _search.results
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            stickToEdgePolicy: ListViewStickToEdgePolicy.Beginning
            attachedObjects: [
                ListScrollStateHandler { id: resultScroll; objectName: "resultScroll" },
                AutoPager {
                    objectName: "resultPager"
                    enabled: _live.foreground && navigation.top === searchPage && _search.canLoadMore && _search.errorText.length === 0
                    atEdge: resultScroll.atEnd
                    contentCount: _search.count
                    onRequestMore: _search.loadMore()
                }
            ]
            function itemType(data,indexPath) { return "result"; }
            listItemComponents: [ ListItemComponent { type: "result"; ChatRow { row: ListItemData } } ]
            onTriggered: searchPage.activateRow(dataModel.data(indexPath))
        }
        Label {
            visible: !_search.loading && _search.count === 0 && _search.errorText.length === 0 && (query.text.trim().length > 0 || (searchPage.mode === 2 && searchPage.filter > 0))
            text: searchPage.demoSearch ? qsTr("В демоданных ничего не найдено") : qsTr("Ничего не найдено")
            textStyle.color: Color.create(_prefs.secondary); horizontalAlignment: HorizontalAlignment.Center
        }
        Button { visible: query.text.trim().length === 0 && _search.recentQueries.length > 0 && !(searchPage.mode === 2 && searchPage.filter > 0); text: qsTr("Очистить недавние запросы"); horizontalAlignment: HorizontalAlignment.Fill; onClicked: _search.clearRecent() }
        Button { visible: _search.errorText.length > 0 && !_search.loading; text: qsTr("Повторить поиск"); horizontalAlignment: HorizontalAlignment.Fill; onClicked: _search.retry() }
    }
}
