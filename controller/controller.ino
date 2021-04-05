#include <bluefruit.h>

#include "connection_wrapper.h"
#include "peripherial_connection.h"

// Print to serial
#define DEBUG

#define MAX_CNTRL_CONNECTIONS 1
#define MAX_PRPH_CONNECTIONS  2
#define DEFAULT_STRING        "Hello World"

// BLE Services
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery

uint8_t cntrl_conn_cnt = 0;
ConnectionWrapper cntrl_connections[MAX_CNTRL_CONNECTIONS];

uint8_t prph_conn_cnt = 0;
PeripheralConnection prph_connections[MAX_PRPH_CONNECTIONS];

String lastStr;

void setup()
{
  Serial.begin(9600);
  while( !Serial ) delay(10);

  // Setup the BLE LED to be enabled on CONNECT
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 

  Bluefruit.begin(MAX_CNTRL_CONNECTIONS, MAX_PRPH_CONNECTIONS);
  Bluefruit.setTxPower(4);
  Bluefruit.setName("Capstone Beacon");

  Bluefruit.Periph.setConnectCallback(cntrl_connect_callback);
  Bluefruit.Periph.setDisconnectCallback(cntrl_disconnect_callback);

  Bluefruit.Central.setConnectCallback(prph_connect_callback);
  Bluefruit.Central.setDisconnectCallback(prph_disconnect_callback);

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

  for(uint8_t id = 0; id < MAX_PRPH_CONNECTIONS; ++id) {
    prph_connections[id].bleuart.begin();
    prph_connections[id].bleuart.setRxCallback(bleuart_rx_callback);
  }

  // Set up and start advertising and scanning
  startScanning();
  startAdv();
  updateAdvertisedString(DEFAULT_STRING);
}

void startScanning(void){
  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Interval = 100 ms, window = 80 ms
   * - Filter only accept bleuart service in advertising
   * - Don't use active scan (used to retrieve the optional scan response adv packet)
   * - Start(0) = will scan forever since no timeout is given
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80);       // in units of 0.625 ms
  Bluefruit.Scanner.filterUuid(BLEUART_UUID_SERVICE);
  Bluefruit.Scanner.useActiveScan(false);       // Don't request scan response data
  Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds
#ifdef DEBUG
  Serial.println("Started Scanning");
#endif
}

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
#ifdef DEBUG
  Serial.println("Started Advertising");
#endif
}

void loop()
{
  // Forward data from HW Serial to BLEUART
  /*
  if (Serial.available())
  {
      uint8_t buf[64];
      int count = Serial.readBytes(buf, sizeof(buf));
      bleuart.write( buf, count );
  }
  */
  while(Serial.available()){
    String data = Serial.readString();
    updateAdvertisedString(data);
  }

  // Forward from BLEUART to HW Serial
  String curStr = "";
  while (bleuart.available()) {
    // default MTU with an extra byte for string terminator
    uint8_t buffer[20 + 1] = {0};
    if(bleuart.read(buffer, 20)) {
      curStr = String((char *) buffer);
    }
  }
  if (curStr != "") {
#ifdef DEBUG
    Serial.print("Received string: ");
    Serial.println(curStr);
#endif
    updateAdvertisedString(curStr);
  }
}

void updateAdvertisedString(String curStr) {
  if(curStr == lastStr) return;
#ifdef DEBUG
  Serial.print("Updating advertised string to: ");
#endif
  Serial.println(curStr);

  bleuart.write((const uint8_t *) curStr.c_str(), curStr.length());
  // forward to centrals
  for(uint8_t id = 0; id < MAX_CNTRL_CONNECTIONS; ++id)  {
    if(!cntrl_connections[id].isInvalid()){
      bleuart.write(cntrl_connections[id].getConnHandle(), (const uint8_t *) curStr.c_str(), curStr.length());
    }
  }
  lastStr = curStr;
}

/**
 * Callback invoked when scanner picks up an advertising packet
 * @param report Structural advertising data
 */
void scan_callback(ble_gap_evt_adv_report_t* report) {
#ifdef DEBUG
  Serial.println("Scan Callback");
#endif
  // Since we configure the scanner with filterUuid()
  // Scan callback only invoked for device with bleuart service advertised  
  // Connect to the device with bleuart service in advertising packet
  Bluefruit.Central.connect(report);
}

/**
 * Callback invoked when an peripherals connection is established
 * @param conn_handle
 */
void prph_connect_callback(uint16_t conn_handle) {
#ifdef DEBUG
  Serial.printf("Peripheral Connect Callback, Handle: %u\n", conn_handle);
#endif
  // Find an available ID to use
  PeripheralConnection* peripheral = nullptr;
  for(uint8_t id = 0; id < MAX_PRPH_CONNECTIONS; ++id) {
    if(prph_connections[id].isInvalid()) {
      peripheral = &prph_connections[id];
      break;
    }
  }
  if(!peripheral) return;

  peripheral->setConnHandle(conn_handle);
  char peripheral_name[32] = {0};
  Bluefruit.Connection(conn_handle)->getPeerName(peripheral_name, 32);
  peripheral->setName(peripheral_name);

#ifdef DEBUG
  Serial.print("Connected to ");
  Serial.println(peripheral_name);

  Serial.print("Discovering BLE UART service ... ");
#endif
  if (peripheral->bleuart.discover(conn_handle)) {
#ifdef DEBUG
    Serial.println("Found it");
    Serial.println("Enabling TXD characteristic's CCCD notify bit");
#endif
    peripheral->bleuart.enableTXD();
  } else {
#ifdef DEBUG
    Serial.println("Found ... NOTHING!");
#endif

    // disconnect since we couldn't find bleuart service
    Bluefruit.disconnect(conn_handle);
    peripheral->invalidate();
  }  

  prph_conn_cnt++;
  if(prph_conn_cnt < MAX_PRPH_CONNECTIONS) {
    Bluefruit.Scanner.start(0);
  }
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void prph_disconnect_callback(uint16_t conn_handle, uint8_t reason) {
#ifdef DEBUG
  Serial.printf("Peripheral Disconnect Callback, Handle: %u\n", conn_handle);
#endif
  PeripheralConnection* peripheral = nullptr;
  for(uint8_t id = 0; id < MAX_PRPH_CONNECTIONS; ++id) {
    if(prph_connections[id].getConnHandle() == conn_handle) {
      peripheral = &prph_connections[id];
      break;
    }
  }
  if(!peripheral) return;
#ifdef DEBUG
  Serial.print("Peripheral ");
  Serial.print(peripheral->getName());
  Serial.print(" disconnected, reason = 0x");
  Serial.println(reason, HEX);
#endif

  peripheral->bleuart.disableTXD();
  peripheral->invalidate();
  prph_conn_cnt--;
}

/**
 * Callback invoked when BLE UART data is received
 * @param uart_svc Reference object to the service where the data 
 * arrived.
 */
void bleuart_rx_callback(BLEClientUart& uart_svc) {
#ifdef DEBUG
  Serial.println("Client BLE Uart RX Callback");
#endif
  // uart_svc is peripheral.bleuart
  PeripheralConnection *peripheral = nullptr;
  for(uint8_t id = 0; id < MAX_PRPH_CONNECTIONS; ++id) {
    if(prph_connections[id].getConnHandle() == uart_svc.connHandle()) {
      peripheral = &prph_connections[id];
      break;
    }
  }
  if(!peripheral) return;
  
  // Read then forward to all peripherals
  while (uart_svc.available()) {
    // default MTU with an extra byte for string terminator
    uint8_t buffer[20 + 1] = {0};
    if (uart_svc.read(buffer, 20)) {
      String msg((char *) buffer);
#ifdef DEBUG
      Serial.printf("Received: %s from %s\n", msg, peripheral->getName());
#endif
      updateAdvertisedString(msg);
    }
  }
}

// callback invoked when central connects
void cntrl_connect_callback(uint16_t conn_handle) {
#ifdef DEBUG
  Serial.printf("Central Connect Callback, Handle: %u\n", conn_handle);
#endif
  ConnectionWrapper *central = nullptr;
  for(uint8_t id = 0; id < MAX_CNTRL_CONNECTIONS; ++id) {
      if(cntrl_connections[id].isInvalid()){
        central = &cntrl_connections[id];
        break;
      }
  }
  if(!central) return;

  central->setConnHandle(conn_handle);
  char central_name[32] = {0};
  Bluefruit.Connection(conn_handle)->getPeerName(central_name, 32);
  central->setName(central_name);

#ifdef DEBUG
  Serial.print("Connected to Central: ");
  Serial.println(central_name);
#endif
  ++cntrl_conn_cnt;
  if(cntrl_conn_cnt < MAX_CNTRL_CONNECTIONS) {
    // keep advertising
    Bluefruit.Advertising.start(0);
  }
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void cntrl_disconnect_callback(uint16_t conn_handle, uint8_t reason) {
#ifdef DEBUG
  Serial.printf("Central Disconnect Callback, Handle: %u\n", conn_handle);
#endif
  ConnectionWrapper *central = nullptr;
  for(uint8_t id = 0; id < MAX_CNTRL_CONNECTIONS; ++id) {
      if(cntrl_connections[id].getConnHandle() == conn_handle){
        central = &cntrl_connections[id];
        break;
      }
  }
  if(!central) return;

#ifdef DEBUG
  Serial.print("Central ");
  Serial.print(central->getName());
  Serial.print(" disconnected, reason = 0x");
  Serial.println(reason, HEX);
#endif
  central->invalidate();
  --cntrl_conn_cnt;
}

void notify_callback(uint16_t conn_handle, bool enabled) {
#ifdef DEBUG
  Serial.printf("Notify Callback, Handle: %u\n", conn_handle);
#endif
  ConnectionWrapper *central = nullptr;
  for(uint8_t id = 0; id < MAX_CNTRL_CONNECTIONS; ++id) {
      if(cntrl_connections[id].getConnHandle() == conn_handle){
        central = &cntrl_connections[id];
        break;
      }
  }
  if(!central) return;
#ifdef DEBUG
  Serial.print(central->getName());
  Serial.print(" notifications ");
  Serial.println(enabled ? "enabled!" : "disabled!");
#endif
}
