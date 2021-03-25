#include <bluefruit.h>

// BLE Service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery

const uint8_t CUSTOM_SERVICE_UUID[] =
{
  0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
  0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
};

const uint8_t CUSTOM_TX_UUID[] =
{
  0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
  0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
};

const uint8_t CUSTOM_RX_UUID[] =
{
  0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
  0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E
};


BLEUuid service_uuid = BLEUuid(CUSTOM_SERVICE_UUID);
BLEUuid tx_characteristic_uuid = BLEUuid(CUSTOM_TX_UUID);
BLEUuid rx_characteristic_uuid = BLEUuid(CUSTOM_RX_UUID);

BLEService        beaconService = BLEService(service_uuid);
BLECharacteristic beaconTxCharacteristic = BLECharacteristic(tx_characteristic_uuid);
BLECharacteristic beaconRxCharacteristic = BLECharacteristic(rx_characteristic_uuid);

void setup()
{
  Serial.begin(9600);

  // Setup the BLE LED to be enabled on CONNECT
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BEACON");

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // Configure and Start Device Information Service
  bledis.setManufacturer("Group W3");
  bledis.setModel("B01");
  bledis.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // Set up and start advertising
  startAdv();
}

String lastStr = "Hello world";

void startAdv(void)
{
  beaconService.begin();

  beaconRxCharacteristic.setProperties(CHR_PROPS_READ);
  beaconRxCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  beaconRxCharacteristic.begin();
  beaconRxCharacteristic.write("Hello world");
  
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  beaconTxCharacteristic.setProperties(CHR_PROPS_WRITE);
  beaconTxCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  beaconTxCharacteristic.begin();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void loop()
{
  // Forward data from HW Serial to BLEUART
  if (Serial.available())
  {
      uint8_t buf[64];
      int count = Serial.readBytes(buf, sizeof(buf));
      bleuart.write( buf, count );
  }

  // Forward from BLEUART to HW Serial
  String curStr = "";
  while ( bleuart.available() )
  {
    uint8_t ch;
    ch = (uint8_t) bleuart.read();
    Serial.write(ch);
    curStr += (char)ch;
  }
  if (curStr != "") {
    Serial.print("received string :");
    Serial.println(curStr);
    // when a new string is written to the beacon, update the advertisment data
    if (curStr != lastStr) {
      updateAdvertisedString(curStr);
    }
  }
}

void updateAdvertisedString(String curStr) {
  beaconRxCharacteristic.write(curStr.c_str());
  Serial.print(curStr.c_str());
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);

  
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}
