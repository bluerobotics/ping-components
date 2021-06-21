#include <QQmlEngine>

#include "devicemanager.h"
#include "filelink.h"
#include "logger.h"
#include "ping.h"
#include "ping360.h"
#include "settingsmanager.h"

PING_LOGGING_CATEGORY(DEVICEMANAGER, "ping.devicemanager");

DeviceManager::DeviceManager()
    : _detector(new ProtocolDetector())
{
    for (const auto& key : _roleNames.keys()) {
        _roles.append(key);
        _sensors.insert(key, {});
    }

    _detector->moveToThread(&_detectorThread);
    connect(&_detectorThread, &QThread::started, _detector, &ProtocolDetector::scan);
    // We need to take care here to not erase configurations that are already connected
    // Also we should remove or set Connected to false if device is disconnected
    connect(_detector, &ProtocolDetector::availableLinksChanged, this, &DeviceManager::updateAvailableConnections);
    connect(Ping360HelperService::self(), &Ping360HelperService::availableLinkFound, this,
        &DeviceManager::updateAvailableConnections);
}

void DeviceManager::append(const LinkConfiguration& linkConf, const QString& deviceName, const QString& detectorName)
{
    for (int i {0}; i < _sensors[Connection].size(); i++) {
        const auto vectorLinkConf = _sensors[Connection][i].value<QSharedPointer<LinkConfiguration>>();
        if (!vectorLinkConf.isNull() && *vectorLinkConf == linkConf) {
            qCDebug(DEVICEMANAGER) << "Connection configuration already exist for:" << _sensors[Name][i] << linkConf
                                   << linkConf.argsAsConst();
            _sensors[Available][i] = true;
            const auto indexRow = index(i);
            emit dataChanged(indexRow, indexRow, _roles);
            return;
        }
    }

    qCDebug(DEVICEMANAGER) << "Add connection configuration for:" << deviceName << linkConf;

    const int line = rowCount();
    beginInsertRows(QModelIndex(), line, line);
    _sensors[Available].append(true);
    _sensors[Connection].append(
        QVariant::fromValue(QSharedPointer<LinkConfiguration>(new LinkConfiguration(linkConf))));
    _sensors[Connected].append(false);
    _sensors[DetectorName].append(detectorName);
    _sensors[Name].append(deviceName);
    _sensors[UnavailableCounter].append(0);

    const auto& indexRow = index(line);
    endInsertRows();
    emit dataChanged(indexRow, indexRow, _roles);
    emit countChanged();
}

void DeviceManager::startDetecting()
{
    qCDebug(DEVICEMANAGER) << "Start protocol detector service.";
    _detectorThread.start();
    Ping360HelperService::self()->startBroadcastService();
}

void DeviceManager::stopDetecting()
{
    qCDebug(DEVICEMANAGER) << "Stop protocol detector service.";
    Ping360HelperService::self()->stopBroadcastService();
    _detector->stop();
    _detectorThread.quit();
}

void DeviceManager::connectLink(LinkConfiguration* linkConf)
{
    // Stop detector if we are going to connect with something
    stopDetecting();

    // Find configuration in vector with valid index
    int objIndex = -1;
    for (int i {0}; i < _sensors[Connection].size(); i++) {
        auto sensorLinkConf = _sensors[Connection][i].value<QSharedPointer<LinkConfiguration>>().get();
        if (*sensorLinkConf == *linkConf) {
            objIndex = i;
            break;
        }
    }

    // index is -1 when connection configuration does not exist in list
    if (objIndex < 0) {
        qCWarning(DEVICEMANAGER) << "Connection configuration does not exist in list.";
        return;
    }

    _sensors[Name][objIndex] = PingHelper::nameFromDeviceType(linkConf->deviceType());
    qCDebug(DEVICEMANAGER) << "Connecting with sensor:" << _sensors[Name][objIndex].toString() << *linkConf;

    // We could use a single Ping instance, but since we are going to support multiple devices
    // this pointer will hold everything for us
    if (linkConf->deviceType() == PingDeviceType::PING1D) {
        _primarySensor.reset(new Ping());
    } else {
        _primarySensor.reset(new Ping360());
    }

    emit primarySensorChanged();
    _primarySensor->connectLink(*linkConf);
    _sensors[Connected][objIndex] = true;
}

void DeviceManager::connectLinkDirectly(const LinkConfiguration& linkConfiguration)
{
    if (linkConfiguration.deviceType() == PingEnumNamespace::PingDeviceType::UNKNOWN) {
        qCWarning(DEVICEMANAGER) << "Link configuration does not provide valid sensor type:" << linkConfiguration;
        return;
    }

    // Do not add simulation links in lastLinkConfigurations
    if (!linkConfiguration.isSimulation()) {
        SettingsManager::self()->lastLinkConfigurations()->append(linkConfiguration);
    }

    append(linkConfiguration);
    std::remove_const<LinkConfiguration>::type removeConstlinkConfiguration = linkConfiguration;
    connectLink(&removeConstlinkConfiguration);
}

void DeviceManager::connectLinkDirectly(AbstractLinkNamespace::LinkType connType, const QStringList& connString,
    PingEnumNamespace::PingDeviceType deviceType)
{
    auto linkConfiguration = LinkConfiguration {connType, connString};
    linkConfiguration.setDeviceType(deviceType);

    connectLinkDirectly(linkConfiguration);
}

void DeviceManager::playLogFile(AbstractLinkNamespace::LinkType connType, const QStringList& connString)
{
    // Update link configuration with sensor information that is included in the log file
    auto linkConfiguration = LinkConfiguration {connType, connString};
    linkConfiguration.setDeviceType(FileLink::staticLogSensorStruct(linkConfiguration).sensor.type.ping);

    // Append configuration as device of type "None"
    // This will create and populate all necessary roles before connecting
    append(linkConfiguration);
    connectLink(&linkConfiguration);
}

void DeviceManager::updateAvailableConnections(
    const QVector<LinkConfiguration>& availableLinkConfigurations, const QString& detector)
{
    qCDebug(DEVICEMANAGER) << "Available devices:" << availableLinkConfigurations;

    for (int i {0}; i < _sensors[Available].size(); i++) {
        const auto linkConf = _sensors[Connection][i].value<QSharedPointer<LinkConfiguration>>();
        if (linkConf->isSimulation() || _sensors[DetectorName][i] != detector) {
            continue;
        }

        // Make all connections unavailable by default
        _sensors[Available][i] = false;
        const auto indexRow = index(i);
        emit dataChanged(indexRow, indexRow, _roles);
    }

    // Check if the configuration already exists for a sensor
    // Serial ports does not support multiple devices connected
    // Some sensors, like Ping1D, can fail to answer a ABR procedure
    for (const auto& link : availableLinkConfigurations) {
        const bool sameSerialDevice = std::any_of(
            _sensors[Connection].cbegin(), _sensors[Connection].cend(), [&link](const QVariant& variantLink) {
                const auto sensorLink = variantLink.value<QSharedPointer<LinkConfiguration>>();
                qCDebug(DEVICEMANAGER) << "Device" << *sensorLink
                                       << "already already provided by a different connection:" << link;
                return link.serialPort() == sensorLink->serialPort() && link != *sensorLink;
            });

        if (!sameSerialDevice) {
            append(link, PingHelper::nameFromDeviceType(link.deviceType()), detector);
        }
    }

    // We'll let the link to fail the communication attempt "a max number of fails" before making it unavailable
    // This is necessary to avoid any problem related to automatic baud rates problem from the sensor side.
    static const int maxNumberOfFails = 3;
    for (int i {0}; i < _sensors[Available].size(); i++) {
        const auto linkConf = _sensors[Connection][i].value<QSharedPointer<LinkConfiguration>>();
        if (linkConf->isSimulation()) {
            continue;
        }

        const auto indexRow = index(i);

        // The sensor was detected, we can remove unavailable counter
        if (_sensors[Available][i].toBool()) {
            _sensors[UnavailableCounter][i] = 0;
            emit dataChanged(indexRow, indexRow, _roles);
            continue;
        }
        _sensors[Available][i] = _sensors[UnavailableCounter][i].toInt() < maxNumberOfFails;
        _sensors[UnavailableCounter][i] = _sensors[UnavailableCounter][i].toInt() + 1;

        emit dataChanged(indexRow, indexRow, _roles);
    }
}

void DeviceManager::clear()
{
    beginResetModel();
    for (const auto category : _roleNames.keys()) {
        _sensors[category].clear();
    }
    endResetModel();
    emit countChanged();
}

QObject* DeviceManager::qmlSingletonRegister(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    return self();
}

DeviceManager* DeviceManager::self()
{
    // The QML engine will kill it for us since this object is created inside the QML engine
    // and has as childrens QML elements
    static DeviceManager* self = new DeviceManager();
    return self;
}

DeviceManager::~DeviceManager()
{
    stopDetecting();
    _detectorThread.wait();
}
