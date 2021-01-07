import DeviceManager 1.0
import FileManager 1.0
import GradientScale 1.0
import Qt.labs.settings 1.0
import QtGraphicalEffects 1.0
import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Controls 1.4 as QC1
import QtQuick.Layouts 1.3
import SettingsManager 1.0
import StyleManager 1.0
import Util 1.0
import WaterfallPlot 1.0

Item {
    id: root

    property alias displaySettings: displaySettings
    property var ping: DeviceManager.primarySensor

    function draw(points, confidence, initialPoint, length, distance) {
        waterfall.draw(points, confidence, initialPoint, length, distance);
        chart.draw(points, length + initialPoint, initialPoint);
    }

    function setDepth(depth) {
        depthAxis.depth_mm = depth;
        readout.value = depth;
    }

    function setConfidence(perc) {
        readout.confidence = perc;
    }

    function confidenceToColor(confidence) {
        return Qt.rgba(2 * (1 - confidence / 100), 2 * confidence / 100, 0);
    }

    function transformValue(value, precision) {
        return typeof (value) == "number" ? value.toFixed(precision) : value + " ";
    }

    function captureVisualizer() {
        waterfall.grabToImage(function(result) {
            print("Grab waterfall image callback.");
            print(FileManager.createFileName(FileManager.Pictures));
            result.saveToFile(FileManager.createFileName(FileManager.Pictures));
        });
    }

    function clear() {
        waterfall.clear();
    }

    function handleShortcut(key) {
        return false;
    }

    anchors.fill: parent
    onWidthChanged: {
        if (chart.Layout.minimumWidth === chart.width)
            waterfall.width = width - chart.width;

    }

    Connections {
        target: ping
        onPointsChanged: {
            // Move from mm to m
            root.draw(ping.points, ping.confidence, ping.start_mm * 0.001, ping.length_mm * 0.001, ping.distance * 0.001);
        }
        onDistanceChanged: {
            root.setDepth(ping.distance / 1000);
        }
        onConfidenceChanged: {
            root.setConfidence(ping.confidence);
        }
    }

    QC1.SplitView {
        orientation: Qt.Horizontal
        anchors.fill: parent

        WaterfallPlot {
            id: waterfall

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredWidth: 350
            Layout.minimumWidth: 350

            Rectangle {
                x: waterfall.mousePos.x - width / 2 + height / 2
                y: width / 2
                height: 15
                width: waterfall.height
                visible: waterfall.containsMouse

                ColumnLayout {
                    x: waterfall.mousePos.y - width / 2
                    y: -height * 2
                    rotation: -90

                    Text {
                        id: mouseReadout

                        text: (waterfall.mouseColumnDepth * SettingsManager.distanceUnits["distanceScalar"]).toFixed(2) + SettingsManager.distanceUnits["distance"]
                        color: confidenceToColor(waterfall.mouseColumnConfidence)
                        font.family: "Arial"
                        font.pointSize: 15
                        font.bold: true

                        Text {
                            id: mouseConfidenceText

                            x: mouseReadout.width - width
                            y: mouseReadout.height * 4 / 5
                            text: transformValue(waterfall.mouseColumnConfidence) + "%"
                            visible: typeof (waterfall.mouseColumnConfidence) == "number"
                            color: confidenceToColor(waterfall.mouseColumnConfidence)
                            font.family: "Arial"
                            font.pointSize: 10
                            font.bold: true
                        }

                    }

                }

                transform: Rotation {
                    origin.x: height / 2
                    angle: 90
                }

                gradient: Gradient {
                    GradientStop {
                        position: 0.3
                        color: "transparent"
                    }
                    // Not working with material

                    GradientStop {
                        position: 0.5
                        color: StyleManager.secondaryColor
                    }

                    GradientStop {
                        position: 0.8
                        color: "transparent"
                    }

                }

            }

            DepthAxis {
                id: depthAxis

                anchors.fill: parent
                width: waterfall.width
                start_mm: waterfall.minDepthToDraw
                end_mm: waterfall.maxDepthToDraw
                visible: start_mm != end_mm
            }

            GradientScale {
                width: 100
                height: 10
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                waterfallGradient: waterfall.waterfallGradient
            }

        }

        Chart {
            id: chart

            Layout.fillHeight: true
            Layout.maximumWidth: 250
            Layout.preferredWidth: 100
            Layout.minimumWidth: 75
            // TODO these should be properties of the Ping1DVisualizer
            maxDepthToDraw: waterfall.maxDepthToDraw
            minDepthToDraw: waterfall.minDepthToDraw
        }

        Settings {
            property alias chartWidth: chart.width
        }

    }

    ValueReadout {
        id: readout
    }

    Component {
        id: displaySettings

        GridLayout {
            anchors.fill: parent
            columns: 5
            rowSpacing: 5
            columnSpacing: 5

            CheckBox {
                id: smoothDataChB

                text: "Smooth Data"
                checked: true
                Layout.columnSpan: 5
                Layout.fillWidth: true
                onCheckStateChanged: {
                    waterfall.smooth = checkState;
                }
            }

            CheckBox {
                id: antialiasingDataChB

                property bool isMac: Util.isMac()

                text: "Antialiasing"
                checked: !isMac
                visible: !isMac || SettingsManager.debugMode
                Layout.columnSpan: 5
                Layout.fillWidth: true
                onVisibleChanged: {
                    if (!visible && isMac)
                        checked = false;

                }
                onCheckStateChanged: {
                    waterfall.antialiasing = checkState;
                }
            }

            CheckBox {
                id: removeAScanChB

                text: "A-Scan"
                checked: true
                Layout.columnSpan: 5
                Layout.fillWidth: true
                onCheckStateChanged: {
                    chart.visible = checkState;
                }
            }

            Label {
                text: "Plot Theme:"
            }

            PingComboBox {
                id: plotThemeCB

                Layout.columnSpan: 4
                Layout.fillWidth: true
                Layout.minimumWidth: 200
                model: waterfall.themes
                onCurrentTextChanged: waterfall.theme = currentText
            }

            Settings {
                property alias plotThemeIndex: plotThemeCB.currentIndex
                property alias removeAScanState: removeAScanChB.checkState
                property alias smoothDataState: smoothDataChB.checkState
                property alias waterfallAntialiasingData: antialiasingDataChB.checkState

                category: "Ping1DVisualizer"
            }

        }

    }

}
