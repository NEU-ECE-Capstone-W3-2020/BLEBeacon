#include <bluefruit.h>
#include <vector>

#define MAX_PRPH_CONNECTIONS 1

// BLE Service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery
uint8_t connection_cnt;
uint16_t connections[MAX_PRPH_CONNECTIONS];

void setup()
{
  Serial.begin(9600);
  while( !Serial ) delay(10);

  connection_cnt = 0;
  memset(connections, 0, MAX_PRPH_CONNECTIONS * sizeof(uint16_t));
  // Setup the BLE LED to be enabled on CONNECT
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 
  /* Bluefruit.configPrphBandwidth(BANDWIDTH_MAX); */

  Bluefruit.begin(MAX_PRPH_CONNECTIONS, 0);
  Bluefruit.setTxPower(4);
  Bluefruit.setName("Capstone Beacon");
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // Configure and Start Device Information Service
  bledis.setManufacturer("Group W3");
  bledis.setModel("B01");
  bledis.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // Configure and Start BLE Uart Service
  bleuart.setNotifyCallback(notify_callback);
  bleuart.begin();

  // Set up and start advertising
  startAdv();
}

String lastStr = "Hello world";

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

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
  /* if (Serial.available()) */
  /* { */
  /*     uint8_t buf[64]; */
  /*     int count = Serial.readBytes(buf, sizeof(buf)); */
  /*     bleuart.write( buf, count ); */
  /* } */
    while(Serial.available()){
        String data = Serial.readString();
        updateAdvertisedString(data);
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
  Serial.print(curStr.c_str());
  bleuart.write((const uint8_t *) curStr.c_str(), curStr.length());
  for(uint8_t i = 0; i < connection_cnt; ++i)  {
    bleuart.write(connections[i], (const uint8_t *) curStr.c_str(), curStr.length());
  }
  lastStr = curStr;
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, 32);

  Serial.print("Connected to ");
  Serial.println(central_name);
  connections[connection_cnt] = conn_handle;
  ++connection_cnt;
  if(connection_cnt < MAX_PRPH_CONNECTIONS) {
    Serial.println("Keep Advertising");
    Bluefruit.Advertising.start(0);
  }
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);

  for(uint8_t i = 0; i < connection_cnt; ++i) {
    if(connections[i] == conn_handle) {
      for(uint8_t j = i; j < MAX_PRPH_CONNECTIONS - 1; ++j) {
        connections[j] = connections[j + 1];
      }
      connections[MAX_PRPH_CONNECTIONS - 1] = 0;
      break;
    }
  }
  --connection_cnt;
}

void notify_callback(uint16_t conn_handle, bool enabled) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, 32);
  Serial.print(central_name);
  if(enabled) {
    Serial.println(" notifications enabled!");
  } else {
    Serial.println(" notifications disabled!");
  }
}
