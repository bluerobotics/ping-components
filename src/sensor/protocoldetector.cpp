#include <QDebug>
#include <QFuture>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QUdpSocket>
#include <QtConcurrent>

#include "logger.h"
#include "protocoldetector.h"
#include "settingsmanager.h"
#include <ping-message-common.h>
#include <ping-message-ping1d.h>
#include <ping-message.h>

PING_LOGGING_CATEGORY(PING_PROTOCOL_PROTOCOLDETECTOR, "ping.protocol.protocoldetector")

const QStringList ProtocolDetector::_invalidSerialPortNames({
#ifdef Q_OS_OSX
    "cu.",
    "SPPDev",
    "iPhone",
    "Bluetooth",
#endif

#ifdef Q_OS_LINUX
    "ttyS",
#endif
});

ProtocolDetector::ProtocolDetector()
{
    // Register type to be used in availableLinksChanged
    qRegisterMetaType<QVector<LinkConfiguration>>();

    // To find a ping, we use this message on a link, then wait for a reply
    common_general_request deviceInformationMessage;
    deviceInformationMessage.set_requested_id(CommonId::DEVICE_INFORMATION);
    deviceInformationMessage.updateChecksum();
    _deviceInformationMessageByteArray = QByteArray(
        reinterpret_cast<const char*>(deviceInformationMessage.msgData), deviceInformationMessage.msgDataLength());

    _linkConfigs.append({{LinkType::Udp, {"192.168.2.2", "9090"}, "BlueRov2 Ping1D Port"},
        {LinkType::Udp, {"192.168.2.2", "9092"}, "BlueRov2 Ping360 Port"},
        {LinkType::Udp, {"127.0.0.1", "1234"}, "Development port"}});

    // Load user personalized links
    _linkConfigs.append(*SettingsManager::self()->lastLinkConfigurations());

    for (auto linkConfiguration : _linkConfigs) {
        qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Initial search list:" << linkConfiguration;
    }

    /** Encapsulate doScan to avoid direct call.
     * This protect us to freeze the main loop calling doScan directly
     * doScan should only be called by connection from others threads
     * or calling scan directly (that is a signal emission).
     */
    connect(this, &ProtocolDetector::scan, this, &ProtocolDetector::doScan);
};

void ProtocolDetector::doScan()
{
    _active = true;
    LinkConfiguration linkConf;

    // Scan until something is connected
    while (_active) {
        auto linksConf = updateLinkConfigurations(_linkConfigs);
        qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Looking for devices in:" << linksConf;
        for (LinkConfiguration& tryLinkConf : linksConf) {
            if (!_active) {
                break;
            }
            linkConf = tryLinkConf;
            checkLink(linkConf);
        }

        QVector<LinkConfiguration> availableLinksCopy = _availableLinks;
        emit availableLinksChanged(availableLinksCopy, QStringLiteral("Ping Protocol Detector"));
        _availableLinks.clear();
        QThread::msleep(500);
    }
    qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Scan finished.";
}

bool ProtocolDetector::checkLink(LinkConfiguration& linkConf)
{
    _detected = false;
    _parser.clearBuffer();
    if (linkConf.type() == LinkType::Udp) {
        checkUdp(linkConf);
    } else if (linkConf.type() == LinkType::Serial) {
        checkSerial(linkConf);
    } else {
        qDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Couldn't handle configuration:" << linkConf;
    }

    if (_detected) {
        qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Ping detected on:" << linkConf;
        if (!_availableLinks.contains(linkConf)) {
            _availableLinks.append(linkConf);
        }
        emit connectionDetected(linkConf);
    }
    return _detected;
}

QVector<LinkConfiguration> ProtocolDetector::updateLinkConfigurations(QVector<LinkConfiguration>& linkConfig) const
{
    QVector<LinkConfiguration> tempConfigs;
    auto portsInfo = QSerialPortInfo::availablePorts();
    for (const auto& portInfo : portsInfo) {
        // Do not run with invalid ports
        if (!isValidPort(portInfo)) {
            continue;
        }

        // By default, all sensors should deal with 115200 auto baudrate detection
        auto config = {portInfo.portName(), QString::number(115200)};
        tempConfigs.append({LinkType::Serial, config, QString("Detector serial link")});
    }
    return linkConfig + tempConfigs;
}

bool ProtocolDetector::checkSerial(LinkConfiguration& linkConf)
{
    QSerialPortInfo portInfo(linkConf.serialPort());
    int baudrate = linkConf.serialBaudrate();

    // Check if port can be opened
    if (!canOpenPort(portInfo, 500)) {
        qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Couldn't open port" << portInfo.portName();
        return false;
    }

    QSerialPort port(portInfo);

    qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Probing Serial" << port.portName() << baudrate;

    if (!port.open(QIODevice::ReadWrite)) {
        qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Fail to open";
        return false;
    }
    qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Port is open";

    port.setBaudRate(baudrate);
    port.setBreakEnabled(true);
    QThread::msleep(50);
    port.setBreakEnabled(false);
    QThread::msleep(11);
    port.write("UUU");
    port.flush();
    QThread::msleep(11);

    // Probe
    port.write(_deviceInformationMessageByteArray);
    port.waitForBytesWritten(100);

    int attempts = 0;

    // Try to get a valid response, timeout after 10 * 50 ms
    while (_active && !_detected && attempts++ < 10) {
        port.waitForReadyRead(50);
        _detected = checkBuffer(port.readAll(), linkConf);
    }

    port.close();

    return _detected;
}

bool ProtocolDetector::checkUdp(LinkConfiguration& linkConf)
{
    QUdpSocket socket;

    // To test locally, change the host to 127.0.0.1 and use something like:
    // nc -kul 127.0.0.1 8888 > /dev/ttyUSB0 < /dev/ttyUSB0
    // or
    // socat -T0.1 UDP-LISTEN:1234,fork,reuseaddr,ignoreeof OPEN:/dev/ttyUSB0,b115200,raw,ignoreeof

    qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Probing UDP:" << linkConf;

    // Connect with server
    socket.connectToHost(linkConf.udpHost(), linkConf.udpPort());

    // Give the socket half second to connect to the other side otherwise error out
    int socketAttemps = 0;
    while (!socket.waitForConnected(100) && socketAttemps < 5) {
        // Increases socketAttemps here to avoid empty loop optimization
        socketAttemps++;
    }
    if (socket.state() != QUdpSocket::ConnectedState) {
        qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Socket is not in connected state.";
        QString errorMessage = QStringLiteral("Error (%1): %2.").arg(socket.state()).arg(socket.errorString());
        qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << errorMessage;
        return _detected;
    }

    // Send message
    socket.write(_deviceInformationMessageByteArray);

    int attempts = 0;

    // Try to get a valid response, timeout after 10 * 50 ms
    while (_active && !_detected && attempts++ < 20) {
        socket.waitForReadyRead(50);
        /**
         * The connection state should be checked while looking for new packages
         */
        if (socket.state() != QUdpSocket::ConnectedState) {
            qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "Socket is not in connected state.";
            QString errorMessage = QStringLiteral("Error (%1): %2.").arg(socket.state()).arg(socket.errorString());
            qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << errorMessage;
            break;
        }
        _detected = checkBuffer(socket.readAll(), linkConf);
    }

    socket.close();

    // Close calls `disconnectFromHost`, this function waits until the socket is totally disconnected,
    // avoiding any future connection problems
    if (socket.state() == QUdpSocket::UnconnectedState || socket.waitForDisconnected(1000)) {
        qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << "UDP socket disconnected.";
    }

    return _detected;
}

bool ProtocolDetector::checkBuffer(const QByteArray& buffer, LinkConfiguration& linkConf)
{
    qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR) << buffer;
    for (const auto& byte : buffer) {
        if (_parser.parseByte(byte) == Parser::NEW_MESSAGE) {
            // Print information from detected devices
            common_device_information device_information(_parser.rxMessage());
            qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR)
                << "Detect new device:"
                << "\ndevice_type:" << device_information.device_type()
                << "\ndevice_revision:" << device_information.device_revision()
                << "\nfirmware_version_major:" << device_information.firmware_version_major()
                << "\nfirmware_version_minor:" << device_information.firmware_version_minor()
                << "\nfirmware_version_patch:" << device_information.firmware_version_patch();

            // TODO: Ping1D with firmware 3.26 or older
            // We should remove this code in future releases but allowing undetected devices to be flashed
            PingDeviceType type = static_cast<PingDeviceType>(device_information.device_type());
            if (type == PingDeviceType::PING360) {
                linkConf.setDeviceType(PingDeviceType::PING360);
            } else {
                linkConf.setDeviceType(PingDeviceType::PING1D);
            }
            return true;
        }
    }
    return false;
}

bool ProtocolDetector::canOpenPort(QSerialPortInfo& port, int msTimeout)
{
    // Call function asynchronously:
    auto checkPort = [](const QSerialPortInfo& portInfo) {
        QSerialPort serialPort(portInfo);
        bool ok = serialPort.open(QIODevice::ReadWrite);
        if (!ok) {
            qCWarning(PING_PROTOCOL_PROTOCOLDETECTOR)
                << "Fail to open serial port:" << portInfo.portName() << "reason:" << serialPort.error();
        }
        // Close will check if is open
        serialPort.close();
        return ok;
    };

    QFuture<bool> future = QtConcurrent::run(checkPort, port);
    // Wait for msTimeout
    float waitForTenthOfTimeout = 0;
    while (waitForTenthOfTimeout < 10 && !future.isFinished()) {
        QThread::msleep(msTimeout / 10.0f);
        qCDebug(PING_PROTOCOL_PROTOCOLDETECTOR)
            << "Waiting port to open.. " << waitForTenthOfTimeout << port.portName();
        waitForTenthOfTimeout += 1;
    }

    bool ok = false;
    if (future.isFinished()) {
        ok = future.result();
    }
    return ok;
}

bool ProtocolDetector::isValidPort(const QSerialPortInfo& serialPortInfo) const
{
    for (const auto& name : _invalidSerialPortNames) {
        if (serialPortInfo.portName().contains(name, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}
