#include "devicemanager.h"
#include "logger.h"

PING_LOGGING_CATEGORY(DEVICEMANAGER, "ping.devicemanager");

DeviceManager::DeviceManager() :
    _detector(new ProtocolDetector())
{
    // This helps to avoid type errors in qml
    _primarySensor.reset(new Ping());
    emit primarySensorChanged();

    for(const auto& key : _roleNames.keys()) {
        _roles.append(key);
        _sensors.insert(key, {});
    }

    _detector->moveToThread(&_detectorThread);
    connect(&_detectorThread, &QThread::started, _detector, &ProtocolDetector::scan);
    // We need to take care here to not erase configurations that are already connected
    // Also we should remove or set Connected to false if device is disconnected
    connect(_detector, &ProtocolDetector::availableLinksChanged, this, [this](auto links) {
        qCDebug(DEVICEMANAGER) << "Available devices:" << links;
        updateAvailableConnections(links);
    });
    append({AbstractLinkNamespace::PingSimulation}, "Ping1D");
}

void DeviceManager::append(const LinkConfiguration& linkConf, const QString& deviceName)
{
    for(int i{0}; i < _sensors[Connection].size(); i++) {
        auto vectorLinkConf = _sensors[Connection][i].value<QSharedPointer<LinkConfiguration>>().get();
        if(vectorLinkConf->argsAsConst() == linkConf.argsAsConst()) {
            qCDebug(DEVICEMANAGER) << "Connection configuration already exist.";
            qCDebug(DEVICEMANAGER) << linkConf;
            _sensors[Available][i] = true;
            const auto indexRow = index(i);
            emit dataChanged(indexRow, indexRow, _roles);
            return;
        }
    }
    const int line = rowCount();
    beginInsertRows(QModelIndex(), line, line);
    _sensors[Available].append(true);
    _sensors[Connection].append(QVariant::fromValue(QSharedPointer<LinkConfiguration>(new LinkConfiguration(linkConf))));
    _sensors[Connected].append(false); // TODO: Make it true for primarySensor
    _sensors[Name].append(deviceName);

    const auto& indexRow = index(line);
    endInsertRows();
    emit dataChanged(indexRow, indexRow, _roles);
    emit countChanged();
}

void DeviceManager::startDetecting()
{
    qCDebug(DEVICEMANAGER) << "Start protocol detector service.";
    _detectorThread.start();
}

void DeviceManager::stopDetecting()
{
    qCDebug(DEVICEMANAGER) << "Stop protocol detector service.";
    _detector->stop();
    _detectorThread.quit();
    _detectorThread.wait();
}

void DeviceManager::connectLink(LinkConfiguration* linkConf)
{
    // Find configuration in vector with valid index
    int objIndex = -1;
    for(int i{0}; i < _sensors[Connection].size(); i++) {
        auto sensorLinkConf = _sensors[Connection][i].value<QSharedPointer<LinkConfiguration>>().get();
        if(sensorLinkConf->argsAsConst() == linkConf->argsAsConst()) {
            objIndex = i;
            break;
        }
    }

    // index is -1 when connection configuration does not exist in list
    if(objIndex < 0) {
        qCWarning(DEVICEMANAGER) << "Connection configuration does not exist in list.";
        return;
    }

    qCDebug(DEVICEMANAGER) << "Connecting with configuration:" << linkConf[0];

    // We could use a single Ping instance, but since we are going to support multiple devices
    // this pointer will hold everything for us
    _primarySensor.reset(new Ping());
    emit primarySensorChanged();
    _primarySensor->connectLink(*linkConf);
    _sensors[Connected][objIndex] = true;
}

void DeviceManager::connectLinkDirectly(AbstractLinkNamespace::LinkType connType, const QStringList& connString)
{
    auto linkConfiguration = LinkConfiguration{connType, connString};

    // Append configuration as device of type "None"
    // This will create and populate all necessary roles before connecting
    append(linkConfiguration);
    connectLink(&linkConfiguration);
}

void DeviceManager::updateAvailableConnections(const QVector<LinkConfiguration>& availableLinkConfigurations)
{
    // Make all connections unavailable by default
    for(int i{0}; i < _sensors[Available].size(); i++) {
        auto linkConf = _sensors[Connection][i].value<QSharedPointer<LinkConfiguration>>();
        if(linkConf->type() == AbstractLinkNamespace::PingSimulation) {
            continue;
        }
        _sensors[Available][i] = false;
        const auto indexRow = index(i);
        emit dataChanged(indexRow, indexRow, _roles);
    }

    // Right now we only support Ping1D, we should update protocol detector to detect device type
    for(const auto& link : availableLinkConfigurations) {
        append(link, "Ping1D");
    }
}

QObject* DeviceManager::qmlSingletonRegister(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    return self();
}

DeviceManager* DeviceManager::self()
{
    static DeviceManager* self = new DeviceManager();
    return self;
}

DeviceManager::~DeviceManager()
{
    stopDetecting();
}
