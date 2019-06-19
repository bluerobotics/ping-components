#include "ping360.h"

#include <functional>

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include "hexvalidator.h"
#include "link/seriallink.h"
#include "networkmanager.h"
#include "networktool.h"
#include "notificationmanager.h"
#include "settingsmanager.h"

Q_LOGGING_CATEGORY(PING_PROTOCOL_PING360, "ping.protocol.ping360")

const int Ping360::_pingMaxFrequency = 50;

Ping360::Ping360()
    :PingSensor()
    ,_data(_num_points, 0)
{
    connect(this, &Sensor::connectionOpen, this, &Ping360::startPreConfigurationProcess);
}

void Ping360::startPreConfigurationProcess()
{
    // Fetch sensor configuration to update class variables
    ping_message_general_request msg;
    msg.set_requested_id(Ping360Namespace::DeviceData);
    msg.updateChecksum();
    writeMessage(msg);
}

void Ping360::loadLastSensorConfigurationSettings()
{
    //TODO
}

void Ping360::updateSensorConfigurationSettings()
{
    //TODO
}

void Ping360::connectLink(LinkType connType, const QStringList& connString)
{
    Sensor::connectLink(LinkConfiguration{connType, connString});
    startPreConfigurationProcess();
}

void Ping360::handleMessage(const ping_message& msg)
{
    qCDebug(PING_PROTOCOL_PING360) << "Handling Message:" << msg.message_id();

    switch (msg.message_id()) {

    case Ping360Namespace::DeviceData: {
        const ping360_device_data deviceData(msg);
        deviceData.mode();
        _gain_setting = deviceData.gain_setting();
        _angle = deviceData.angle();
        _transmit_duration = deviceData.transmit_duration();
        deviceData.transmit_duration();
        deviceData.transmit_frequency();
        deviceData.number_of_samples();
        deviceData.data_length();
        deviceData.data();

        for (int i = 0; i < deviceData.data_length(); i++) {
            _data.replace(i, deviceData.data()[i] / 255.0);
        }

        emit gainSettingChanged();
        emit angleChanged();
        emit transmitDurationChanged();
        emit dataChanged();

        break;
    }

    default:
        qWarning(PING_PROTOCOL_PING360) << "UNHANDLED MESSAGE ID:" << msg.message_id();
        break;
    }
    emit parsedMsgsUpdate();
}

void Ping360::firmwareUpdate(QString fileUrl, bool sendPingGotoBootloader, int baud, bool verify)
{
    Q_UNUSED(fileUrl)
    Q_UNUSED(sendPingGotoBootloader)
    Q_UNUSED(baud)
    Q_UNUSED(verify)
    //TODO
}

void Ping360::flash(const QString& fileUrl, bool sendPingGotoBootloader, int baud, bool verify)
{
    Q_UNUSED(fileUrl)
    Q_UNUSED(sendPingGotoBootloader)
    Q_UNUSED(baud)
    Q_UNUSED(verify)
    //TODO
}

void Ping360::setLastSensorConfiguration()
{
    //TODO
}

void Ping360::printSensorInformation() const
{
    qCDebug(PING_PROTOCOL_PING360) << "Ping360 Status:";
    //TODO
}

void Ping360::checkNewFirmwareInGitHubPayload(const QJsonDocument& jsonDocument)
{
    Q_UNUSED(jsonDocument)
    //TODO
}

void Ping360::resetSensorLocalVariables()
{
    //TODO
}

QQuickItem* Ping360::controlPanel(QObject *parent)
{
    QQmlEngine *engine = qmlEngine(parent);
    if(!engine) {
        qCDebug(PING_PROTOCOL_PING360) << "No qml engine to load visualization.";
        return nullptr;
    }
    QQmlComponent component(engine, QUrl("qrc:/Ping360ControlPanel.qml"), parent);
    _controlPanel.reset(qobject_cast<QQuickItem*>(component.create()));
    _controlPanel->setParentItem(qobject_cast<QQuickItem*>(parent));
    return _controlPanel.get();
}

QQmlComponent* Ping360::sensorVisualizer(QObject *parent)
{
    QQmlEngine *engine = qmlEngine(parent);
    if(!engine) {
        qCDebug(PING_PROTOCOL_PING360) << "No qml engine to load visualization.";
        return nullptr;
    }
    return new QQmlComponent(engine, QUrl("qrc:/Ping360Visualizer.qml"), parent);
}

Ping360::~Ping360()
{
    updateSensorConfigurationSettings();
}
