#pragma once

#include <functional>

#include <QProcess>
#include <QSharedPointer>
#include <QTimer>

#include "parsers/parser.h"
#include "parsers/parser_ping.h"
#include "pingmessage/ping_ping360.h"
#include "protocoldetector.h"
#include "pingsensor.h"

/**
 * @brief Define Ping360 sensor
 * Ping 360 Sonar
 *
 */
class Ping360 : public PingSensor
{
    Q_OBJECT
public:

    Ping360();
    ~Ping360();

    /**
     * @brief Add new connection
     *
     * @param connType connection type
     * @param connString arguments for the new connection
     */
    Q_INVOKABLE void connectLink(AbstractLinkNamespace::LinkType connType, const QStringList& connString);

    /**
     * @brief debug function
     */
    void printSensorInformation() const override final;

    /**
     * @brief Return number of pings emitted
     *
     * @return uint32_t
     */
    uint32_t ping_number() { return _ping_number; }
    Q_PROPERTY(int ping_number READ ping_number NOTIFY pingNumberUpdate)

    /**
     * @brief Return pulse emission in ms
     *
     * @return uint16_t
     */
    uint16_t transmit_duration() { return _transmit_duration; }
    Q_PROPERTY(int transmit_duration READ transmit_duration NOTIFY transmitDurationUpdate)

    /**
     * @brief Return start distance of sonar points in mm
     *
     * @return uint32_t
     */
    uint32_t start_mm() { return _scan_start; }

    /**
     * @brief Set sensor start analyze distance in mm
     *
     * @param start_mm
     */
    void set_start_mm(int start_mm)
    {
        Q_UNUSED(start_mm)
    }

    //TODO: update this signal name and others to Changed
    Q_PROPERTY(int start_mm READ start_mm WRITE set_start_mm NOTIFY scanStartUpdate)

    /**
     * @brief return points length in mm
     *
     * @return uint32_t
     */
    uint32_t length_mm() { return _scan_length; }

    /**
     * @brief Set sensor window analysis size
     *
     * @param length_mm
     */
    void set_length_mm(int length_mm)
    {
        Q_UNUSED(length_mm)
    }
    Q_PROPERTY(int length_mm READ length_mm WRITE set_length_mm NOTIFY scanLengthUpdate)

    /**
     * @brief Return gain setting
     *
     * @return uint32_t
     */
    uint32_t gain_setting() { return _gain_setting; }

    /**
     * @brief Set sensor gain index
     *
     * @param gain_setting
     */
    void set_gain_setting(int gain_setting)
    {
        Q_UNUSED(gain_setting)
    }
    Q_PROPERTY(int gain_setting READ gain_setting WRITE set_gain_setting NOTIFY gainSettingUpdate)

    /**
     * @brief Return last array of points
     *
     * @return QVector<double>
     */
    QVector<double> data() { return _data; }
    Q_PROPERTY(QVector<double> data READ data NOTIFY dataChanged)

    /**
     * @brief Get time between pings in ms
     *
     * @return uint16_t
     */
    uint16_t ping_interval() { return _ping_interval; }

    /**
     * @brief Set time between pings in ms
     *
     * @param ping_interval
     */
    void set_ping_interval(uint16_t ping_interval)
    {
        Q_UNUSED(ping_interval)
    }
    Q_PROPERTY(int ping_interval READ ping_interval WRITE set_ping_interval NOTIFY pingIntervalUpdate)

    /**
     * @brief Get the speed of sound (mm/s) used for calculating the distance from time-of-flight
     *
     * @return uint32_t
     */
    uint32_t speed_of_sound() { return _speed_of_sound; }

    /**
     * @brief Set speed of sound (mm/s) used for calculating distance from time-of-flight
     *
     * @param speed_of_sound
     */
    void set_speed_of_sound(uint32_t speed_of_sound)
    {
        Q_UNUSED(speed_of_sound)
    }
    Q_PROPERTY(int speed_of_sound READ speed_of_sound WRITE set_speed_of_sound NOTIFY speedOfSoundUpdate)

    /**
     * @brief Return ping frequency
     *
     * @return float
     */
    float pingFrequency() { return _ping_interval ? static_cast<int>(1000/_ping_interval) : 0; };

    /**
     * @brief Set ping frequency
     *
     * @param pingFrequency
     */
    void setPingFrequency(float pingFrequency);
    Q_PROPERTY(float pingFrequency READ pingFrequency WRITE setPingFrequency NOTIFY pingIntervalUpdate)

    /**
     * @brief Return the max frequency that the sensor can work
     *
     * @return int
     */
    int pingMaxFrequency() { return _pingMaxFrequency; }
    Q_PROPERTY(int pingMaxFrequency READ pingMaxFrequency CONSTANT)

    /**
     * @brief Do firmware sensor update
     *
     * @param fileUrl firmware file path
     * @param sendPingGotoBootloader Use "goto bootloader" message
     * @param baud baud rate value
     * @param verify this variable is true when all
     */
    Q_INVOKABLE void firmwareUpdate(QString fileUrl, bool sendPingGotoBootloader = true, int baud = 57600,
                                    bool verify = true);

    /**
     * @brief Return the visualization widget
     *
     * @param parent
     * @return QQmlComponent* sensorVisualizer
     */
    Q_INVOKABLE QQmlComponent* sensorVisualizer(QObject *parent) final;

signals:
    /**
     * @brief emitted when propriety changes
     */
///@{
    void dataChanged();
    void gainSettingUpdate();
    void pingIntervalUpdate();
    void pingNumberUpdate();
    void scanLengthUpdate();
    void scanStartUpdate();
    void speedOfSoundUpdate();
    void transmitDurationUpdate();
///@}

private:
    Q_DISABLE_COPY(Ping360)
    /**
     * @brief Sensor variables
     */
///@{
    uint16_t _transmit_duration = 0;
    uint32_t _ping_number = 0;
    uint32_t _scan_start = 0;
    uint32_t _scan_length = 0;
    uint32_t _gain_setting = 0;
    uint32_t _speed_of_sound = 0;
///@}

    uint16_t _num_points = 2000;

    QVector<double> _data;

    uint16_t _ping_interval = 0;
    static const int _pingMaxFrequency;

    void handleMessage(const ping_message& msg) final; // handle incoming message

    void loadLastSensorConfigurationSettings();
    void updateSensorConfigurationSettings();
    void setLastSensorConfiguration();

    /**
     * @brief Internal function used to use as a flash callback
     *
     * @param fileUrl Firmware file path
     * @param sendPingGotoBootloader Use "goto bootloader" message
     * @param baud baud rate value
     * @param verify this variable is true when all
     */
    void flash(const QString& fileUrl, bool sendPingGotoBootloader, int baud, bool verify);

    /**
     * @brief Reset sensor local variables
     *  TODO: This variables should be moved to a structure
     *
     */
    void resetSensorLocalVariables();

    /**
     * @brief This saves the last configuration ID
     *  This value need to be set as an invalid one (-1)
     *  To allow sensor reconfiguration.
     *
     *  This should be removed after creating a PingSensor class that does not deal with connections and
     *  Firmware update.
     *  Check: https://github.com/bluerobotics/ping-viewer/issues/406
     */
    int _lastPingConfigurationSrcId = -1;

    /**
     * @brief Start the pre configuration process of the sensor
     *
     */
    void startPreConfigurationProcess();

    /**
     * @brief Take care of github payload and detect new versions available
     *
     * @param jsonDocument
     */
    void checkNewFirmwareInGitHubPayload(const QJsonDocument& jsonDocument);
};
