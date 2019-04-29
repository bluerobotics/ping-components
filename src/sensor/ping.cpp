#include "ping.h"

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

Q_LOGGING_CATEGORY(PING_PROTOCOL_PING, "ping.protocol.ping")

const int Ping::_pingMaxFrequency = 50;

Ping::Ping() : Sensor()
{
    _points.reserve(_num_points);
    for (int i = 0; i < _num_points; i++) {
        _points.append(0);
    }
    _parser = new PingParser();
    connect(dynamic_cast<PingParser*>(_parser), &PingParser::newMessage, this, &Ping::handleMessage);
    connect(dynamic_cast<PingParser*>(_parser), &PingParser::parseError, this, &Ping::parserErrorsUpdate);
    connect(link(), &AbstractLink::newData, _parser, &Parser::parseBuffer);
    emit linkUpdate();

    _periodicRequestTimer.setInterval(1000);
    connect(&_periodicRequestTimer, &QTimer::timeout, this, [this] {
        if(!link()->isWritable())
        {
            qCWarning(PING_PROTOCOL_PING) << "Can't write in this type of link.";
            _periodicRequestTimer.stop();
            return;
        }

        if(!link()->isOpen())
        {
            qCCritical(PING_PROTOCOL_PING) << "Can't write, port is not open!";
            _periodicRequestTimer.stop();
            return;
        }

        // Update lost messages count
        _lostMessages = 0;
        for(const auto& requestedId : requestedIds)
        {
            _lostMessages += requestedId.waiting;
        }
        emit lostMessagesUpdate();

        request(PingPing1DNamespace::PcbTemperature);
        request(PingPing1DNamespace::ProcessorTemperature);
        request(PingPing1DNamespace::Voltage5);
        request(PingPing1DNamespace::ModeAuto);
    });

    //connectLink(LinkType::Serial, {"/dev/ttyUSB2", "115200"});

    connect(this, &Sensor::connectionOpen, this, &Ping::startPreConfigurationProcess);

    // Wait for device id to load the correct settings
    connect(this, &Ping::srcIdUpdate, this, &Ping::setLastPingConfiguration);

    connect(this, &Ping::firmwareVersionMinorUpdate, this, [this] {
        // Wait for firmware information to be available before looking for new versions
        static bool once = false;
        if(!once)
        {
            once = true;
            NetworkTool::self()->checkNewFirmware("ping1d", std::bind(&Ping::checkNewFirmwareInGitHubPayload, this,
                                                  std::placeholders::_1));
        }
    });

    connect(this, &Ping::autoDetectUpdate, this, [this](bool autodetect) {
        if(!autodetect) {
            if(detector()->isRunning()) {
                detector()->stop();
            }
        } else {
            if(!detector()->isRunning()) {
                detector()->scan();
            }
        }
    });

    // Load last successful connection
    auto config = SettingsManager::self()->lastLinkConfiguration();
    qCDebug(PING_PROTOCOL_PING) << "Loading last configuration connection from settings:" << config;
    addDetectionLink(config);
    detectorThread()->start();
}

void Ping::startPreConfigurationProcess()
{
    qCDebug(PING_PROTOCOL_PING) << "Start pre configuration task and requests.";
    if(!link()->isWritable()) {
        qCDebug(PING_PROTOCOL_PING) << "It's only possible to set last configuration when link is writable.";
        return;
    }

    setAutoDetect(false);
    SettingsManager::self()->lastLinkConfiguration(*link()->configuration());

    // Request device information
    request(PingPing1DNamespace::PingEnable);
    request(PingPing1DNamespace::ModeAuto);
    request(PingPing1DNamespace::Profile);
    request(PingPing1DNamespace::FirmwareVersion);
    request(PingPing1DNamespace::DeviceId);
    request(PingPing1DNamespace::SpeedOfSound);

    // Start periodic request timer
    _periodicRequestTimer.start();

    // Save configuration
    SettingsManager::self()->lastLinkConfiguration(*link()->configuration());
}

void Ping::loadLastPingConfigurationSettings()
{
    // Set default values
    for(const auto& key : _pingConfiguration.keys()) {
        _pingConfiguration[key].value = _pingConfiguration[key].defaultValue;
    }

    // Load settings for device using device id
    QVariant pingConfigurationVariant = SettingsManager::self()->getMapValue({"Ping", "PingConfiguration", QString(_srcId)});
    if(pingConfigurationVariant.type() != QVariant::Map) {
        qCWarning(PING_PROTOCOL_PING) << "No valid PingConfiguration in settings." << pingConfigurationVariant.type();
        return;
    }

    // Get the value of each configuration and set it on device
    auto map = pingConfigurationVariant.toMap();
    for(const auto& key : _pingConfiguration.keys()) {
        _pingConfiguration[key].set(map[key]);
    }
}

void Ping::updatePingConfigurationSettings()
{
    // Save all sensor configurations
    for(const auto& key : _pingConfiguration.keys()) {
        auto& dataStruct = _pingConfiguration[key];
        dataStruct.set(dataStruct.getClassValue());
        SettingsManager::self()->setMapValue({"Ping", "PingConfiguration", QString(_srcId), key}, dataStruct.value);
    }
}

void Ping::addDetectionLink(const LinkConfiguration& linkConfiguration)
{
    if(linkConfiguration.isValid()) {
        detector()->appendConfiguration(linkConfiguration);
    } else {
        qCDebug(PING_PROTOCOL_PING) << "Invalid configuration:" << linkConfiguration.errorToString();
    }
}

void Ping::connectLink(LinkType connType, const QStringList& connString)
{
    Sensor::connectLink(LinkConfiguration{connType, connString});
    startPreConfigurationProcess();
}

void Ping::handleMessage(ping_message msg)
{
    qCDebug(PING_PROTOCOL_PING) << "Handling Message:" << msg.message_id();

    auto& requestedId = requestedIds[static_cast<PingPing1DNamespace::ping_ping1D_id>(msg.message_id())];
    if(requestedId.waiting) {
        requestedId.waiting--;
        requestedId.ack++;
    }

    switch (msg.message_id()) {

    case PingMessageNamespace::Ack: {
        ping_message_ack ackMessage{msg};
        qCDebug(PING_PROTOCOL_PING) << "ACK message:" << ackMessage.acked_id();
        break;
    }

    case PingMessageNamespace::Nack: {
        ping_message_nack nackMessage{msg};
        qCCritical(PING_PROTOCOL_PING) << "Sensor NACK!";
        _nack_msg = QString("%1: %2").arg(nackMessage.nack_message()).arg(nackMessage.nacked_id());
        qCDebug(PING_PROTOCOL_PING) << "NACK message:" << _nack_msg;
        emit nackMsgUpdate();

        auto& nackRequestedId = requestedIds[static_cast<PingPing1DNamespace::ping_ping1D_id>(nackMessage.nacked_id())];
        if(nackRequestedId.waiting) {
            nackRequestedId.waiting--;
            nackRequestedId.nack++;
        }
        break;
    }

    // needs dynamic-payload patch
    case PingMessageNamespace::AsciiText: {
        _ascii_text = ping_message_ascii_text(msg).ascii_message();
        qCInfo(PING_PROTOCOL_PING) << "Sensor status:" << _ascii_text;
        emit asciiTextUpdate();
        break;
    }

    case PingPing1DNamespace::FirmwareVersion: {
        ping_ping1D_firmware_version m(msg);
        _device_type = m.device_type();
        _device_model = m.device_model();
        _firmware_version_major = m.firmware_version_major();
        _firmware_version_minor = m.firmware_version_minor();

        emit deviceTypeUpdate();
        emit deviceModelUpdate();
        emit firmwareVersionMajorUpdate();
        emit firmwareVersionMinorUpdate();
    }
    break;

    // This message is deprecated, it provides no added information because
    // the device id is already supplied in every message header
    case PingPing1DNamespace::DeviceId: {
        ping_ping1D_device_id m(msg);
        _srcId = m.src_device_id();

        emit srcIdUpdate();
    }
    break;

    case PingPing1DNamespace::Distance: {
        ping_ping1D_distance m(msg);
        _distance = m.distance();
        _confidence = m.confidence();
        _pulse_duration = m.pulse_duration();
        _ping_number = m.ping_number();
        _scan_start = m.scan_start();
        _scan_length = m.scan_length();
        _gain_index = m.gain_index();

        // TODO: change to distMsgUpdate() or similar
        emit distanceUpdate();
        emit pingNumberUpdate();
        emit confidenceUpdate();
        emit pulseDurationUpdate();
        emit scanStartUpdate();
        emit scanLengthUpdate();
        emit gainIndexUpdate();
    }
    break;

    case PingPing1DNamespace::DistanceSimple: {
        ping_ping1D_distance_simple m(msg);
        _distance = m.distance();
        _confidence = m.confidence();

        emit distanceUpdate();
        emit confidenceUpdate();
    }
    break;

    case PingPing1DNamespace::Profile: {
        ping_ping1D_profile m(msg);
        _distance = m.distance();
        _confidence = m.confidence();
        _pulse_duration = m.pulse_duration();
        _ping_number = m.ping_number();
        _scan_start = m.scan_start();
        _scan_length = m.scan_length();
        _gain_index = m.gain_index();
//        _num_points = m.profile_data_length(); // const for now
//        memcpy(_points.data(), m.data(), _num_points); // careful with constant

        // This is necessary to convert <uint8_t> to <int>
        // QProperty only supports vector<int>, otherwise, we could use memcpy, like the two lines above
        for (int i = 0; i < m.profile_data_length(); i++) {
            _points.replace(i, m.profile_data()[i] / 255.0);
        }

        // TODO: change to distMsgUpdate() or similar
        emit distanceUpdate();
        emit pingNumberUpdate();
        emit confidenceUpdate();
        emit pulseDurationUpdate();
        emit scanStartUpdate();
        emit scanLengthUpdate();
        emit gainIndexUpdate();
        emit pointsUpdate();
    }
    break;

    case PingPing1DNamespace::ModeAuto: {
        ping_ping1D_mode_auto m(msg);
        if(_mode_auto != m.mode_auto()) {
            _mode_auto = m.mode_auto();
            emit modeAutoUpdate();
        }
    }
    break;

    case  PingPing1DNamespace::PingEnable: {
        ping_ping1D_ping_enable m(msg);
        _ping_enable = m.ping_enabled();
        emit pingEnableUpdate();
    }
    break;

    case PingPing1DNamespace::PingInterval: {
        ping_ping1D_ping_interval m(msg);
        _ping_interval = m.ping_interval();
        emit pingIntervalUpdate();
    }
    break;

    case PingPing1DNamespace::Range: {
        ping_ping1D_range m(msg);
        _scan_start = m.scan_start();
        _scan_length = m.scan_length();
        emit scanLengthUpdate();
        emit scanStartUpdate();
    }
    break;

    case PingPing1DNamespace::GeneralInfo: {
        ping_ping1D_general_info m(msg);
        _gain_index = m.gain_index();
        emit gainIndexUpdate();
    }
    break;

    case PingPing1DNamespace::GainIndex: {
        ping_ping1D_gain_index m(msg);
        _gain_index = m.gain_index();
        emit gainIndexUpdate();
    }
    break;

    case PingPing1DNamespace::SpeedOfSound: {
        ping_ping1D_speed_of_sound m(msg);
        _speed_of_sound = m.speed_of_sound();
        emit speedOfSoundUpdate();
    }
    break;

    case PingPing1DNamespace::ProcessorTemperature: {
        ping_ping1D_processor_temperature m(msg);
        _processor_temperature = m.processor_temperature();
        emit processorTemperatureUpdate();
    }
    break;

    case PingPing1DNamespace::PcbTemperature: {
        ping_ping1D_pcb_temperature m(msg);
        _pcb_temperature = m.pcb_temperature();
        emit pcbTemperatureUpdate();
    }
    break;

    case PingPing1DNamespace::Voltage5: {
        ping_ping1D_voltage_5 m(msg);
        _board_voltage = m.voltage_5(); // millivolts
        emit boardVoltageUpdate();
    }
    break;

    default:
        qWarning(PING_PROTOCOL_PING) << "UNHANDLED MESSAGE ID:" << msg.message_id();
        break;
    }

    // TODO: is this signalling expensive?
    // we can cut down on this a lot in general
    _dstId = msg.dst_device_id();
    _srcId = msg.src_device_id();

    emit dstIdUpdate();
    emit srcIdUpdate();
    emit parsedMsgsUpdate();

//    printStatus();
}

void Ping::firmwareUpdate(QString fileUrl, bool sendPingGotoBootloader, int baud, bool verify)
{
    if(fileUrl.contains("http")) {
        NetworkManager::self()->download(fileUrl, [this, sendPingGotoBootloader, baud, verify](const QString& path) {
            qCDebug(FLASH) << "Downloaded firmware:" << path;
            flash(path, sendPingGotoBootloader, baud, verify);
        });
    } else {
        flash(fileUrl, sendPingGotoBootloader, baud, verify);
    }
}

void Ping::flash(const QString& fileUrl, bool sendPingGotoBootloader, int baud, bool verify)
{
    flasher()->setState(Flasher::Idle);
    flasher()->setState(Flasher::StartingFlash);
    if(!HexValidator::isValidFile(fileUrl)) {
        auto errorMsg = QStringLiteral("File does not contain a valid Intel Hex format: %1").arg(fileUrl);
        qCWarning(PING_PROTOCOL_PING) << errorMsg;
        flasher()->setState(Flasher::Error, errorMsg);
        return;
    };

    SerialLink* serialLink = dynamic_cast<SerialLink*>(link());
    if (!serialLink) {
        auto errorMsg = QStringLiteral("It's only possible to flash via serial.");
        qCWarning(PING_PROTOCOL_PING) << errorMsg;
        flasher()->setState(Flasher::Error, errorMsg);
        return;
    }

    if(!link()->isOpen()) {
        auto errorMsg = QStringLiteral("Link is not open to do the flash procedure.");
        qCWarning(PING_PROTOCOL_PING) << errorMsg;
        flasher()->setState(Flasher::Error, errorMsg);
        return;
    }

    // Stop requests and messages from the sensor
    _periodicRequestTimer.stop();
    setPingFrequency(0);

    if (sendPingGotoBootloader) {
        qCDebug(PING_PROTOCOL_PING) << "Put it in bootloader mode.";
        ping_ping1D_goto_bootloader m;
        m.updateChecksum();
        writeMessage(m);
    }

    // Wait for bytes to be written before finishing the connection
    while (serialLink->port()->bytesToWrite()) {
        qCDebug(PING_PROTOCOL_PING) << "Waiting for bytes to be written...";
        serialLink->port()->waitForBytesWritten();
        qCDebug(PING_PROTOCOL_PING) << "Done !";
    }

    qCDebug(PING_PROTOCOL_PING) << "Finish connection.";
    // TODO: Move thread delay to something more.. correct.
    QThread::msleep(1000);
    link()->finishConnection();

    QSerialPortInfo pInfo(serialLink->port()->portName());
    QString portLocation = pInfo.systemLocation();

    qCDebug(PING_PROTOCOL_PING) << "Save sensor configuration.";
    updatePingConfigurationSettings();

    qCDebug(PING_PROTOCOL_PING) << "Start flash.";
    QThread::msleep(1000);

    flasher()->setBaudRate(baud);
    flasher()->setFirmwarePath(fileUrl);
    flasher()->setLink(link()->configuration()[0]);
    flasher()->setVerify(verify);
    flasher()->flash();

    QThread::msleep(500);
    // Clear last configuration src ID to detect device as a new one
    connect(&_flasher, &Flasher::stateChanged, this, [this] {
        if(flasher()->state() == Flasher::States::FlashFinished)
        {
            QThread::msleep(500);
            // Clear last configuration src ID to detect device as a new one
            resetSensorLocalVariables();
            detector()->scan();
        }

    });
}

void Ping::request(int id)
{
    if(!link()->isWritable()) {
        qCWarning(PING_PROTOCOL_PING) << "Can't write in this type of link.";
        return;
    }
    qCDebug(PING_PROTOCOL_PING) << "Requesting:" << id;

    ping_message_empty m;
    m.set_id(id);
    m.updateChecksum();
    writeMessage(m);

    // Add requested id
    requestedIds[static_cast<PingPing1DNamespace::ping_ping1D_id>(id)].waiting++;
}

void Ping::setLastPingConfiguration()
{
    if(_lastPingConfigurationSrcId == _srcId) {
        return;
    }
    _lastPingConfigurationSrcId = _srcId;
    if(!link()->isWritable()) {
        qCDebug(PING_PROTOCOL_PING) << "It's only possible to set last configuration when link is writable.";
        return;
    }

    // Load previous configuration with device id
    loadLastPingConfigurationSettings();

    // Print last configuration
    QString output = QStringLiteral("\nPingConfiguration {\n");
    for(const auto& key : _pingConfiguration.keys()) {
        output += QString("\t%1: %2\n").arg(key).arg(_pingConfiguration[key].value);
    }
    output += QStringLiteral("}");
    qCDebug(PING_PROTOCOL_PING).noquote() << output;


    // Set loaded configuration in device
    static QString debugMessage =
        QStringLiteral("Device configuration does not match. Waiting for (%1), got (%2) for %3");
    static auto lastPingConfigurationTimer = new QTimer();
    connect(lastPingConfigurationTimer, &QTimer::timeout, this, [this] {
        bool stopLastPingConfigurationTimer = true;
        for(const auto& key : _pingConfiguration.keys())
        {
            auto& dataStruct = _pingConfiguration[key];
            if(dataStruct.value != dataStruct.getClassValue()) {
                qCDebug(PING_PROTOCOL_PING) <<
                                            debugMessage.arg(dataStruct.value).arg(dataStruct.getClassValue()).arg(key);
                dataStruct.setClassValue(dataStruct.value);
                stopLastPingConfigurationTimer = false;
            }
            if(key.contains("automaticMode") && dataStruct.value) {
                qCDebug(PING_PROTOCOL_PING) << "Device was running with last configuration in auto mode.";
                // If it's running in automatic mode
                // no further configuration is necessary
                break;
            }
        }
        if(stopLastPingConfigurationTimer)
        {
            qCDebug(PING_PROTOCOL_PING) << "Last configuration done, timer will stop now.";
            lastPingConfigurationTimer->stop();
            do_continuous_start(PingPing1DNamespace::Profile);
        }
    });
    lastPingConfigurationTimer->start(500);
    lastPingConfigurationTimer->start();
}

void Ping::setPingFrequency(float pingFrequency)
{
    if (pingFrequency <= 0 || pingFrequency > _pingMaxFrequency) {
        qCWarning(PING_PROTOCOL_PING) << "Invalid frequency:" << pingFrequency;
        do_continuous_stop(PingPing1DNamespace::Profile);
    } else {
        int periodMilliseconds = 1000.0f / pingFrequency;
        qCDebug(PING_PROTOCOL_PING) << "Setting frequency(Hz) and period(ms):" << pingFrequency << periodMilliseconds;
        set_ping_interval(periodMilliseconds);
        do_continuous_start(PingPing1DNamespace::Profile);
    }
    qCDebug(PING_PROTOCOL_PING) << "Ping frequency" << pingFrequency;
}

void Ping::printStatus()
{
    qCDebug(PING_PROTOCOL_PING) << "Ping Status:";
    qCDebug(PING_PROTOCOL_PING) << "\t- srcId:" << _srcId;
    qCDebug(PING_PROTOCOL_PING) << "\t- dstID:" << _dstId;
    qCDebug(PING_PROTOCOL_PING) << "\t- device_type:" << _device_type;
    qCDebug(PING_PROTOCOL_PING) << "\t- device_model:" << _device_model;
    qCDebug(PING_PROTOCOL_PING) << "\t- firmware_version_major:" << _firmware_version_major;
    qCDebug(PING_PROTOCOL_PING) << "\t- firmware_version_minor:" << _firmware_version_minor;
    qCDebug(PING_PROTOCOL_PING) << "\t- distance:" << _distance;
    qCDebug(PING_PROTOCOL_PING) << "\t- confidence:" << _confidence;
    qCDebug(PING_PROTOCOL_PING) << "\t- pulse_duration:" << _pulse_duration;
    qCDebug(PING_PROTOCOL_PING) << "\t- ping_number:" << _ping_number;
    qCDebug(PING_PROTOCOL_PING) << "\t- start_mm:" << _scan_start;
    qCDebug(PING_PROTOCOL_PING) << "\t- length_mm:" << _scan_length;
    qCDebug(PING_PROTOCOL_PING) << "\t- gain_index:" << _gain_index;
    qCDebug(PING_PROTOCOL_PING) << "\t- mode_auto:" << _mode_auto;
    qCDebug(PING_PROTOCOL_PING) << "\t- ping_interval:" << _ping_interval;
    qCDebug(PING_PROTOCOL_PING) << "\t- points:" << QByteArray((const char*)_points.data(), _num_points).toHex(',');
}

void Ping::writeMessage(const ping_message &msg)
{
    if(link() && link()->isOpen() && link()->isWritable()) {
        link()->write(reinterpret_cast<const char*>(msg.msgData), msg.msgDataLength());
    }
}

void Ping::checkNewFirmwareInGitHubPayload(const QJsonDocument& jsonDocument)
{
    float lastVersionAvailable = 0.0;

    auto filesPayload = jsonDocument.array();
    for(const QJsonValue& filePayload : filesPayload) {
        qCDebug(PING_PROTOCOL_PING) << filePayload["name"].toString();

        // Get version from Ping_V(major).(patch)_115kb.hex where (major).(patch) is <version>
        static const QRegularExpression versionRegex(QStringLiteral(R"(Ping_V(?<version>\d+\.\d+)_115kb\.hex)"));
        auto filePayloadVersion = versionRegex.match(filePayload["name"].toString()).captured("version").toFloat();
        _firmwares[filePayload["name"].toString()] = filePayload["download_url"].toString();

        if(filePayloadVersion > lastVersionAvailable) {
            lastVersionAvailable = filePayloadVersion;
        }
    }
    emit firmwaresAvailableUpdate();

    auto sensorVersion = QString("%1.%2").arg(_firmware_version_major).arg(_firmware_version_minor).toFloat();
    static QString firmwareUpdateSteps{"https://github.com/bluerobotics/ping-viewer/wiki/firmware-update"};
    if(lastVersionAvailable > sensorVersion) {
        QString newVersionText =
            QStringLiteral("Firmware update for Ping available: %1<br>").arg(lastVersionAvailable) +
            QStringLiteral("<a href=\"%1\">Check firmware update steps here!</a>").arg(firmwareUpdateSteps);
        NotificationManager::self()->create(newVersionText, "green", StyleManager::infoIcon());
    }
}

void Ping::resetSensorLocalVariables()
{
    _ascii_text = QString();
    _nack_msg = QString();

    _srcId = 0;
    _dstId = 0;

    _device_type = 0;
    _device_model = 0;
    _firmware_version_major = 0;
    _firmware_version_minor = 0;

    _distance = 0;
    _confidence = 0;
    _pulse_duration = 0;
    _ping_number = 0;
    _scan_start = 0;
    _scan_length = 0;
    _gain_index = 0;
    _speed_of_sound = 0;

    _processor_temperature = 0;
    _pcb_temperature = 0;
    _board_voltage = 0;

    _ping_enable = false;
    _mode_auto = 0;
    _ping_interval = 0;

    _lastPingConfigurationSrcId = -1;
}

Ping::~Ping()
{
    updatePingConfigurationSettings();
}

QDebug operator<<(QDebug d, const Ping::messageStatus& other)
{
    return d << "waiting: " << other.waiting << ", ack: " << other.ack << ", nack: " << other.nack;
}
