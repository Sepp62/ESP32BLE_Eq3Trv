
/** EQ3 TRV 
 *
 *  Created: 01/01/2023
 *      Author: Bernd Woköck
 *
 *  Derived from NimBLE client sample: H2zero: https://github.com/h2zero/NimBLE-Arduino
 *                                     Use at least NimBLE V1.4.1 !!!
 * 
 * Protocol sources:
 * https://reverse-engineering-ble-devices.readthedocs.io/en/latest/protocol_description/00_protocol_description.html
 * https://github.com/Heckie75/eQ-3-radiator-thermostat/blob/master/eq-3-radiator-thermostat-api.md
 * https://github.com/softypit/esp32_mqtt_eq3
 * https://github.com/softypit/esp32_mqtt_eq3/blob/master/main/eq3_main.c
 * Some hints:
 * https://github.com/h2zero/NimBLE-Arduino/issues/223
 * https://github.com/arendst/Tasmota/issues/16775
 * https://github.com/barbudor/Tasmota/blob/development/tasmota/tasmota_xdrv_driver/xdrv_79_esp32_ble.ino
 * 
 */

#include <NimBLEDevice.h>
#include "Eq3TrvDevices.h"

Eq3TrvDevices eq3Trvs;

static uint32_t scanTime = 50; // should be shorter than SCAN_INTERVAL 

// Define a class to handle the callbacks when advertisments are received
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks
{
    void onResult(NimBLEAdvertisedDevice* advertisedDevice)
    {
        eq3Trvs.OnBtDeviceDiscovered( advertisedDevice );
    };
};

// Notification / Indication receiving handler callback
void notifyCB( NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify )
{
    eq3Trvs.OnBtNotify(pRemoteCharacteristic, pData, length, isNotify);
    return;
}

// Callback to process the results of the last scan or restart it
void scanEndedCB(NimBLEScanResults results)
{
    eq3Trvs.TriggerScanInterval();
    Serial.println("scan ended");
}

// test functions
void PrintStatus()
{
    Eq3TrvStatus_t status;
    if (eq3Trvs.GetLastStatusResult(status))
    {
        Serial.println("");
        Serial.println("device status");
        Serial.printf("data valid: %d\n",status.bValidData );
        Serial.printf("device: %s\n",    status.address.c_str() );
        Serial.printf("temp: %2.1f\n",   status.fTemp);
        Serial.printf("valve: %d%%\n",   status.nValve);
        Serial.printf("mode auto: %d\n", status.bAutoMode);
        Serial.printf("holiday:   %d\n", status.bHoliday);
        Serial.printf("boost:     %d\n", status.bBoost);
        Serial.printf("dst:       %d\n", status.bDst );
        Serial.printf("wdwopen:   %d\n", status.bWindowOpen);
        Serial.printf("locked:    %d\n", status.bLocked);
        Serial.printf("lowbat:    %d\n", status.bLowBattery);
    }
}

void Test()
{
    static bool bSync = true; // true = execute commands with blocking wait, false = return before notification received

    if (Serial.available() > 0)
    {
        char c = Serial.read();

        if (c == '0')
            eq3Trvs.ModeAuto("00:1a:22:1b:98:b0", false, bSync);
        else if (c == '1')
            eq3Trvs.ModeAuto("00:1a:22:1b:98:b0", true, bSync);
        else if (c == '2')
            eq3Trvs.SelectTemp("00:1a:22:1b:98:b0", 19.0, bSync);
        else if (c == '3')
            eq3Trvs.SelectTemp("00:1a:22:1b:98:b0", 20.0, bSync);
        else if (c == '4')
            eq3Trvs.Lock("00:1a:22:1b:98:b0", true, bSync);
        else if (c == '5')
            eq3Trvs.Lock("00:1a:22:1b:98:b0", false, bSync);
        else if (c == '6')
            eq3Trvs.SelectComfortTemp("00:1a:22:1b:98:b0", bSync);
        else if (c == '7')
            eq3Trvs.SelectReducedTemp("00:1a:22:1b:98:b0", bSync);
        else if (c == '8')
            eq3Trvs.SelectTemp("00:1a:22:12:bc:33", 19.5, bSync);
        else if (c == '9')
            eq3Trvs.SelectTemp("00:1a:22:12:bc:33", 20.5, bSync);
        else if (c == 'a')
            eq3Trvs.SelectTemp("00:1a:22:12:b4:d7", 18.5, bSync);
        else if (c == 'b')
            eq3Trvs.SelectTemp("00:1a:22:12:b4:d7", 20.5, bSync);
        else if (c == 'c')
            eq3Trvs.SetTime("00:1a:22:12:b4:d7", 6, 1, 2023, 0, 4, 0, bSync);

        if (bSync)
            PrintStatus();
    }

    // print results from async operation
    if (!bSync)
    {
        if (eq3Trvs.CheckCmdCompletionResult() != CMD_NORESULT)
            PrintStatus();
    }
}

void setup ()
{
    Serial.begin(115200);
    Serial.println("Starting NimBLE Client");

    eq3Trvs.SetNotifyCallback( notifyCB );

    // Initialize NimBLE, no device name specified as we are not advertising
    NimBLEDevice::init("");

    // create new scan
    NimBLEScan* pScan = NimBLEDevice::getScan(); 
    
    // create a callback that gets called when advertisers are found
    pScan->setAdvertisedDeviceCallbacks( new AdvertisedDeviceCallbacks() );
    
    // Set scan interval (how often) and window (how long) in milliseconds
    pScan->setInterval(100);
    pScan->setWindow(99);
    
    // Active scan will gather scan response data from advertisers but will use more energy from both devices
    pScan->setActiveScan(true);
    // Start scanning for advertisers for the scan time specified (in seconds) 0 = forever, optional callback for when scanning stops. 
    pScan->start(scanTime, scanEndedCB);
}

void loop ()
{
    Test();

    // start device scan (break after every 60 seconds)
    if( !NimBLEDevice::getScan()->isScanning() && eq3Trvs.IsScanIntervalExpired() )
    {
        NimBLEDevice::getScan()->start(scanTime,scanEndedCB);
        Serial.println( "scan started" );
    }
}
