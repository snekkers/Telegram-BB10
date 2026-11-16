import bb.cascades 1.3
import "Ui.js" as Ui
Page {
    id: page
    objectName: "photoPage"
    property string photoPosition: _media.position
    property bool photoLoaded: _media.photoSource.length>0
    onPhotoPositionChanged: if(photoScroll)photoScroll.resetViewableArea(ScrollAnimation.None)
    actionBarVisibility: ChromeVisibility.Hidden
    titleBar: AppHeader {caption: qsTr("Фотография");detail: _media.position;compact: true}
    shortcuts: [Shortcut {key: "p";onTriggered: _media.movePhoto(-1)},Shortcut {key: "n";onTriggered: _media.movePhoto(1)}]
    Container {
        background: Color.Black
        ActivityIndicator {visible: _media.loading;running: visible;horizontalAlignment: HorizontalAlignment.Center}
        ScrollView {
            id: photoScroll
            objectName: "photoScroll"
            layoutProperties: StackLayoutProperties {spaceQuota: 1}
            property real viewportWidth: 720
            property real viewportHeight: 560
            attachedObjects: [LayoutUpdateHandler {onLayoutFrameChanged: {photoScroll.viewportWidth=layoutFrame.width;photoScroll.viewportHeight=layoutFrame.height;}}]
            scrollViewProperties {
                scrollMode: ScrollMode.Both
                pinchToZoomEnabled: true
                minContentScale: 1
                maxContentScale: 4
            }
            ImageView {
                objectName: "fullPhoto"
                imageSource: _media.photoSource
                preferredWidth: photoScroll.viewportWidth
                preferredHeight: photoScroll.viewportHeight
                scalingMethod: ScalingMethod.AspectFit
            }
        }
        Label {text: _media.caption;textFormat: TextFormat.Plain;textStyle.color: Color.White;multiline: true;maxHeight: ui.du(10);leftMargin: ui.du(2);rightMargin: ui.du(2)}
        Label {visible: _media.status.length>0;text: _media.status;textStyle.color: Color.White;multiline: true;leftMargin: ui.du(2)}
        Container {
            layout: StackLayout {orientation: LayoutOrientation.LeftToRight}
            horizontalAlignment: HorizontalAlignment.Center
            Button {objectName: "previousPhotoButton";text: "‹";enabled: _media.canPrevious;onClicked: _media.movePhoto(-1)}
            Button {objectName: "savePhotoButton";text: qsTr("Сохранить");enabled: _media.canSave;onClicked: _media.savePhoto()}
            Button {objectName: "nextPhotoButton";text: "›";enabled: _media.canNext;onClicked: _media.movePhoto(1)}
        }
    }
}
