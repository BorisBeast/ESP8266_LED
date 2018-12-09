#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <Ticker.h>

extern "C" {
  #include "user_interface.h"
}

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <WiFiManager.h>           //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <ArduinoOTA.h>

#include <FS.h>
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal

#define FASTLED_ESP8266_RAW_PIN_ORDER
//#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
FASTLED_USING_NAMESPACE

#include <EEPROM.h>

// An IR detector/demodulator is connected to GPIO pin 5
#define RECV_PIN 5    //D1
#define BAUD_RATE 115200
#define LED_PIN 16    //D0

#define CAPTURE_BUFFER_SIZE 100
#define TIMEOUT 15U
#define MIN_UNKNOWN_SIZE 12

ADC_MODE(ADC_VCC);

// Use turn on the save buffer feature for more complete capture coverage.
IRrecv irrecv(RECV_PIN, CAPTURE_BUFFER_SIZE, TIMEOUT, true);
decode_results results;  // Somewhere to store the results
Ticker ticker;

ESP8266WebServer server(80);

#define DATA_PIN      D2     // for Huzzah: Pins w/o special function:  #4, #5, #12, #13, #14; // #16 does not work :(
#define LED_TYPE      WS2812
#define COLOR_ORDER   GRB
#define NUM_LEDS     108

#define MILLI_AMPS         10000//2000     // IMPORTANT: set here the max milli-Amps of your power supply 5V 2A = 2000
#define FRAMES_PER_SECOND  120 // here you can control the speed. With the Access Point / Web Server the animations run a bit slower.

#define EE_HUE 0
#define EE_SAT 1
#define EE_VAL 2

CRGB leds[NUM_LEDS];
CRGB solidColor = CRGB::White;
CHSV color(0,0,255);

uint8_t power = 0;
uint8_t brightness = 255;
bool wifiConnected = false;

void toggleLed()
{
  int state = digitalRead(LED_PIN);  // get the current state of LED pin
  digitalWrite(LED_PIN, !state);     // set pin to the opposite state
}

void setLed(uint8_t state)
{
  digitalWrite(LED_PIN, state);
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, toggleLed);
}

bool connectToWiFi() {
  ticker.attach(0.5, toggleLed);
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSTAStaticIPConfig(IPAddress(192,168,1,228), IPAddress(192,168,1,254), IPAddress(255,255,255,0));
  wifiManager.setConfigPortalTimeout(180);

  if (!wifiManager.autoConnect("configESP")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    // ESP.reset();
    // delay(1000);
    ticker.attach(1, toggleLed);
    return false;
  }

  Serial.println("connected...yeey :)");
  ticker.detach();

  digitalWrite(LED_PIN, HIGH);
  return true;
}

void otaSetup() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"vlad1234");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("*OTA: Ready");
}

void setupWebServer() {
  server.serveStatic("/index.htm", SPIFFS, "/index.htm");
  server.serveStatic("/fonts", SPIFFS, "/fonts", "max-age=86400");
  server.serveStatic("/js", SPIFFS, "/js");
  server.serveStatic("/css", SPIFFS, "/css", "max-age=86400");
  server.serveStatic("/images", SPIFFS, "/images", "max-age=86400");
  server.serveStatic("/", SPIFFS, "/index.htm");

  server.on("/all", HTTP_GET, []() {
    sendAll();
  });

  server.on("/power", HTTP_POST, []() {
    String value = server.arg("value");
    setPower(value.toInt());
    sendPower();
  });

  server.on("/color", HTTP_POST, []() {
    String h = server.arg("h");
    String s = server.arg("s");
    String v = server.arg("v");
    setColor(h.toInt(), s.toInt(), v.toInt());
    sendColor();
  });

  server.begin();

  Serial.println("HTTP server started");
}

void sendAll()
{
    String json = "{";

    json += "\"power\":" + String(power);
    json += ",\"color\":{";
    json += "\"h\":" + String(color.h);
    json += ",\"s\":" + String(color.s);
    json += ",\"v\":" + String(color.v);
    json += "}}";

    server.send(200, "text/json", json);
}

void sendPower()
{
  String json = String(power);
  server.send(200, "text/json", json);
}

void sendColor()
{
  String json = "{";
  json += "\"h\":" + String(color.h);
  json += ",\"s\":" + String(color.s);
  json += ",\"v\":" + String(color.v);
  json += "}";
  server.send(200, "text/json", json);
}

void setPower(uint8_t value)
{
  power = value == 0 ? 0 : 1;
  setLed(!power);
  EEPROM.write(5, power);
  EEPROM.commit();
}

void setColor(uint8_t h, uint8_t s, uint8_t v)
{
  color = CHSV(h, s, v);
  hsv2rgb_spectrum(color, solidColor);

  EEPROM.write(EE_HUE, h);
  EEPROM.write(EE_SAT, s);
  EEPROM.write(EE_VAL, v);
  EEPROM.commit();

  Serial.printf("h=%u,s=%u,v=%u\r\n", h, s, v);
}

void setup() {
  Serial.begin(BAUD_RATE, SERIAL_8N1, SERIAL_TX_ONLY);
  while (!Serial)  // Wait for the serial connection to be establised.
    delay(50);

  pinMode(LED_PIN, OUTPUT);

  wifiConnected = connectToWiFi();

  if (wifiConnected) {
    otaSetup();
  }

  EEPROM.begin(512);
  loadSettings();
  
  Serial.println();
  Serial.print("IRrecvDumpV2 is now running and waiting for IR input on Pin ");
  Serial.println(RECV_PIN);

#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(MIN_UNKNOWN_SIZE);
#endif  // DECODE_HASH
  irrecv.enableIRIn();  // Start the receiver

  Serial.println();
  Serial.print( F("Heap: ") ); Serial.println(ESP.getFreeHeap());
  Serial.print( F("Boot Vers: ") ); Serial.println(system_get_boot_version());
  Serial.print( F("CPU: ") ); Serial.println(ESP.getCpuFreqMHz());
  Serial.print( F("SDK: ") ); Serial.println(ESP.getSdkVersion());
  Serial.print( F("Chip ID: ") ); Serial.println(ESP.getChipId());
  Serial.print( F("Flash ID: ") ); Serial.println(ESP.getFlashChipId());
  Serial.print( F("Flash Size: ") ); Serial.println(ESP.getFlashChipRealSize());
  Serial.print( F("Vcc: ") ); Serial.println(ESP.getVcc());
  Serial.println();

  if (wifiConnected) {
    if (SPIFFS.begin()) {
      Serial.println("mounted file system");
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();
        Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
      }
      Serial.printf("\n");
    
      setupWebServer();
    } else {
      Serial.println("failed to mount FS");
    }
  }

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
}

void loadSettings()
{
  byte h = EEPROM.read(EE_HUE);
  byte s = EEPROM.read(EE_SAT);
  byte v = EEPROM.read(EE_VAL);

  if (v == 0) v = 255;

  color = CHSV(h, s, v);
  hsv2rgb_spectrum(color, solidColor);
}


void loop() {
  if (wifiConnected) {
    ArduinoOTA.handle();

    server.handleClient();
  }
  
  // Check if the IR code has been received.
  if (irrecv.decode(&results)) {
    // Display a crude timestamp.
    uint32_t now = millis();
    Serial.printf("Timestamp : %06u.%03u\n", now / 1000, now % 1000);
    if (results.overflow)
      Serial.printf("WARNING: IR code is too big for buffer (>= %d). "
                    "This result shouldn't be trusted until this is resolved. "
                    "Edit & increase CAPTURE_BUFFER_SIZE.\n",
                    CAPTURE_BUFFER_SIZE);
//    // Display the basic output of what we found.
    Serial.print(resultToHumanReadableBasic(&results));
    yield();  // Feed the WDT as the text output can take a while to print.

    if (results.decode_type == NEC) {
      if(results.value == 0xFFA25D) {  //POWER
        toggleLed();
        power = !power;
      } else if(results.value == 0xFF6897) {  //0
        setColor(0, 0, color.v);  //white
      } else if(results.value == 0xFF30CF) {   //1
//        setColor(HUE_RED, 255, color.v); //red
        setColor(0, 255, color.v); //red
      } else if(results.value == 0xFF18E7) {   //2
//        setColor(HUE_GREEN, 255, color.v); //green
        setColor(85, 255, color.v); //green
      } else if(results.value == 0xFF7A85) {   //3
//        setColor(HUE_BLUE, 255, color.v); //blue
        setColor(171, 255, color.v); //blue
      } else if(results.value == 0xFF906F) {   //+
        uint8_t v = color.v;
        v<<=1;
        if(v==0) v=255;
        setColor(color.h, color.s, v);
      } else if(results.value == 0xFFA857) {   //-
        uint8_t v = color.v;
        v>>=1;
        if(v==0) v=255;
        setColor(color.h, color.s, v);
      }
    }
  }

  if (power == 0) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    FastLED.delay(15);
    return;
  }

  fill_solid(leds, NUM_LEDS, solidColor);
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}
