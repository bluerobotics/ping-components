import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import SettingsManager 1.0

Item {
    id: root

    // these are actually all in meters right now
    property var start_mm: 0
    // The depth at the top edge of the item
    property var end_mm: 0
    // The depth at the bottom edge of the item
    property var length_mm: end_mm - start_mm
    property var depth_mm: 0 // The range to target (bottom) currently reported by the ping
    property var validIncrements: [0.1, 0.2, 0.5, 1, 2, 5, 10]
    property int numTicks: 10
    property int maxTicks: 10
    property var increment: 1 // increment to use between tick marks
    property var color: "white"
    property real scalar: SettingsManager.distanceUnits["distanceScalar"]

    function getIncrement() {
        var inc;
        for (var i = 0; i < validIncrements.length; i++) {
            inc = validIncrements[i];
            if (length_mm / inc <= maxTicks)
                break;

        }
        return inc;
    }

    // Update increment size and number of ticks to display
    function recalc() {
        increment = getIncrement();
        numTicks = length_mm / increment + 1;
    }

    // The pixel distance between ticks at regular intervals
    function nominalHeight() {
        return increment * parent.height / length_mm;
    }

    // The remaining height between the penultimate and last tick mark at the bottom of the item
    function remainingHeight() {
        return parent.height - ((numTicks - 1) * nominalHeight());
    }

    onStart_mmChanged: {
        recalc();
    }
    onEnd_mmChanged: {
        recalc();
    }

    Component {
        id: tickMark

        RowLayout {
            property var lastIndex: (index + 1) == numTicks

            anchors.right: parent.right
            height: lastIndex ? remainingHeight() : nominalHeight()
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom

            Rectangle {
                id: tick

                width: 15
                height: 4
                Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                color: root.color
                border.color: "black"
                border.width: 0.5
                visible: parent.height > 0
            }

        }

    }

    Component {
        id: extendedTickMark
        RowLayout {
            anchors.right: parent.right
            height: lastIndex ? remainingHeight() : nominalHeight()
            width: root.width

            Layout.fillHeight: true
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom

            property var lastIndex: (index + 1) == numTicks
                Rectangle {
                    id: extendedTick
                    width: root.width
                    height: 1
                    Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                    color: root.color
                    border.color: root.color
                    border.width: 1
                    visible: parent.height > 0
                }
        }
    }

    Component {
        id: tickLabel

        RowLayout {
            property var lastIndex: (index + 1) == numTicks

            anchors.right: parent.right
            height: lastIndex ? remainingHeight() : nominalHeight()
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom

            Text {
                id: label

                color: root.color
                visible: parent.height >= height
                font.bold: true
                font.pointSize: 22
                Layout.alignment: Qt.AlignRight | Qt.AlignBottom // Use layout alignement instead (no anchors)
                Layout.bottomMargin: 5
                Layout.rightMargin: 7
                style: Text.Outline
                styleColor: "black"
                text: lastIndex ? (scalar * end_mm).toFixed(1) : increment < 1 ? (scalar * (start_mm + (index + 1) * increment)).toFixed(1) : (scalar * (start_mm + (index + 1) * increment)).toFixed(0)
            }

        }

    }

    // Use two columns, one to show tick marks, one to show labels
    // The positioning/layout logic is the same for both columnns
    // I could not correctly combine the tickmarks + labels into a
    // single visual item (in order to use only one column here)
    Column {
        anchors.fill: parent

        Repeater {
            model: numTicks
            delegate: tickMark
        }

    }

    Column {
        anchors.fill: parent

        Repeater {
            model: numTicks
            delegate: extendedTickMark
        }
    }

    Column {
        anchors.fill:parent
        Repeater {
            model: numTicks
            delegate: tickLabel
        }

    }

    // A triangle indicator to show the current depth reported by the ping
    Canvas {
        id: depthPointer

        y: (parent.height / length_mm) * (depth_mm - start_mm) - height / 2
        width: 20
        height: 12
        anchors.right: parent.right
        onWidthChanged: requestPaint()
        visible: depth_mm ? depth_mm > 0 : false
        onPaint: {
            var ctx = getContext("2d");
            ctx.fillStyle = "red";
            ctx.strokeStyle = "black";
            ctx.beginPath();
            ctx.moveTo(0, 0);
            ctx.lineTo(width, height / 2);
            ctx.lineTo(0, height);
            ctx.lineTo(0, 0);
            ctx.stroke();
            ctx.fill();
        }
    }

}
