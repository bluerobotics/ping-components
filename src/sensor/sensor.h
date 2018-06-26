#pragma once

#include "link.h"
#include "parsers/parser.h"

// TODO rename to Device?
/**
 * @brief Manage sensor connection
 *
 */
class Sensor : public QObject
{
    Q_OBJECT
public:
    Sensor();
    ~Sensor();

    /**
     * @brief Return entry link
     *
     * @return AbstractLink*
     */
    AbstractLink* link() { return _linkIn ? _linkIn->self() : nullptr; };
    Q_PROPERTY(AbstractLink* link READ link NOTIFY linkUpdate)

    /**
     * @brief Return log link
     *
     * @return AbstractLink*
     */
    AbstractLink* linkLog() { return _linkOut ? _linkOut->self() : nullptr; };
    Q_PROPERTY(AbstractLink* linkLog READ linkLog NOTIFY linkLogUpdate)

    /**
     * @brief Return sensor name
     *
     * @return QString
     */
    QString name() { return _name; };
    Q_PROPERTY(QString name READ name NOTIFY nameUpdate)

    /**
     * @brief Add input link
     *
     * @param connString Connection string defined as (int:string:arg)
     * @param logConnString Log connection string defined as (int:string:arg)
     */
    Q_INVOKABLE void connectLink(QStringList connString, const QStringList& logConnString = QStringList());

    /**
     * @brief Add log link
     *
     * @param connString Log connection string defined as (int:string:arg)
     * @return Q_INVOKABLE connectLinkLog
     */
    Q_INVOKABLE void connectLinkLog(QStringList connString);

    /**
     * @brief Set auto detect
     *
     * @param autodetect
     */
    void setAutoDetect(bool autodetect);
    /**
     * @brief Check auto detect state
     *
     * @return true running
     * @return false not running
     */
    bool autoDetect() { return _autodetect; };
    Q_PROPERTY(bool autodetect READ autoDetect WRITE setAutoDetect NOTIFY autoDetectUpdate)

protected:
    bool _autodetect;

    Link* _linkIn;
    Link* _linkOut;
    Parser* _parser; // communication implementation

    QString _name; // TODO populate

signals:
    void autoDetectUpdate(bool autodetect);

    // In
    void connectionClose();
    void connectionOpen();
    void nameUpdate();
    void linkUpdate();

    // Out
    void linkLogUpdate();
};
