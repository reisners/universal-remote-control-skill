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

#include <RCSwitch.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRUtils.h>

#include <WiFiUdp.h>
WiFiUDP UDP;
#include <WakeOnLan.h>
WakeOnLan WOL(UDP);

RCSwitch mySwitch = RCSwitch();

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

#define DS2401_PIN D7
OneWire ds(DS2401_PIN);                    // OneWire bus on digital pin 7
char urcid[24];  // buffer to hold device's URCID (DS2401 serial number as a hex string)

#define USE_SERIAL Serial

#define DEBUG_WEBSOCKETS

#define RF_PIN D8

#define IR_PIN D2

#define GREEN_PIN D4
#define RED_PIN D6
#define STATUS_INIT 0
#define STATUS_CONNECTING 1
#define STATUS_CONNECTED 2
#define STATUS_EXECUTING 3
#define PONG_MAX 524287

uint32_t countdown = 0;
int led_status = STATUS_INIT;

IRsend irsend(IR_PIN);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(GREEN_PIN, OUTPUT);     // Initialize the GREEN_PIN pin as an output
  pinMode(RED_PIN, OUTPUT);     // Initialize the RED_PIN pin as an output

  set_status(STATUS_CONNECTING);  
  
  USE_SERIAL.begin(115200);
  USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();

  for(uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  if (!initializeURCID()) {
    ESP.reset();
  }

  WiFiManager wifiManager;
  wifiManager.autoConnect(urcid);
  USE_SERIAL.println("Connected to Wifi.\n");

  // server address, port and URL
  webSocket.beginSSL("ord61b4er1.execute-api.eu-west-1.amazonaws.com", 443, "/beta");
  webSocket.setExtraHeaders();
  char headers[128];
  sprintf(headers, "URCID: %s", urcid);
  webSocket.setExtraHeaders(headers);

  // event handler
  webSocket.onEvent(webSocketEvent);

  // try every 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);
 
  mySwitch.enableTransmit(RF_PIN);

  irsend.begin();

  WOL.setRepeat(3, 100); // Repeat the packet three times with 100ms delay between
  USE_SERIAL.println("WOL.setRepeat() ok\n");
  WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
  USE_SERIAL.println("WOL.calculateBroadcastAddress() ok\n");
}

void loop() {
  webSocket.loop();
  led_loop();
}

void led_loop() {
  if (countdown > 0) {
    countdown--;
  }
  int green = LOW;
  int red = LOW;
  int blue = LOW;
  switch (led_status) {
    case STATUS_INIT:
      green = HIGH;
      red = HIGH;
      blue = HIGH;
      break;
    case STATUS_CONNECTING:
      green = LOW;
      red = HIGH;
      break;
    case STATUS_CONNECTED:
      green = HIGH;
      red = LOW;
      blue = LOW;
      break;
    case STATUS_EXECUTING:
      green = HIGH;
      red = LOW;
      blue = HIGH;
      break;
  }
  analogWrite(GREEN_PIN, countdown >> 9);
  digitalWrite(RED_PIN, red);
  digitalWrite(LED_BUILTIN, blue);
}

void set_status(int status) {
  led_status = status;
}

boolean initializeURCID() {
  byte buffer[8];
  byte crc_calc;    //calculated CRC
  byte crc_byte;    //actual CRC as sent by DS2401
  boolean present = ds.reset();
  if (!present) {
    USE_SERIAL.println("ERROR: DS2401 not present.");
    return 0;
  }
  
  ds.write(0x33);  //Send Read data command
  
  buffer[0] = ds.read();
  PrintTwoDigitHex (buffer[0], 1);
  
  for (int i = 1; i <= 6; i++)
  {
    buffer[i] = ds.read(); //store each byte in different position in array
    PrintTwoDigitHex (buffer[i], 0);
  }

  crc_byte = ds.read(); //read CRC, this is the last byte
  crc_calc = OneWire::crc8(buffer, 7); //calculate CRC of the data

  if (crc_calc != crc_byte) {
    USE_SERIAL.print("ERROR DS2401 CRC mismatch: 0x");
    PrintTwoDigitHex (crc_calc, 1);
    USE_SERIAL.print(" != 0x");
    PrintTwoDigitHex (crc_byte, 1);
    USE_SERIAL.println();
    return 0;
  }
  
  String s = ByteConvert::arrayToString(sizeof(buffer),buffer);
  sprintf(urcid, "URC%s", s.c_str());
  USE_SERIAL.printf("URCID = %s\n", urcid);
  return 1;
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
      USE_SERIAL.printf("[WSc] received %u bytes binary data:\n", length);
      hexdump(payload, length);

      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
   case WStype_PING:
        // pong will be send automatically
        USE_SERIAL.printf("[WSc] PING\n");
        break;
    case WStype_PONG:
        // answer to a ping we send
        USE_SERIAL.printf("[WSc] PONG\n");
        countdown = PONG_MAX;
        break;
    default:
      USE_SERIAL.printf("[WSc] unknown type: %d\n", type);
      ESP.reset();
      break;    
  }
}

void handleWsConnected(uint8_t * payload) {
  USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

  // send Aloha message to server when Connected
  webSocket.sendTXT("{'msg':'Aloha'}");
}

void handleWsDisconnected(uint8_t * payload) {
  USE_SERIAL.printf("[WSc] Disconnected!\n");
  set_status(STATUS_CONNECTING);  
}

void handleWsText(uint8_t * payload) {
  set_status(STATUS_EXECUTING);
  USE_SERIAL.printf("[WSc] received text: %s\n", payload);
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
  set_status(STATUS_CONNECTED);  
}

void handleACK(const JsonDocument& json) {
  USE_SERIAL.printf("ACK\n");
  set_status(STATUS_CONNECTED);
}

void handleSEND(const JsonDocument& json) {
  const char *channel = json["channel"];
  USE_SERIAL.printf("SEND --> %s\n", channel);
  if (!strcmp(channel, "IR")) {
    sendIR(json);
  } else if (!strcmp(channel, "RF433MHZ")) {
    sendRF433MHz(json);
  } else if (!strcmp(channel, "WakeOnLan")) {
    sendWakeOnLan(json);
  } else {
    USE_SERIAL.printf("[SEND] unknown channel: %s\n", channel);
  }
}

void sendIR(const JsonDocument& json) {
  const char *protocol = json["data"]["protocol"];

  if (!strcmp(protocol, "RC5")) {
  } else if (!strcmp(protocol, "RC6")) {
  } else if (!strcmp(protocol, "NEC")) {
  } else if (!strcmp(protocol, "SONY")) {
  } else if (!strcmp(protocol, "PANASONIC")) {
    const uint64_t code = hexToUInt64(json["data"]["code"]);
    const int nbits = json["data"]["nbits"];
    const int nrepeat = json["data"]["nrepeat"];
    irsend.sendPanasonic64(code, nbits, nrepeat);
    USE_SERIAL.printf("[IR] PANASONIC %s (%d bits) nrepeat %d\n", uint64ToString(code).c_str(), nbits, nrepeat);
  } else if (!strcmp(protocol, "JVC")) {
  } else if (!strcmp(protocol, "SAMSUNG")) {
  } else if (!strcmp(protocol, "WHYNTER")) {
  } else if (!strcmp(protocol, "AIWA_RC_T501")) {
  } else if (!strcmp(protocol, "LG")) {
  } else if (!strcmp(protocol, "SANYO")) {
  } else if (!strcmp(protocol, "MITSUBISHI")) {
  } else if (!strcmp(protocol, "DISH")) {
  } else if (!strcmp(protocol, "SHARP")) {
  } else if (!strcmp(protocol, "COOLIX")) {
  } else if (!strcmp(protocol, "DAIKIN")) {
  } else if (!strcmp(protocol, "DENON")) {
  } else if (!strcmp(protocol, "KELVINATOR")) {
  } else if (!strcmp(protocol, "SHERWOOD")) {
  } else if (!strcmp(protocol, "MITSUBISHI_AC")) {
  } else if (!strcmp(protocol, "RCMM")) {
  } else if (!strcmp(protocol, "SANYO_LC7461")) {
  } else if (!strcmp(protocol, "RC5X")) {
  } else if (!strcmp(protocol, "GREE")) {
  } else if (!strcmp(protocol, "PRONTO")) {
  } else if (!strcmp(protocol, "NEC_LIKE")) {
  } else if (!strcmp(protocol, "ARGO")) {
  } else if (!strcmp(protocol, "TROTEC")) {
  } else if (!strcmp(protocol, "NIKAI")) {
  } else if (!strcmp(protocol, "RAW")) {
    const uint16_t len = json["data"]["timings"].size();
    uint16_t timings[len];
    for (int i = 0; i < len; i++) {
      timings[i] = json["data"]["timings"][i];
    }
    uint16_t khz  = json["data"]["kilohertz"];
    irsend.sendRaw(timings, len, khz);
    USE_SERIAL.printf("[IR] RAW %d timings at %d kHz\n", len, khz);
  } else if (!strcmp(protocol, "GLOBALCACHE")) {
  } else if (!strcmp(protocol, "TOSHIBA_AC")) {
  } else if (!strcmp(protocol, "FUJITSU_AC")) {
  } else if (!strcmp(protocol, "MIDEA")) {
  } else if (!strcmp(protocol, "MAGIQUEST")) {
  } else if (!strcmp(protocol, "LASERTAG")) {
  } else if (!strcmp(protocol, "CARRIER_AC")) {
  } else if (!strcmp(protocol, "HAIER_AC")) {
  } else if (!strcmp(protocol, "MITSUBISHI2")) {
  } else if (!strcmp(protocol, "HITACHI_AC")) {
  } else if (!strcmp(protocol, "HITACHI_AC1")) {
  } else if (!strcmp(protocol, "HITACHI_AC2")) {
  } else if (!strcmp(protocol, "GICABLE")) {
  } else if (!strcmp(protocol, "HAIER_AC_YRW02")) {
  } else if (!strcmp(protocol, "WHIRLPOOL_AC")) {
  } else if (!strcmp(protocol, "SAMSUNG_AC")) {
  } else if (!strcmp(protocol, "LUTRON")) {
  } else if (!strcmp(protocol, "ELECTRA_AC")) {
  } else if (!strcmp(protocol, "PANASONIC_AC")) {
  } else if (!strcmp(protocol, "PIONEER")) {
  } else if (!strcmp(protocol, "LG2")) {
  } else if (!strcmp(protocol, "MWM")) {
  } else if (!strcmp(protocol, "DAIKIN2")) {
  } else if (!strcmp(protocol, "VESTEL_AC")) {
  } else if (!strcmp(protocol, "TECO")) {
  } else if (!strcmp(protocol, "SAMSUNG36")) {
  } else if (!strcmp(protocol, "TCL112AC")) {
  } else if (!strcmp(protocol, "LEGOPF")) {
  } else if (!strcmp(protocol, "MITSUBISHI_HEAVY_88")) {
  } else if (!strcmp(protocol, "MITSUBISHI_HEAVY_152")) {
  } else if (!strcmp(protocol, "DAIKIN216")) {
  } else if (!strcmp(protocol, "SHARP_AC")) {
  } else if (!strcmp(protocol, "GOODWEATHER")) {
  } else if (!strcmp(protocol, "INAX")) {
  } else if (!strcmp(protocol, "DAIKIN160")) {
  } else if (!strcmp(protocol, "NEOCLIMA")) {
  } else if (!strcmp(protocol, "DAIKIN176")) {
  } else if (!strcmp(protocol, "DAIKIN128")) {
  } else {
    USE_SERIAL.printf("[IR] unknown protocol %s\n", protocol);
  }
}

void sendRF433MHz(const JsonDocument& json) {
  const int protocol = json["data"]["protocol"];
  const int pulseLength = json["data"]["pulse-length"];
  const char *code = json["data"]["code"];
  mySwitch.setPulseLength(pulseLength);
  mySwitch.setProtocol(protocol);
  mySwitch.send(code);
  USE_SERIAL.printf("[RF433MHz] pulseLength=%d, protocol=%d, code=%s\n", pulseLength, protocol, code);
}

void sendWakeOnLan(const JsonDocument& json) {
  const char *mac = json["data"]["mac"];
  const int nrepeat = json["data"]["nrepeat"];
  const int delayms = json["data"]["delay"];
  WOL.setRepeat(nrepeat, delayms);
  WOL.sendMagicPacket(mac);
  USE_SERIAL.printf("[WakeOnLan] MAC=%s nrepeat=%d delay=%dms\n", mac, nrepeat, delayms);
}

uint64_t hexToUInt64(const char* hex)
{
  return (uint64_t) strtoull(hex, 0, 16);
}
