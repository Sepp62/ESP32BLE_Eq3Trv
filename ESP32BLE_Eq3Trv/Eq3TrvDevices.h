#pragma once

#include <NimBLEDevice.h>

typedef struct Eq3TrvStatus
{
    Eq3TrvStatus() {
        fTemp = fOffsetTemp = 0.0;
        nValve = 0;
        bValidData = bAutoMode = bLocked = bHoliday = bBoost = bDst = bWindowOpen = bLowBattery = false;
    }
    std::string address;
    bool  bValidData;

    float fTemp;
    float fOffsetTemp;
    int   nValve;
    bool  bAutoMode;
    bool  bLocked;
    bool  bHoliday;
    bool  bDst;
    bool  bBoost;
    bool  bWindowOpen;
    bool  bLowBattery;
}
Eq3TrvStatus_t;

typedef enum 
{
    CMD_NORESULT = 0,
    CMD_SUCCESS  = 1,
    CMD_ERROR    = 2,
}
CmdStatus_t;

class Eq3TrvData
{
public:
    Eq3TrvData( std::string address )  { _status.address = address; }
    Eq3TrvStatus_t _status;
protected:
};

class Eq3TrvDevices
{
public:
    Eq3TrvDevices();

    // initialization
    void SetNotifyCallback( notify_callback notifyCB );
    
    // bluetooth event functions
    void OnBtDeviceDiscovered( NimBLEAdvertisedDevice* advertisedDevice );
    void OnBtNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

    // scan interval handling
    void TriggerScanInterval();
    bool IsScanIntervalExpired();

    // device list handling
    Eq3TrvData* FindTrvData( std::string address );
    int  GetDeviceList (std::vector<std::string>& addresses);

    // get status/data of last device, which executed a command
    bool GetLastStatusResult(Eq3TrvStatus_t& status);

    // for async operation
    CmdStatus_t CheckCmdCompletionResult();              // check whether command is successfully finished
    bool IsIdle();                                       // check if code is ready for next command

    // EQ3 trv commands
    bool SelectTemp(std::string address, float temp, bool bSync = true);
    bool ModeAuto(std::string address, bool bAuto, bool bSync = true);
    bool SetTemps(std::string address, float tempComfort, float tempReduced, bool bSync = true);
    bool SelectComfortTemp(std::string address, bool bSync = true);
    bool SelectReducedTemp(std::string address, bool bSync = true);
    bool SetTime(std::string address, int day, int month, int year, int hour, int minute, int second, bool bSync = true );
    bool Boost(std::string address, bool bOn, bool bSync = true);
    bool SelectHolidayMode(std::string address, float temp, int endHour, int endMinute, int endDay, int endMonth, int endYear, bool bSync = true);
    bool Lock(std::string address, bool bOn, bool bSync = true);
    bool Offset(std::string address, float offTemp, bool bSync = true);
    bool On( std::string address, bool bSync = true);
    bool Off( std::string address, bool bSync = true);
    bool SetOpenWindowParams(std::string address, float temp, int minutes, bool bSync = true);

protected:
    const uint32_t CMD_TIMEOUT = 5000;     // wait time (ms) for notification in synchronous mode
    const uint32_t SCAN_INTERVAL = 60000;  // scan cycle 1 minute

    // device status address to data struct lookup
    typedef std::map<std::string, Eq3TrvData*> Eq3DataMap;
    Eq3DataMap _lookupMap;
    
    // add a discovered device to the lookup map
    void addNewTrvData(std::string address); 

    // timer to start scanning 
    uint32_t                    _tiNextScanTime;

    // command data
    uint32_t                    _tiLastCommandTimeout;
    NimBLEClient              * _pActiveClient;
    NimBLERemoteCharacteristic* _pCommandChar;
    NimBLERemoteCharacteristic* _pNotifyChar;
    Eq3TrvData                * _pNotifyData; // last notified data

    // BT callback events
    notify_callback _notifyCB;

    bool connect( std::string address );
    void disconnect();
    bool issueCommand(std::string address, uint8_t command[], int len, bool bSync );
    bool waitCompletion();
    void notifyError( std::string address );
};

