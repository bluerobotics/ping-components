#include <QUrl>

#include "linkconfiguration.h"
#include "networkmanager.h"

const QVector<int> LinkConfiguration::_companionPorts {9090, 9092};

const QMap<LinkConfiguration::Error, QString> LinkConfiguration::_errorMap {
    {NoErrors, "No link configuration errors."},
    {MissingConfiguration, "Warning: Link configuration is missing."},
    {NoType, "Link configuration Type is None."},
    {InvalidType, "Link configuration Type is invalid."},
    {NoArgs, "Link configuration does not have arguments."},
    {InvalidArgsNumber, "Link configuration have a invalid number of arguments"},
    {ArgsAreEmpty, "Link configuration arguments are empty."},
    {InvalidUrl, "Url not formatted properly."},
    {InvalidSubnet, "IP is not in a reachable subnet. Configure your computer network settings."},
};

LinkConfiguration::LinkConfiguration(const QString& configurationString)
    : QObject(nullptr)
{
    auto configurationStringList = configurationString.split(":");

    auto deviceType = configurationStringList.first();
    if (deviceType == PingHelper::nameFromDeviceType(PingDeviceType::PING1D)) {
        _linkConf.deviceType = PingDeviceType::PING1D;
    } else if (deviceType == PingHelper::nameFromDeviceType(PingDeviceType::PING360)) {
        _linkConf.deviceType = PingDeviceType::PING360;
    }
    if (_linkConf.deviceType != PingDeviceType::UNKNOWN) {
        configurationStringList.removeFirst();
    }

    _linkConf.type = static_cast<LinkType>(configurationStringList.first().toInt());
    configurationStringList.removeFirst();
    _linkConf.args = configurationStringList;
}

const QString LinkConfiguration::createFullConfString() const
{
    QStringList args {_linkConf.args};
    args.prepend(QString::number(_linkConf.type));
    return args.join(":");
}

const QStringList LinkConfiguration::createFullConfStringList() const
{
    QStringList args {_linkConf.args};
    args.prepend(QString::number(_linkConf.type));
    return args;
}

LinkConfiguration::Error LinkConfiguration::error() const
{
    // No type, no valid connection
    if (_linkConf.type == LinkType::None) {
        return NoType;
    }

    // Type is not declared in configuration link type
    if (_linkConf.type < LinkType::None || _linkConf.type >= LinkType::Last) {
        return InvalidType;
    }

    // Simulation does not need args
    // TODO: rework this check, check linkconfiguration capabilities or add new ones for this case
    if (isSimulation() && !_linkConf.args.length()) {
        return NoErrors;
    }

    // Everything else does need args
    if (!_linkConf.args.length()) {
        return NoArgs;
    }

    // Usually connections are made with path:format/conf
    if (_linkConf.args.length() != 2) {
        return InvalidArgsNumber;
    }

    // Args must be no empty strings
    for (const auto& arg : _linkConf.args) {
        if (arg.isEmpty()) {
            return InvalidArgsNumber;
        }
    }

    // Check for issues in UDP configuration
    if (_linkConf.type == LinkType::Udp) {
        // If is not in the same subnet
        if (!isInSubnet()) {
            return InvalidSubnet;
        }

        // If the address is invalid
        if (!QUrl(_linkConf.args[0]).isValid()) {
            return InvalidUrl;
        }
    }

    // Name is not necessary to do a connection
    if (_linkConf.name.isEmpty()) {
        return MissingConfiguration;
    }

    return NoErrors;
}

void LinkConfiguration::setArgs(const QStringList& args)
{
    _linkConf.args = args;
    emit configurationChanged();
}

QString LinkConfiguration::serialPort() const
{
    if (!checkType(LinkType::Serial) || !_linkConf.args.size()) {
        return QString();
    }
    return _linkConf.args[0];
}

int LinkConfiguration::serialBaudrate() const
{
    if (!checkType(LinkType::Serial) || _linkConf.args.size() < 1) {
        return 0;
    }

    return _linkConf.args[1].toInt();
}

bool LinkConfiguration::isCompanionPort() const
{
    if (!isValid() || !checkType(LinkType::Udp)) {
        return false;
    }

    return _companionPorts.contains(udpPort());
}

QString LinkConfiguration::udpHost() const
{
    if (!checkType(LinkType::Udp) || !_linkConf.args.size()) {
        return QString();
    }

    return _linkConf.args[0];
}

int LinkConfiguration::udpPort() const
{
    if (!checkType(LinkType::Udp) || _linkConf.args.size() < 1) {
        return 0;
    }

    return _linkConf.args[1].toInt();
}

bool LinkConfiguration::isInSubnet() const { return NetworkManager::isIpInSubnet(udpHost()); }

bool operator==(const LinkConfiguration& first, const LinkConfiguration& second)
{
    auto firstLinkconf = first.configurationStruct();
    auto secondLinkconf = second.configurationStruct();
    return (firstLinkconf.type == secondLinkconf.type) && (firstLinkconf.args == secondLinkconf.args)
        && (firstLinkconf.deviceType == secondLinkconf.deviceType);
}

bool operator!=(const LinkConfiguration& first, const LinkConfiguration& second) { return !(first == second); }

QDebug operator<<(QDebug d, const LinkConfiguration& other)
{
    QString text(QStringLiteral("LinkConfiguration{Name: %1, Sensor: %2, LinkType: %3, Arguments: (%4)}"));
    d << text.arg(other.name(), PingHelper::nameFromDeviceType(other.deviceType()), other.typeToString(),
        other.args()->join(":"));
    return d;
}

QDataStream& operator<<(QDataStream& out, const LinkConfiguration linkConfiguration)
{
    out << linkConfiguration.configurationStructPtr()->name;
    out << QVariant(linkConfiguration.configurationStructPtr()->args);
    out << QVariant(linkConfiguration.configurationStructPtr()->type);
    out << QVariant(static_cast<int>(linkConfiguration.configurationStructPtr()->deviceType));
    return out;
}

QDataStream& operator>>(QDataStream& in, LinkConfiguration& linkConfiguration)
{
    QString name;
    QVariant variantArgs, variantType, variantDeviceType;
    in >> name;
    in >> variantArgs;
    in >> variantType;
    in >> variantDeviceType;
    linkConfiguration = LinkConfiguration(variantType.value<LinkType>(), variantArgs.toStringList(), name,
        static_cast<PingDeviceType>(variantDeviceType.toInt()));
    emit linkConfiguration.configurationChanged();
    return in;
}
