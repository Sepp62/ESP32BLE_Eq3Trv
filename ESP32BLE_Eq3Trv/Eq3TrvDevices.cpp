#include "Eq3TrvDevices.h"

// debug output
constexpr auto SER_DEBUG = 0; // set = 1 for serial debug output
#define debug_printf(...) \
            do { if (SER_DEBUG) Serial.printf( __VA_ARGS__); } while (0)

// BLE UUIDs
static NimBLEUUID EQ3_SERVICE_UUID("3e135142-654f-9090-134a-a6ff5bb77046");
static NimBLEUUID EQ3_COMMAND_CHAR_UUID("3fa4585a-ce4a-3bad-db4b-b8df8179ea09");
static NimBLEUUID EQ3_NOTIFY_CHAR_UUID("d0e8434d-cd29-0996-af41-6c90f4e0eb2a");

Eq3TrvDevices::Eq3TrvDevices() : 
    _tiNextScanTime( 0 ),
    _tiLastCommandTimeout( 0 ),
    _pActiveClient(nullptr),
    _pCommandChar(nullptr),
    _pNotifyChar(nullptr),
    _pNotifyData(nullptr)
{
}

// for asynchronous operation: call periodically from loop()
// otherwise set sync flag in command calls for "blocking wait"
CmdStatus_t Eq3TrvDevices::CheckCmdCompletionResult()
{
    // check timeout
    if (_pActiveClient != nullptr && _pNotifyData == nullptr )  // command is running, but no result yet
    {
        if (millis() > _tiLastCommandTimeout)
        {
            debug_printf( "command timeout\n" );
            std::string address = _pActiveClient->getPeerAddress().toString();
            disconnect();
            notifyError( address );
            return CMD_ERROR;
        }
    }

    if( _pActiveClient != nullptr && _pNotifyData != nullptr)  // regular completion due to data indication
    {
        debug_printf("command completed\n");
        disconnect();
        return CMD_SUCCESS;
    }

    return CMD_NORESULT;
}

// make sure that there is no running command, before calling the next command
bool Eq3TrvDevices::IsIdle()
{
    return _pActiveClient == nullptr;
}

// call it from "scan end notification" or after disconnecting a device
void Eq3TrvDevices::TriggerScanInterval()
{
    _tiNextScanTime = millis() + SCAN_INTERVAL;
}

bool Eq3TrvDevices::IsScanIntervalExpired()
{
    return millis() > _tiNextScanTime;
}

// get status data from last notification
bool Eq3TrvDevices::GetLastStatusResult(Eq3TrvStatus_t& status)
{
    if (_pNotifyData)
    {
        status = _pNotifyData->_status;
        _pNotifyData = nullptr;
        return true;
    }
    return false;
}

// list of all discovered devices since startup
int Eq3TrvDevices::GetDeviceList(std::vector<std::string>& addresses)
{
    int n = 0;
    for( Eq3DataMap::const_iterator it = _lookupMap.begin(); it != _lookupMap.end(); ++it )
        addresses.push_back( it->first );

    return addresses.size();
}

// EQ3 TRV commands...
bool Eq3TrvDevices::SelectTemp(std::string address, float temp, bool bSync )
{
    uint8_t command[] = { 0x41, (uint8_t)(temp * 2) };
    debug_printf("Update temperature to %2.1f\n", temp);
    return issueCommand(address, command, 2, bSync );
}

bool Eq3TrvDevices::SetTemps(std::string address, float tempComfort, float tempReduced, bool bSync )
{
    uint8_t command[] = { 0x11, (uint8_t)(tempComfort * 2), (uint8_t)(tempReduced * 2) };
    debug_printf("Update temps comfort %2.1f, reduced %2.1f\n", tempComfort, tempReduced);
    return issueCommand(address, command, 3, bSync);
}

bool Eq3TrvDevices::SelectComfortTemp(std::string address, bool bSync )
{
    uint8_t command[] = { 0x43 };
    debug_printf("Update select comfort temp\n");
    return issueCommand(address, command, 1, bSync);
}

bool Eq3TrvDevices::SelectReducedTemp(std::string address, bool bSync )
{
    uint8_t command[] = { 0x44 };
    debug_printf("Update select reduced temp\n");
    return issueCommand(address, command, 1, bSync);
}

bool Eq3TrvDevices::SetTime(std::string address, int day, int month, int year, int hour, int minute, int second, bool bSync)
{
    uint8_t command[] = { 0x03, (uint8_t)(year % 100), (uint8_t)month, (uint8_t)day, (uint8_t)hour, (uint8_t)minute, (uint8_t)second };
    debug_printf("Update set time\n");
    return issueCommand(address, command, 7, bSync);
}

bool Eq3TrvDevices::Boost(std::string address, bool bOn, bool bSync)
{
    uint8_t command[] = { 0x45, (uint8_t)bOn };
    debug_printf("Update boost to %d\n", bOn );
    return issueCommand(address, command, 2, bSync);
}

bool Eq3TrvDevices::SelectHolidayMode(std::string address, float temp, int endHour, int endMinute, int endDay, int endMonth, int endYear, bool bSync)
{
    uint8_t command[] = { 0x40, ((uint8_t)(temp * 2)) | 0x80, (uint8_t)endDay, (uint8_t)(endYear % 100), (uint8_t)(endHour*2 + endMinute/30), (uint8_t)endMonth };
    debug_printf("Update holiday mode\n" );
    return issueCommand(address, command, 6, bSync);
}

bool Eq3TrvDevices::Lock(std::string address, bool bOn, bool bSync)
{
    uint8_t command[] = { 0x80, (uint8_t)bOn };
    debug_printf("Update lock to %d\n", bOn);
    return issueCommand(address, command, 2, bSync);
}

bool Eq3TrvDevices::Offset(std::string address, float offTemp, bool bSync)
{
    uint8_t command[] = { 0x13, (uint8_t)((offTemp * 2) + 7) };
    debug_printf("Update offset temperature to %2.1f\n", offTemp);
    return issueCommand(address, command, 2, bSync);
}

bool Eq3TrvDevices::On(std::string address, bool bSync)
{
    return SelectTemp(address, 30.0f, bSync);
}

bool Eq3TrvDevices::Off(std::string address, bool bSync)
{
    return SelectTemp( address, 4.5f, bSync);
}

bool Eq3TrvDevices::SetOpenWindowParams(std::string address, float temp, int minutes, bool bSync)
{
    uint8_t command[] = { 0x14, (uint8_t)(temp * 2), (uint8_t)(minutes / 5) };
    debug_printf("Update open windpw params\n" );
    return issueCommand(address, command, 3, bSync);
}

bool Eq3TrvDevices::ModeAuto(std::string address, bool bAuto, bool bSync)
{
    uint8_t command[] = { 0x40, bAuto ? 0x00 : 0x40 };
    debug_printf("Update mode to auto: %d\n", bAuto);
    return issueCommand( address, command, 2, bSync );
}

// class initialization - set notification callback function
void Eq3TrvDevices::SetNotifyCallback( notify_callback notifyCB )
{
    _notifyCB  = notifyCB;
}

// called from advertising callback
void Eq3TrvDevices::OnBtDeviceDiscovered(NimBLEAdvertisedDevice* advertisedDevice)
{
    NimBLEUUID uid = advertisedDevice->getServiceUUID();
    if (uid == EQ3_SERVICE_UUID)
    {
        std::string address = advertisedDevice->getAddress().toString();
        debug_printf("EQ3 service found, addr: %s\n", address.c_str());
        addNewTrvData( address );
    }
}

// notification callback
void Eq3TrvDevices::OnBtNotify( NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify )
{
    // std::string rs(pRemoteCharacteristic->getRemoteService()->getUUID());

    if( pRemoteCharacteristic->getUUID() != EQ3_NOTIFY_CHAR_UUID )
    {
        debug_printf("notification for another device\n");
        return;
    }

    Eq3TrvData* pTrvData = FindTrvData( pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress() );
    if( pTrvData == nullptr )
    {
        debug_printf("eq3trv data not found, unknown address\n");
        return;
    }

    // handle a status notifications only
    if (!(pData[0] == 2 && pData[1] == 1))
        return;
    
    pTrvData->_status.bValidData  = true; // mark data as valid
    pTrvData->_status.nValve      = (uint8_t)pData[3];
    pTrvData->_status.fTemp       = ((float)pData[5]) / 2;
    pTrvData->_status.bAutoMode   = (pData[2] & 1)   ? false : true;
    pTrvData->_status.bHoliday    = (pData[2] & 2)   ? true  : false;
    pTrvData->_status.bBoost      = (pData[2] & 4)   ? true  : false;
    pTrvData->_status.bDst        = (pData[2] & 8)   ? true  : false;
    pTrvData->_status.bWindowOpen = (pData[2] & 16 ) ? true  : false;
    pTrvData->_status.bLocked     = (pData[2] & 32 ) ? true  : false;
    pTrvData->_status.bLowBattery = (pData[2] & 128) ? true  : false;

    // signal new data
    debug_printf("notification got!\n");
    _pNotifyData = pTrvData; // remember last result
}

Eq3TrvData* Eq3TrvDevices::FindTrvData(std::string address)
{
    Eq3DataMap::iterator mi = _lookupMap.find( address );

    if( mi != _lookupMap.end() )
        return mi->second;

    return nullptr;
}

void Eq3TrvDevices::addNewTrvData(std::string address)
{
    Eq3DataMap::iterator mi = _lookupMap.find(address);

    // already there
    if (mi != _lookupMap.end())
        return;

    Eq3TrvData* pNewData = new Eq3TrvData(address);
    _lookupMap[address] = pNewData;

    debug_printf("number of eq3 trvs: %d\n", _lookupMap.size());
}

bool Eq3TrvDevices::connect( std::string address )
{
    debug_printf("connect started\n");

    if( _pActiveClient )
        disconnect();

    NimBLEClient* pClient = nullptr;
    NimBLEAddress bleAddress(address);

    // Check if we have a client we should reuse first
    if( NimBLEDevice::getClientListSize() )
    {
        // special case when we already know this device, we send false as the second argument in connect() to prevent refreshing the service database
        // this saves considerable time and power
        pClient = NimBLEDevice::getClientByPeerAddress( bleAddress );
        if( pClient )
        {
            if(!pClient->connect( bleAddress, false ) )
            {
                debug_printf("Reconnect failed\n");
                return false;
            }
            debug_printf("Reconnected client\n");
        }
        // we don't already have a client that knows this device, we will check for a client that is disconnected that we can use
        else
        {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    // No client to reuse? Create a new one
    if( !pClient )
    {
        if(NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS)
        {
            debug_printf("Max clients reached - no more connections available\n");
            return false;
        }

        pClient = NimBLEDevice::createClient();
        debug_printf("New client created\n");

        pClient->setConnectionParams( 12, 12, 0, 51 );
        pClient->setConnectTimeout( 5 );

        if( !pClient->connect( bleAddress ) )
        {
            // Created a client but failed to connect, don't need to keep it as it has no data
            NimBLEDevice::deleteClient( pClient );
            debug_printf("Failed to connect, deleted client\n");
            return false;
        }
    }

    if( !pClient->isConnected() )
    {
        if (!pClient->connect( bleAddress ) )
        {
            debug_printf("Failed to connect\n");
            return false;
        }
    }

    debug_printf("Connected to: %s\n", pClient->getPeerAddress().toString().c_str());
    debug_printf("RSSI: %d\n", pClient->getRssi());

    // Now we can read/write/subscribe the charateristics of the services we are interested in
    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pCommandChar = nullptr;
    NimBLERemoteCharacteristic* pNotifyChar = nullptr;

    if( ( pSvc = pClient->getService( EQ3_SERVICE_UUID ) ) == nullptr )
    {
        debug_printf("eq3 trv service not found.\n");
        return false;
    }
    if((pCommandChar = pSvc->getCharacteristic( EQ3_COMMAND_CHAR_UUID )) == nullptr )
    {
        debug_printf("eq3 command char not found\n");
        return false;
    }

    if(pCommandChar->canRead())
        pCommandChar->readValue(); // make an explicit "read" to start authentication

    if( (pNotifyChar = pSvc->getCharacteristic(EQ3_NOTIFY_CHAR_UUID)) == nullptr)
    {
        debug_printf("eq3 notify char not found\n");
        return false;
    }

    if (pNotifyChar->canRead())
        pNotifyChar->readValue(); // make an explicit "read" to start authentication

    if( !pNotifyChar->canNotify() )
    {
        debug_printf("eq3 notify char cannot notify\n");
        return false;
    }

    if (!pNotifyChar->subscribe(true, _notifyCB))
    {
        // Disconnect if subscribe failed
        debug_printf("eq3 trv notify subscription failed\n");
        return false;
    }

    debug_printf("eq3 trv successfully connected\n");

    _pActiveClient = pClient;
    _pCommandChar = pCommandChar;
    _pNotifyChar = pNotifyChar;
    _pNotifyData = nullptr; // delete last indication result

    return true;
}

void Eq3TrvDevices::disconnect()
{
    debug_printf( "disconnect ");
    if(_pActiveClient )
    {
        if( _pNotifyChar )
            _pNotifyChar->unsubscribe();
        _pNotifyChar = nullptr;
        _pCommandChar = nullptr;
        if( _pActiveClient->isConnected() )
            _pActiveClient->disconnect();
        _pActiveClient = nullptr;
        _tiLastCommandTimeout = 0;
    }
    TriggerScanInterval();
    debug_printf("completed\n");
}

bool Eq3TrvDevices::waitCompletion()
{
    CmdStatus_t cmdStatus;
    while ((cmdStatus = CheckCmdCompletionResult()) == CMD_NORESULT) // eq3 trv needs ~ 100ms for indication
    {
        debug_printf(".");
        delay(10);
    }
    if (cmdStatus == CMD_SUCCESS)
        return true;

    return false;
}

void Eq3TrvDevices::notifyError(std::string address)
{
    // notify error by setting bValid flag to false
    Eq3TrvData* pTrv = FindTrvData(address);
    if (pTrv)
    {
        pTrv->_status.bValidData = false;
        _pNotifyData = pTrv;
    }
}

bool Eq3TrvDevices::issueCommand(std::string address, uint8_t command[], int len, bool bSync )
{
    // check if busy
    if (_tiLastCommandTimeout != 0)
    {
        debug_printf( "command already running, refuse.\n");
        return false;
    }

    // connect and issue command
    if( connect(address) )
    {
        if(_pCommandChar && _pCommandChar->writeValue( command, len, true ) )
        {
            _tiLastCommandTimeout = millis() + CMD_TIMEOUT; // timeout 5 seconds
            if( bSync )
                return waitCompletion(); // sync call is wasting ~ 100ms
            return true;
        }
    }
    else
    {
        disconnect();
        notifyError( address );
    }

    return false;
}
