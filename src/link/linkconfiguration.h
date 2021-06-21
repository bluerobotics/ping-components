#pragma once

#include <QDebug>
#include <QMap>
#include <QString>
#include <QStringList>

#include "abstractlinknamespace.h"
#include "ping-message-all.h"

/**
 * @brief Link configuration class
 *
 */
class LinkConfiguration : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Link configuration structure
     *
     */
    struct LinkConf {
        QStringList args;
        QString name;
        LinkType type = LinkType::None;
        // TODO: We should somehow make this class or this structure more abstract
        // and remove any protocol or device specific information
        // right now this is necessary, since link configuration is our default
        // link information structure
        PingDeviceType deviceType = PingDeviceType::UNKNOWN;
    };

    /**
     * @brief Link configuration errors
     *
     */
    enum Error : int {
        MissingConfiguration = -1, // This can be used in future for warnings and not real errors
        NoErrors = 0,
        NoType,
        InvalidType,
        NoArgs,
        InvalidArgsNumber,
        ArgsAreEmpty,
        InvalidUrl,
        InvalidSubnet,
    };
    Q_ENUM(Error)

    /**
     * @brief Construct a new Link Configuration object
     *
     * @param linkType
     * @param args
     * @param name
     * @param deviceType
     */
    LinkConfiguration(LinkType linkType = LinkType::None, const QStringList& args = QStringList(),
        const QString& name = QString(), PingDeviceType deviceType = PingDeviceType::UNKNOWN)
        : _linkConf {args, name, linkType, deviceType} {};

    /**
     * @brief Construct a new Link Configuration object
     *
     * @param confLinkStructure
     */
    LinkConfiguration(LinkConf& confLinkStructure)
        : _linkConf {confLinkStructure} {};

    /**
     * @brief Construct a new Link Configuration object
     *
     * @param other
     */
    LinkConfiguration(const LinkConfiguration& other)
        : QObject(nullptr)
        , _linkConf {other.configurationStruct()} {};

    /**
     * @brief Construct a new Link Configuration object from string configuration
     *
     * @param configurationString
     */
    LinkConfiguration(const QString& configurationString);

    /**
     * @brief Destroy the Link Configuration object
     *
     */
    ~LinkConfiguration() = default;

    /**
     * @brief Return a list with the arguments
     *
     * @return const QStringList*
     */
    const QStringList* args() const { return &_linkConf.args; };

    /**
     * @brief Return list as copy
     *
     * @return Q_INVOKABLE argsAsConst
     */
    Q_INVOKABLE QStringList argsAsConst() const { return _linkConf.args; };

    /**
     * @brief Set link configuration arguments
     *
     * @param args
     */
    void setArgs(const QStringList& args);

    /**
     * @brief Return PingDeviceType enumeration for device specific identification
     *
     * @return PingDeviceType
     */
    Q_INVOKABLE PingEnumNamespace::PingDeviceType deviceType() const { return _linkConf.deviceType; };

    /**
     * @brief Set the Device Type object
     *
     * @param deviceType
     */
    void setDeviceType(const PingDeviceType deviceType) { _linkConf.deviceType = deviceType; };

    /**
     * @brief Check if type is the one in the configuration
     *
     * @param type
     * @return true
     * @return false
     */
    bool checkType(LinkType type) const { return _linkConf.type == type; };

    /**
     * @brief Return configuration as structure
     *
     * @return LinkConf
     */
    LinkConf configurationStruct() const { return _linkConf; };

    /**
     * @brief Return a pointer for the configuration structure
     *
     * @return const LinkConf*
     */
    const LinkConf* configurationStructPtr() const { return &_linkConf; };

    /**
     * @brief Create and return a configuration string
     *
     * @return const QString
     */
    Q_INVOKABLE const QString createConfString() const { return _linkConf.args.join(":"); };

    /**
     * @brief Create and return a configuration in string list format
     *
     * @return const QStringList
     */
    const QStringList createConfStringList() const { return _linkConf.args; };

    /**
     * @brief Create and return old style configuration link
     *
     * @return const QString
     */
    const QString createFullConfString() const;

    /**
     * @brief Create a full configuration in string list format
     *
     * @return const QStringList
     */
    const QStringList createFullConfStringList() const;

    /**
     * @brief Return error numbers
     *
     * @return Error
     */
    Q_INVOKABLE LinkConfiguration::Error error() const;

    /**
     * @brief Convert error number in a friendly human message
     *
     * @param error
     * @return QString
     */
    static QString errorToString(Error error) { return _errorMap[error]; }

    /**
     * @brief Return error in a friendly human message
     *
     * @return QString
     */
    Q_INVOKABLE QString errorToString() const { return errorToString(error()); }

    /**
     * @brief Check if configuration is correct
     *
     * @return bool
     */
    Q_INVOKABLE bool isValid() const { return error() <= NoErrors; }

    /**
     * @brief Return configuration name
     *
     * @return Q_INVOKABLE name
     */
    Q_INVOKABLE QString name() const { return _linkConf.name; };

    /**
     * @brief Set configuration name
     *
     * @param name
     */
    void setName(const QString& name) { _linkConf.name = name; };

    /**
     * @brief Return serialport system path
     *
     * @return QString
     */
    QString serialPort() const;

    /**
     * @brief Return serial baudrate
     *
     * @return int
     */
    Q_INVOKABLE int serialBaudrate() const;

    /**
     * @brief Set the Type object
     *
     * @param type
     */
    void setType(const LinkType& type) { _linkConf.type = type; };

    /**
     * @brief Check if IP is subnet
     *
     * @return true
     * @return false if IP is not in the same subnet as the computer or is not an ethernet connection
     */
    bool isInSubnet() const;

    /**
     * @brief Return link configuration type
     *
     * @return AbstractLinkNamespace::LinkType
     */
    Q_INVOKABLE AbstractLinkNamespace::LinkType type() const { return _linkConf.type; };

    /**
     * @brief Check if it's a simulation
     *
     * @return bool
     */
    Q_INVOKABLE bool isSimulation() const
    {
        return _linkConf.type == AbstractLinkNamespace::Ping1DSimulation
            || _linkConf.type == AbstractLinkNamespace::Ping360Simulation;
    };

    /**
     * @brief Return the type in a human readable format
     *  TODO: move this function to somewhere else
     * @return QString
     */
    Q_INVOKABLE QString typeToString() const
    {
        switch (_linkConf.type) {
        case LinkType::None:
            return QStringLiteral("None");
        case LinkType::File:
            return QStringLiteral("File");
        case LinkType::Serial:
            return QStringLiteral("Serial");
        case LinkType::Udp:
            return QStringLiteral("UDP");
        case LinkType::Ping1DSimulation:
            return QStringLiteral("Ping1D Simulation");
        case LinkType::Ping360Simulation:
            return QStringLiteral("Ping360 Simulation");
        default:
            return QStringLiteral("Unknown");
        }
    };

    /**
     * @brief Check if UDP port is a valid companion port
     *
     * @return bool
     */
    Q_INVOKABLE bool isCompanionPort() const;

    /**
     * @brief Will return argument with UDP host name
     *
     * @return QString
     */
    QString udpHost() const;

    /**
     * @brief Will return port used in UDP connection
     *
     * @return int
     */
    int udpPort() const;

    /**
     * @brief Copy operator
     *
     * @param other
     * @return LinkConfiguration&
     */
    LinkConfiguration& operator=(const LinkConfiguration& other)
    {
        this->_linkConf = other.configurationStruct();
        emit configurationChanged();
        return *this;
    }

    Q_PROPERTY(QString string READ createFullConfString NOTIFY configurationChanged)

signals:
    void configurationChanged();

private:
    static const QVector<int> _companionPorts;
    static const QMap<Error, QString> _errorMap;
    LinkConf _linkConf;
};

bool operator==(const LinkConfiguration& first, const LinkConfiguration& second);
bool operator!=(const LinkConfiguration& first, const LinkConfiguration& second);
QDebug operator<<(QDebug d, const LinkConfiguration& other);
QDataStream& operator<<(QDataStream& out, const LinkConfiguration linkConfiguration);
QDataStream& operator>>(QDataStream& in, LinkConfiguration& linkConfiguration);

Q_DECLARE_METATYPE(LinkConfiguration)

/**
 * @brief This allows us to register LinkConfiguration before singletons and main function
 * This is something like Q_DECLARE_METATYPE but for operators
 * We should create a Q_DECLARE_METATYPE_STREAM_OPERATORS defined function to deal with future cases
 * For any further explanation:
 * http://doc.qt.io/qt-5/qmetatype.html#Q_DECLARE_METATYPE
 * https://code.woboq.org/qt5/qtbase/src/corelib/kernel/qmetatype.h.html#_M/Q_DECLARE_METATYPE
 * https://code.woboq.org/qt5/qtbase/src/corelib/kernel/qmetatype.h.html#_Z32qRegisterMetaTypeStreamOperatorsPKcPT_
 */
struct LinkConfigurationRegisterStruct {
    LinkConfigurationRegisterStruct()
    {
        static QBasicAtomicInt metatypeId = Q_BASIC_ATOMIC_INITIALIZER(0);
        if (const int id = metatypeId.loadAcquire()) {
            return;
        }

        const int newId = qRegisterMetaType<LinkConfiguration>("LinkConfiguration");
        metatypeId.storeRelease(newId);
        qRegisterMetaTypeStreamOperators<LinkConfiguration>("LinkConfiguration");
    }
};

namespace {
LinkConfigurationRegisterStruct _linkConfigurationRegisterStruct;
}
