import QtQuick                      2.7
import QtQuick.Controls             1.2

Item {
    id: pingItem
    z: 1
    // Default state
    state: "top-left"
    signal  activated()

    property bool inPopup: false
    property bool smartVisibility: false
    property bool iconVisible: (pingBtMouseArea.containsMouse || pingItemMouseArea.containsMouse || !smartVisibility) && !inPopup
    property var item: null
    property var hideItem: true
    property var hoverParent: undefined
    property var startAngle: 0
    property var icon: undefined
    property var clicked: false
    property var colorUnselected: Qt.rgba(0,0,0,0.5)
    property var colorSelected: Qt.rgba(0,0,0,0.75)
    property var color: hideItem ? colorUnselected : colorSelected
    property var spin: false

    // default, overridable
    width: 36
    height: 36

    onItemChanged: {
        if(item == null) {
            return
        }
        item.parent = itemRect
        item.enabled = false
        item.visible = false
        item.z = 1
        item.opacity = itemRect.opacity
        item.anchors.horizontalCenter = itemRect.horizontalCenter
        item.anchors.verticalCenter = itemRect.verticalCenter
    }

    onClickedChanged: {
        openIcon.flip = clicked
    }

    MouseArea {
        id: pingItemMouseArea
        anchors.fill: parent
        enabled: smartVisibility
        hoverEnabled: true
        onClicked: {
            if(item != null) {
                hideItem = true
            }
        }
    }

    onHideItemChanged: {
        itemRect.hide = hideItem
    }

    Rectangle {
        id: iconRect
        anchors.left: parent.left
        anchors.top: parent.top

        height: parent.height
        width: parent.width

        visible: iconVisible

        color: pingItem.color

        Image {
            id: openIcon
            source:         icon == undefined ? "/icons/arrow_right_white.svg" : icon
            fillMode:       Image.PreserveAspectFit
            mipmap:         true
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            visible: parent.visible
            property var flip: pingItem.clicked
            property var finalAngle: pingItem.spin ? 360 : 180

            height: parent.height
            width: parent.width

            RotationAnimator on rotation {
                id: rotateIcon
                from: startAngle
                to: startAngle
                duration: pingItem.spin ? 1000 : 200
                running: true
                onRunningChanged: {
                    if(pingItem.spin && pingItem.clicked) {
                        openIcon.flip = !openIcon.flip
                    }
                }
            }

            onFlipChanged: {
                rotateIcon.from = rotateIcon.to
                rotateIcon.to = flip ? finalAngle + startAngle : 0 + startAngle
                rotateIcon.running = true
            }
        }

        MouseArea {
            id: pingBtMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                if(item != null) {
                    itemRect.hide = !itemRect.hide
                    hideItem = itemRect.hide
                }

                pingItem.clicked = !pingItem.clicked
            }
        }
    }

    states: [
        State {
            name: "top-left"
            AnchorChanges {
                target: itemRect
                anchors.top: iconRect.top
                anchors.left: iconRect.right
            }
        },
        State {
            name: "bottom-left"
            AnchorChanges {
                target: itemRect
                anchors.bottom: iconRect.bottom
                anchors.left: iconRect.right
            }
        }
    ]

    Rectangle {
        id: itemRect
        opacity: 0

        height: item != null ? item.height*1.05 : 0
        width: item != null ? item.width*1.05 : 0

        color: pingItem.color
        property var hide: true

        onOpacityChanged: {
            item.opacity = itemRect.opacity
            item.visible = itemRect.opacity != 0
        }

        OpacityAnimator {
            id: rectOpa
            target: itemRect
            from: 0
            to: 0
            duration: 200
            running: true
        }

        OpacityAnimator {
            id: itemOpa
            target: item
            from: 0
            to: 0
            duration: 200
            running: true
        }

        onHideChanged: {
            item.enabled = !hide
            rectOpa.from = rectOpa.to
            rectOpa.to = hide ? 0 : 1
            rectOpa.running = true

            itemOpa.from = itemOpa.to
            itemOpa.to = hide ? 0 : 1
            itemOpa.running = true
        }
    }
}
