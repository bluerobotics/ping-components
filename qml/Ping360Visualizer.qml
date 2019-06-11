import QtGraphicalEffects 1.0
import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Controls 1.4 as QC1
import QtQuick.Layouts 1.3
import QtQuick.Shapes 1.0
import Qt.labs.settings 1.0
import WaterfallPlot 1.0
import PolarPlot 1.0

import DeviceManager 1.0
import SettingsManager 1.0
import StyleManager 1.0

Item {
    id: root
    property alias waterfallItem: waterfall
    anchors.fill: parent

    Connections {
        property var ping: DeviceManager.primarySensor
        target: ping

        onDataChanged: {
            // Move from mm to m
            root.draw(ping.data, 100, 0, 100, 0)
        }
    }

    onWidthChanged: {
        if(chart.Layout.minimumWidth === chart.width) {
            waterfall.parent.width = width - chart.width
        }
    }

    function draw(points, confidence, initialPoint, length, distance) {
        waterfall.draw(points, confidence, initialPoint, length, distance)
        chart.draw(points, length + initialPoint, initialPoint)
    }

    function setDepth(depth) {
        readout.value = depth
    }

    function setConfidence(perc) {
        readout.confidence = perc
    }

    QC1.SplitView {
        orientation: Qt.Horizontal
        anchors.fill: parent

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            PolarPlot {
                id: waterfall
                height: Math.min(parent.height, parent.width)
                width: height
                anchors.centerIn: parent

                Shape {
                    visible: waterfall.containsMouse
                    anchors.centerIn: parent
                    opacity: 0.5
                    ShapePath {
                        strokeWidth: 3
                        strokeColor: StyleManager.secondaryColor
                        startX: 0
                        startY: 0
                        //TODO: This need to be updated in sensor integration
                        PathLine {
                            property real angle: -Math.atan2(waterfall.mousePos.x - waterfall.width/2, waterfall.mousePos.y - waterfall.height/2) + Math.PI/2
                            x: waterfall.width*Math.cos(angle)/2
                            y: waterfall.height*Math.sin(angle)/2
                        }
                    }
                }

                Text {
                    id: mouseReadout
                    visible: waterfall.containsMouse
                    x: waterfall.mousePos.x - width/2
                    y: waterfall.mousePos.y - height*2
                    text: (waterfall.mouseSampleDepth*SettingsManager.distanceUnits['distanceScalar']).toFixed(2) + SettingsManager.distanceUnits['distance']
                    color: confidenceToColor(waterfall.mouseSampleConfidence)
                    font.family: "Arial"
                    font.pointSize: 15
                    font.bold: true

                    Text {
                        id: mouseConfidenceText
                        x: mouseReadout.width - width
                        y: mouseReadout.height*4/5
                        text: transformValue(waterfall.mouseSampleConfidence) + "%"
                        visible: typeof(waterfall.mouseSampleConfidence) == "number"
                        color: confidenceToColor(waterfall.mouseSampleConfidence)
                        font.family: "Arial"
                        font.pointSize: 10
                        font.bold: true
                    }
                }
            }
        }

        Chart {
            id: chart
            Layout.fillHeight: true
            Layout.maximumWidth: 250
            Layout.preferredWidth: 100
            Layout.minimumWidth: 75
        }

        Settings {
            property alias chartWidth: chart.width
        }
    }

    ValueReadout {
        id: readout
    }

    function confidenceToColor(confidence) {
        return Qt.rgba(2*(1 - confidence/100), 2*confidence/100, 0)
    }

    function transformValue(value, precision) {
        return typeof(value) == "number" ? value.toFixed(precision) : value + ' '
    }
}
