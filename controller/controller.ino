#include <bluefruit.h>

/* Nordic UART Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * RX Characteristic:   6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 * TX Characteristic:   6E400003-B5A3-F393-E0A9-E50E24DCCA9E
 */

// must reverse each byte for *little endian* representation of data
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
BLEUuid rx_characteristic_uuid = BLEUuid(CUSTOM_RX_UUID); // for writing data (beacon -> device)
BLEUuid tx_characteristic_uuid = BLEUuid(CUSTOM_TX_UUID); // for reading data (device -> beacon)

BLEService        beaconService = BLEService(service_uuid);
BLECharacteristic beaconRxCharacteristic = BLECharacteristic(rx_characteristic_uuid);
BLECharacteristic beaconTxCharacteristic = BLECharacteristic(tx_characteristic_uuid);

BLEDis bledis;    // device information service
BLEBas blebas;    // battery service

uint8_t  bps = 0;

void setup()
{
  Serial.begin(115200); // baud rate
  while ( !Serial ) delay(10);
  
  Serial.println("------------------------------\n");
  Serial.println("----CAPSTONE BEACON DRIVER----");
  Serial.println("------------------------------\n");

  // init the nRF52
  Serial.println("Initializing hardware...");
  Bluefruit.begin();
  Serial.println("Setting device name to 'BEACON'");
  Bluefruit.setName("BEACON");

  // set connect/disconnect callback handlers
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  Serial.println("Configuring the device information service");
  bledis.setManufacturer("GROUP W3");
  bledis.setModel("DA BEACON");
  bledis.begin();

  Serial.println("Configuring the battery service");
  blebas.begin();
  blebas.write(100);

  Serial.println("Configuring the custom service");
  setupService();

  startAdvertising();
  Serial.println("\nAdvertising!");
}

void startAdvertising(void)
{
  // advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(beaconService);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); 
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void setupService(void)
{
  beaconService.begin();

  beaconRxCharacteristic.setProperties(CHR_PROPS_WRITE);
  beaconRxCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  beaconRxCharacteristic.setFixedLen(1);
  beaconRxCharacteristic.begin();
  beaconRxCharacteristic.write8(6); // initial count = 0?

  beaconTxCharacteristic.setProperties(CHR_PROPS_READ);
  beaconTxCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  beaconTxCharacteristic.setFixedLen(1);
  beaconTxCharacteristic.begin();
  beaconTxCharacteristic.write8(2);
}

void connect_callback(uint16_t conn_handle)
{
  // current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
  Serial.println("Advertising!");
}

void loop()
{
  digitalToggle(LED_RED);
  
  if ( Bluefruit.connected() ) {
    Serial.println("CONNECTED");
  }

  // update once per second
  delay(1000);
}
