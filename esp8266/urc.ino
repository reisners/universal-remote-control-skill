#include <ArduinoJson.h>

#include <ByteConvert.hpp>

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClientSecure.h>

#include <WebSocketsClient.h>

#include <Hash.h>

#include <OneWire.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

#define DS2401_PIN D7
OneWire ds(DS2401_PIN);                    // OneWire bus on digital pin 7
char urcid[24];  // buffer to hold device's URCID (DS2401 serial number as a hex string)

#define USE_SERIAL Serial

#define DEBUG_WEBSOCKETS

boolean readOutSerialNumber() {
  byte buffer[8];
  byte crc_calc;    //calculated CRC
  byte crc_byte;    //actual CRC as sent by DS2401
  boolean present = ds.reset();
  if (!present) {
    return 0;
  }
  
  ds.write(0x33);  //Send Read data command
  
  buffer[0] = ds.read();
  USE_SERIAL.print("Family code: 0x");
  PrintTwoDigitHex (buffer[0], 1);
  
  USE_SERIAL.print("Hex ROM data: ");
  for (int i = 1; i <= 6; i++)
  {
    buffer[i] = ds.read(); //store each byte in different position in array
    PrintTwoDigitHex (buffer[i], 0);
    USE_SERIAL.print(" ");
  }
  USE_SERIAL.println();

  crc_byte = ds.read(); //read CRC, this is the last byte
  crc_calc = OneWire::crc8(buffer, 7); //calculate CRC of the data

  USE_SERIAL.print("Calculated CRC: 0x");
  PrintTwoDigitHex (crc_calc, 1);
  USE_SERIAL.print("Actual CRC: 0x");
  PrintTwoDigitHex (crc_byte, 1);  

  String s = ByteConvert::arrayToString(sizeof(buffer),buffer);
  sprintf(urcid, "URC%s", s.c_str());
  USE_SERIAL.printf("URCID = [%s]\n", urcid);
}

void PrintTwoDigitHex (byte b, boolean newline)
{
  Serial.print(b/16, HEX);
  Serial.print(b%16, HEX);
  if (newline) Serial.println();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch(type) {
    case WStype_DISCONNECTED:
      handleWsDisconnected(payload);
      break;
    case WStype_CONNECTED:
      handleWsConnected(payload);
      break;
    case WStype_TEXT:
      handleWsText(payload);
      break;
    case WStype_BIN:
      USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
      hexdump(payload, length);

      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
  }
}

void handleWsConnected(uint8_t * payload) {
  USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

  // send Aloha message to server when Connected
  webSocket.sendTXT("{'msg':'Aloha'}");
}

void handleWsText(uint8_t * payload) {
  USE_SERIAL.printf("[WSc] get text: %s\n", payload);
  StaticJsonDocument<1000> json;
  DeserializationError error = deserializeJson(json, payload);
  const char *command = json["command"];
  if (!strcmp(command, "ACK")) {
    handleACK(json);
  } else if (!strcmp(command, "SEND")) {
    handleSEND(json);
  } else {
    USE_SERIAL.printf("[WSc] unknown command: %s\n", command);
  }
}

void handleACK(JsonDocument json) {
  USE_SERIAL.printf("ACK\n");
}

void handleSEND(JsonDocument json) {
  const char *channel = json["channel"];
  const char *data = json["data"];
  USE_SERIAL.printf("SEND %s --> %s\n", data, channel);
}

void handleWsDisconnected(uint8_t * payload) {
  USE_SERIAL.printf("[WSc] Disconnected!\n");
}

void setup() {
  USE_SERIAL.begin(115200);
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for(uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  readOutSerialNumber();

  /*
  WiFiMulti.addAP("WLAN-740009", "8037832519588396");

  USE_SERIAL.print("Connecting");
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    USE_SERIAL.print(".");
  }
  */
  WiFiManager wifiManager;
  wifiManager.autoConnect("UniversalRemoteControl");

  USE_SERIAL.println("Connected to Wifi.");

  // server address, port and URL
  webSocket.beginSSL("ord61b4er1.execute-api.eu-west-1.amazonaws.com", 443, "/beta");
  webSocket.setExtraHeaders();
  char headers[128];
  sprintf(headers, "URCID: %s", urcid);
  webSocket.setExtraHeaders(headers);

  // event handler
  webSocket.onEvent(webSocketEvent);

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();
}
