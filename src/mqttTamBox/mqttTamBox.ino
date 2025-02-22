/*******************************************************************************************************************************
 *
 *   mqttTamBox v2.0.12
 *   copyright (c) Benny Tjäder
 *
 *   This program is free software: you can redistribute it and/or modify it under the terms of the
 *   GNU General Public License as published  by the Free Software Foundation, either version 3 of the License,
 *   or (at your option) any later version.
 *
 *    Uses and have been tested with the following libraries
 *      ArduinoJson           v7.3.0  by Benoit Blanchon     - https: *github.com/bblanchon/ArduinoJson
 *      PubSubClient          v2.8.0  by Nick O'Leary        - https: *github.com/pubsubclient.knolleary.net/
 *      IotWebConf            v3.2.1  by Balazs Kelemen      - https: *github.com/prampec/IotWebConf
 *      Keypad                v3.1.1  by Chris--A            - https: *github.com/Chris--A/Keypad
 *      Keypad_I2C            v2.3.1  by Joe Young           - https: *github.com/joeyoung/BensArduino-git (kanske byta till I2CKeypad)
 *      LiquidCrystal_PCF8574 v2.2.0  by Matthisa Hertel     - https: *github.com/mathertel/LiquidCrystal_PCF8574
 *
 *    This program is featuring the TAM Box.
 *    TAM Box is used to:
 *     - on sending side:   allocated a track between two stations and request to send a train.
 *     - on receiving side: accept/reject incoming request.
 *
 *    Each track direction is using key A-D. A and C is on left side, and B and D is on right side.
 *
 *    MQTT structure:
 *    message  /scale  /type  /node-id   /port-id    /order     payload
 *    cmd      /h0     /tam   /tambox-4  /a          /req       {json}        TAM request message
 *    cmd      /h0     /tam   /tambox-4  /b          /res       {json}        TAM response message
 *    dt       /h0     /tam   /tambox-4  /a                     {json}        TAM data message
 *    dt       /h0     /ping  /tambox-4                         {json}        Ping message
 *
 ********************************************************************************************************************************
 */
#include <ESP8266HTTPClient.h>                                // Library to handle HTTP download of config file
#include <ArduinoJson.h>                                      // Library to handle JSON objekts
#include <PubSubClient.h>                                     // Library to handle MQTT communication
#include <IotWebConf.h>                                       // Library to take care of client local settings
#include <IotWebConfMultipleWifi.h>                           // Library to handle multiple wifi networks
#include <IotWebConfUsing.h>                                  // This loads aliases for easier class names.
#ifdef ESP8266
  #include <ESP8266HTTPUpdateServer.h>                        // Library for Firmware update
#elif defined(ESP32)
// For ESP32 IotWebConf provides a drop-in replacement for UpdateServer.
  #include <IotWebConfESP32HTTPUpdateServer.h>                // Library for Firmware update
#endif
#include <Wire.h>                                             // Library to handle i2c communication
#include <Keypad_I2C.h>                                       // Library to handle i2c keypad
#include <LiquidCrystal_PCF8574.h>                            // Library to handle i2c LCD
#include <ArduinoOTA.h>                                       // Library for Over-the-Air programming
#include "mqttTamBox.h"                                       // Some of the client settings

// ------------------------------------------------------------------------------------------------------------------------------
// Method declarations.
void setDefaults(void);
void wifiConnected(void);
void setupBroker(void);
bool mqttConnect(void);
bool getConfigFile(void);
void sendPing(void);
void handleRoot(void);
void keyReceived(char key);
void handleInfo(uint8_t dest, uint8_t orderCode);
void handleDirection(uint8_t dest, uint8_t track, uint8_t orderCode);
void handleTrain(uint8_t dest, uint8_t track, uint8_t orderCode, uint16_t train);
void jsonReceived(uint8_t order, char* body);
void mqttJson(char* action, char* bodyType);
void printString(uint8_t str, uint8_t dest, uint16_t train);
void updateLcd(uint8_t dest);
void setDirString(uint8_t dest, uint8_t track);
void setNodeString(uint8_t dest, uint8_t track);
void fixSpecialChar(String text, uint8_t col, uint8_t row);
String removeEscapeChar(String text);
void toggleTrack(void);
String addBlanks(uint8_t blanks);
uint8_t centerText(String txt);
void beep(unsigned char duration, unsigned int freq);
void printSpecialChar(uint8_t character);

// Callback method declarations
void mqttCallback(char* topic, byte* payload, unsigned int length);
void configSaved(void);
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);

// ------------------------------------------------------------------------------------------------------------------------------
// Make objects for the PubSubClient
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ------------------------------------------------------------------------------------------------------------------------------
// Construct an LCD object and pass it the I2C address
LiquidCrystal_PCF8574 lcd(LCD_I2C_ADDR);

// ------------------------------------------------------------------------------------------------------------------------------
// Construct an Keypad object and pass it the I2C address
const byte ROWS                     = 4;                      // Four rows
const byte COLS                     = 4;                      // Four columns
char keys[ROWS][COLS]               = {{'1', '2', '3', 'A'},
                                       {'4', '5', '6', 'B'},
                                       {'7', '8', '9', 'C'},
                                       {'*', '0', '#', 'D'}};

// Digitran keypad, bit numbers of PCF8574 I/O port
byte rowPins[ROWS]                  = {4, 5, 6, 7};           // Connect to the row pinouts of the keypad
byte colPins[COLS]                  = {0, 1, 2, 3};           // Connect to the column pinouts of the keypad

Keypad_I2C Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEY_I2C_ADDR);

// ------------------------------------------------------------------------------------------------------------------------------
// IotWebConf variables set for the configuration web page

// Access point configuration
const char thingName[]              = "mqttTamBox";           // Initial AP name, used as SSID of the own Access Point
const char wifiInitialApPassword[]  = "tambox1234";           // Initial password, used to the own Access Point


// Device configuration
char cfgConfServer[DB_HOST_LEN];
char cfgConfFile[DB_CONFIGFILE_LEN];
char cfgLedBrightness[DB_NUMBER_LEN];
char cfgBackLight[DB_NUMBER_LEN];
char cfgLcdRows[DB_NUMBER_LEN];
char cfgLcdChar[DB_NUMBER_LEN];
char cfgTamTimeOut[DB_NUMBER_LEN];
char cfgDtShowTime[DB_NUMBER_LEN];
char cfgLanguage[DB_NUMBER_LEN];
const uint8_t languages                     = 2;
static char chooserValues[][DB_NUMBER_LEN]  = {"0", "1"};
static char chooserNames[][DB_NUMBER_LEN]   = {"Svenska", "English"};

// ------------------------------------------------------------------------------------------------------------------------------
// IotWebConfig variables

bool needMqttConnect                = false;
bool needReset                      = false;

// Make objects for IotWebConf
DNSServer dnsServer;
WebServer server(80);
#ifdef ESP8266
  ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
  HTTPUpdateServer httpUpdater;
#endif

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

iotwebconf::ChainedWifiParameterGroup chainedWifiParameterGroups[] = {
  iotwebconf::ChainedWifiParameterGroup("wifi1"),
  iotwebconf::ChainedWifiParameterGroup("wifi2")
};
iotwebconf::MultipleWifiAddition multipleWifiAddition(
  &iotWebConf,
  chainedWifiParameterGroups,
  sizeof(chainedWifiParameterGroups) / sizeof(chainedWifiParameterGroups[0]));

// ------------------------------------------------------------------------------------------------------------------------------
// Define settings to show up on configuration web page
// IotWebConfTextParameter(label, id, valueBuffer, length, type = "text", 
//                         placeholder = NULL, defaultValue = NULL,
//                         customHtml = NULL, visible = true)
    
// Add a new parameter section on the settings web page
IotWebConfParameterGroup webConfGroup       = IotWebConfParameterGroup("webConfGroup", "Configuration server settings");
IotWebConfTextParameter webConfServer       = IotWebConfTextParameter(
                                              "Configuration server host or IP-address", "webConfServer", cfgConfServer, DB_HOST_LEN, DB_CONFIG_HOST);
IotWebConfTextParameter webConfFile         = IotWebConfTextParameter(
                                              "Query string", "webConfFile", cfgConfFile, DB_CONFIGFILE_LEN, DB_CONFIG_FILE);

IotWebConfParameterGroup webDeviceGroup     = IotWebConfParameterGroup("webDeviceGroup", "Client settings");
IotWebConfNumberParameter webLedBrightness  = IotWebConfNumberParameter(
                                              "LED brightness", "webLedBrightness", cfgLedBrightness, DB_NUMBER_LEN, "125");
IotWebConfNumberParameter webBackLight      = IotWebConfNumberParameter(
                                              "LCD backlight", "webBackLight", cfgBackLight, DB_NUMBER_LEN, "128");
IotWebConfNumberParameter webLcdRows        = IotWebConfNumberParameter(
                                              "LCD number of rows", "webLcdRows", cfgLcdRows, DB_NUMBER_LEN, "2");
IotWebConfNumberParameter webLcdChar        = IotWebConfNumberParameter(
                                              "LCD number of characters / row", "webLcdChar", cfgLcdChar, DB_NUMBER_LEN, "16");
IotWebConfNumberParameter webTamTimeOut     = IotWebConfNumberParameter(
                                              "TAM request timeout (seconds)", "webTamTimeOut", cfgTamTimeOut, DB_NUMBER_LEN, "30");
IotWebConfNumberParameter webDtShowTime     = IotWebConfNumberParameter(
                                              "Time to show status messages (seconds)", "webDtShowTime", cfgDtShowTime, DB_NUMBER_LEN, "3");
IotWebConfSelectParameter webLanguage       = IotWebConfSelectParameter(
                                              "Tambox language", "webLanguage", cfgLanguage, DB_NUMBER_LEN, (char*)chooserValues, (char*)chooserNames, sizeof(chooserValues) / DB_NUMBER_LEN, DB_NUMBER_LEN);

// ------------------------------------------------------------------------------------------------------------------------------
// Name of the config server we want to get config from

char configHost[DB_CONFIGPATH_LEN];
const char* configPath;
const char* tmpMqttServer;

// Where received config are stored
String tamBoxMqtt[MQTT_PARAM];                                // [SERVER,PORT,USER,PASS,TOPIC]
String tamBoxConfig[CONFIG_DEST][CONFIG_PARAM];               // [DEST_A,DEST_B,DEST_C,DEST_D,OWN][ID,SIGN,NAME,NUMOFDEST,TRACKS,EXIT,TOTTRACKS,TYPE]

bool notReceivedConfig;
bool tamboxReady                    = false;

// ------------------------------------------------------------------------------------------------------------------------------
// Define MQTT topic variables
const byte NORETAIN                 = 0;                      // Used to publish topics as NOT retained
const byte RETAIN                   = 1;                      // Used to publish topics as retained

// ------------------------------------------------------------------------------------------------------------------------------
// Other variables

// Variables to be set after getting configuration from file
String clientID;
uint8_t lcdBackLight;
uint8_t buzzerPin                   = BUZZER_PIN;             // Define buzzerPin

uint8_t destination;
String trainNumber                  = "";

// Variables for Train direction and Train Ids used in topics
// For debuging
#ifdef DEBUG
const char* trainDirTxt[DIR_STATES]         = {OUT, IN};
const char* trackStateTxt[NUM_OF_STATES]    = {_NOTUSED_T,
                                               _IDLE_T,
                                               _TRAFDIR_T,
                                               _INREQUEST_T,
                                               _INACCEPT_T,
                                               _INTRAIN_T,
                                               _OUTREQUEST_T,
                                               _OUTACCEPT_T,
                                               _OUTTRAIN_T,
                                               _LOST_T};
#endif
const char* destIDTxt[NUM_OF_DEST_STRINGS]  = {DEST_A_T,
                                               DEST_B_T,
                                               DEST_C_T,
                                               DEST_D_T,
                                               DEST_OWN_STATION_T,
                                               DEST_A_RIGHT_T,
                                               DEST_B_RIGHT_T,
                                               DEST_C_RIGHT_T,
                                               DEST_D_RIGHT_T,
                                               DEST_CONFIG_T,
                                               DEST_ALL_DEST_T,
                                               DEST_NOT_SELECTED_T};

// For LCD destinations
String lcdString[DEST_BUTTONS][LCD_PARTS];                    // String for each destination with 3 positions

const char* useTrackTxt[DIR_STATES]                   = {LEFT, RIGHT};
uint16_t trainId[DEST_BUTTONS][MAX_NUM_OF_TRACKS];            // Store the train id per track and destination
uint8_t traffDir[DEST_BUTTONS][MAX_NUM_OF_TRACKS]     = {{DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}};
uint8_t lastTraffDir[DEST_BUTTONS][MAX_NUM_OF_TRACKS] = {{DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}};
const char* stringTxt[languages][LCD_STRINGS]         = {{LCD_TRAIN_T,          // TRAIN            Swedish
                                                          LCD_TRAINDIR_NOK_T,   // LCD_TRAINDIR_NOK
                                                          LCD_DEPATURE_T,       // LCD_DEPATURE
                                                          LCD_TAM_CANCEL_T,     // LCD_TAM_CANCEL
                                                          LCD_TAM_ACCEPT_T,     // LCD_TAM_ACCEPT
                                                          LCD_ARRIVAL_T,        // LCD_ARRIVAL
                                                          LCD_TAM_NOK_T,        // LCD_TAM_NOK
                                                          LCD_TAM_CANCELED_T,   // LCD_TAM_CANCELED
                                                          LCD_TAM_OK_T,         // LCD_TAM_OK
                                                          LCD_ARRIVAL_OK_T,     // LCD_ARRIVAL_OK
                                                          LCD_DEPATURE_OK_T},   // LCD_DEPATURE_OK
                                                         {LCD_TRAIN_TE,         // TRAIN            English
                                                          LCD_TRAINDIR_NOK_TE,  // LCD_TRAINDIR_NOK
                                                          LCD_DEPATURE_TE,      // LCD_DEPATURE
                                                          LCD_TAM_CANCEL_TE,    // LCD_TAM_CANCEL
                                                          LCD_TAM_ACCEPT_TE,    // LCD_TAM_ACCEPT
                                                          LCD_ARRIVAL_TE,       // LCD_ARRIVAL
                                                          LCD_TAM_NOK_TE,       // LCD_TAM_NOK
                                                          LCD_TAM_CANCELED_TE,  // LCD_TAM_CANCELED
                                                          LCD_TAM_OK_TE,        // LCD_TAM_OK
                                                          LCD_ARRIVAL_OK_TE,    // LCD_ARRIVAL_OK
                                                          LCD_DEPATURE_OK_TE}}; // LCD_DEPATURE_OK

const char* escapeChar              = "\xc3";                 // Character sent before special characters
// Variables to store actual states
uint8_t trackState[DEST_BUTTONS][MAX_NUM_OF_TRACKS];          // State per track and destination

// Store respond-to, session-id, port-id and desired state on sent and received cmd per destination
String resCmd[DEST_BUTTONS][6];                               // response cmd
String reqCmd[DEST_BUTTONS][6];                               // request cmd

// Store incoming data message if tambox is busy
uint16_t dtInQueue[DEST_BUTTONS][Q_DATA];                     // state, id, track, train

uint8_t destBtnPushed;
uint8_t currentTrack                = LEFT_TRACK;             // Used when toggling the track in LCD
uint8_t requestTrack                = LEFT_TRACK;             // Used when checking TAM timeout
bool tamBoxIdle                     = false;                  // Set tambox busy
bool showText                       = false;                  // Show information text string
unsigned int epochTime;                                       // For the timestamp in MQTT body
unsigned long pingTime;
unsigned long tamTime;
unsigned long beepPaus;
unsigned long showTime;
unsigned long toggleTime;

// Custom character for LCD
byte chr1[8] = {0x8, 0x4, 0xa, 0xd, 0xa, 0x4, 0x8, 0x0};      // Character >>
byte chr2[8] = {0x2, 0x4, 0xa, 0x16, 0xa, 0x4, 0x2, 0x0};     // Character <<
#ifdef NON_EU_CHAR_SET
byte chr3[8] = {0x4, 0x0, 0xe, 0x11, 0x1f, 0x11, 0x11, 0x0};  // Character Å
byte chr4[8] = {0xa, 0x0, 0xe, 0x11, 0x1f, 0x11, 0x11, 0x0};  // Character Ä
byte chr5[8] = {0xa, 0x0, 0xe, 0x11, 0x11, 0x11, 0xe, 0x0};   // Character Ö
byte chr6[8] = {0x4, 0x0, 0xe, 0x1, 0xf, 0x11, 0xf, 0x0};     // Character å
byte chr7[8] = {0xa, 0x0, 0xe, 0x1, 0xf, 0x11, 0xf, 0x0};     // Character ä
byte chr8[8] = {0xa, 0x0, 0xe, 0x11, 0x11, 0x11, 0xe, 0x0};   // Character ö
#endif

HTTPClient http;

/* ------------------------------------------------------------------------------------------------------------------------------
 *  Standard setup function
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setup() {

  // ----------------------------------------------------------------------------------------------------------------------------
  // Setup Arduino IDE serial monitor for "debugging"
#ifdef DEBUG_ESP_PORT
  Serial.begin(115200); Serial.println("");
#endif

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up the buzzer

  pinMode(buzzerPin, OUTPUT);                                 // Set buzzerPin as output

  // ----------------------------------------------------------------------------------------------------------------------------
  // IotWebConfig start
  
  // Adding items to each group
  webConfGroup.addItem(&webConfServer);                       // Configuration server hostname/IP-address
  webConfGroup.addItem(&webConfFile);                         // Configuration server query string
  webDeviceGroup.addItem(&webLedBrightness);                  // LED Brightness
  webDeviceGroup.addItem(&webBackLight);                      // LCD Backlight
  webDeviceGroup.addItem(&webLcdRows);                        // LCD number of rows
  webDeviceGroup.addItem(&webLcdChar);                        // LCD number of characters / row
  webDeviceGroup.addItem(&webTamTimeOut);                     // TAM time out in seconds
  webDeviceGroup.addItem(&webDtShowTime);                     // Show time for dt in seconds
  webDeviceGroup.addItem(&webLanguage);                       // Tambox language, Eng or Swe

  // Initializing the wifi configuration.
  multipleWifiAddition.init();                                // Support multiple Wifi in configuration
  
  iotWebConf.setWifiConnectionTimeoutMs(WIFI_TIMEOUT);        // Set Wifi timeout in ms, Default 30000 ms

  // Set status and config pin
  iotWebConf.setStatusPin(STATUS_PIN);
//  iotWebConf.setConfigPin(CONFIG_PIN);                      // Used to reset AP password

  // Adding up groups to show on config web page
  iotWebConf.addParameterGroup(&webConfGroup);                // Configuration Server Settings
  iotWebConf.addParameterGroup(&webDeviceGroup);              // Device Settings
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);

  // Validate data input
  iotWebConf.setFormValidator(&formValidator);

  // Show/set AP timeout on web page
  iotWebConf.getApTimeoutParameter()->visible = true;

  // Define how to handle updateServer calls.
  iotWebConf.setupUpdateServer(
    [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });

  // Setting default configuration
  bool validConfig = iotWebConf.init();

  if (!validConfig) {
    // Set configuration default values
    strcpy(configHost, DB_CONFIG_HOST);
    strcat(configHost, DB_CONFIG_FILE);
    clientID                        = String(thingName) + "-" + String(random(2147483647));
    clientID.toLowerCase();
    lcdBackLight                    = LCD_BACKLIGHT;
  }

  else {
    strcpy(configHost, cfgConfServer);
    strcat(configHost, cfgConfFile);
    clientID                        = String(iotWebConf.getThingName());
    clientID.toLowerCase();
    lcdBackLight                    = atoi(cfgBackLight);
  }

  notReceivedConfig = true;

#ifdef __ARDUINO_OTA_H
  // ----------------------------------------------------------------------------------------------------------------------------
  // Setup for OTA handling (Over-the-Air programming)

  // Port default to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname default to esp8266-[ChipID]
  ArduinoOTA.setHostname(iotWebConf.getThingName());

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    Serial.print(F("Start updating "));  Serial.println(type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  ArduinoOTA.begin();
  Serial.println(F("Ready"));
  // Serial.print(F("IP address: "));
  // Serial.println(WiFi.localIP());
#endif

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up LCD
  // The begin call takes the width and height. This
  // Should match the number provided to the constructor.

  lcd.begin(atoi(cfgLcdChar), atoi(cfgLcdRows));
  lcd.setBacklight(lcdBackLight);

  // Only 8 custom characters can be defined into the LCD
  lcd.createChar(TRAIN_MOVING_RIGHT, chr1);                   // Create character >>
  lcd.createChar(TRAIN_MOVING_LEFT, chr2);                    // Create character <<
#ifdef NON_EU_CHAR_SET
  lcd.createChar(SWE_CAP_Å, chr3);                            // Create character uppercase Å
  lcd.createChar(SWE_CAP_Ä, chr4);                            // Create character uppercase Ä
  lcd.createChar(SWE_CAP_Ö, chr5);                            // Create character uppercase Ö
  lcd.createChar(SWE_LOW_Å, chr6);                            // Create character lowercase å
  lcd.createChar(SWE_LOW_Ä, chr7);                            // Create character lowercase ä
  lcd.createChar(SWE_LOW_Ö, chr8);                            // Create character lowercase ö
#endif
  lcd.clear();
  lcd.setCursor(atoi(cfgLcdChar) / 2 - centerText(LCD_AP_MODE), LCD_FIRST_ROW);
  lcd.print(LCD_AP_MODE);
  lcd.setCursor(atoi(cfgLcdChar) / 2 - centerText(String(iotWebConf.getThingName())), LCD_SECOND_ROW);
  lcd.print(String(iotWebConf.getThingName()));


  // Set up required URL handlers for the config web pages
  server.on("/", handleRoot);
  server.on("/config", []{iotWebConf.handleConfig();});
  server.onNotFound([](){iotWebConf.handleNotFound();});

  delay(2000);                                                // Wait for IotWebServer to start network connection

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up i2c key board

  Keypad.begin();
    
  // ----------------------------------------------------------------------------------------------------------------------------
  // Set default values

  setDefaults();
  destination = DEST_NOT_SELECTED;
  toggleTime = pingTime = tamTime = beepPaus = showTime = millis();
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Main program loop
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void loop() {

#ifdef __ARDUINO_OTA_H
  // Handler for the OTA process
  ArduinoOTA.handle();
#endif

  // Check connection to the MQTT broker. If no connection, try to reconnect
  if (needMqttConnect) {
    if (mqttConnect()) {
      needMqttConnect = false;
    }
  }

  else if ((iotWebConf.getState() == iotwebconf::OnLine) && (!mqttClient.connected())) {
#ifdef DEBUG
    Serial.printf("%-16s: %d  MQTT reconnect\n", __func__, __LINE__);
#endif
    mqttConnect();
  }

  if (needReset) {
#ifdef DEBUG
    Serial.printf("%-16s: %d  Rebooting after 5 seconds.\n", __func__, __LINE__);
#endif
    lcd.clear();
    lcd.home();
    lcd.print(LCD_REBOOTING);
    iotWebConf.delay(5000);
    ESP.restart();
  }

  // Run repetitive jobs
  mqttClient.loop();                                                                            // Wait for incoming MQTT messages
  iotWebConf.doLoop();                                                                          // Check for IotWebConfig actions

  if (tamboxReady) {
    char key = Keypad.getKey();                                                                 // Get key input
    if (key) {
      beep(4, BEEP_KEY_CLK);                                                                    // Key click 4ms
      keyReceived(key);
    }

    if (showText && (millis() - showTime > atoi(cfgDtShowTime) * 1000)) {                       // Information text is shown
      showTime = millis();
      tamBoxIdle = true;                                                                        // Set tambox idle
#ifdef DEBUG
      Serial.printf("%-16s: %d  - tamBoxIdle set to true\n", __func__, __LINE__);
#endif
      showText = false;
      updateLcd(DEST_ALL_DEST);                                                                 // Restore the LCD
#ifdef DEBUG
      Serial.printf("%-16s: %d  - ShowText set to false\n", __func__, __LINE__);
#endif
    }

    if (tamBoxIdle && (millis() - showTime > atoi(cfgDtShowTime) * 1000)) {                     // Check the queue
      showTime = millis();
      for (uint8_t dest = 0; dest < DEST_BUTTONS; dest++) {
        if (dtInQueue[dest][Q_STATE] == Q_ACTIVE) {                                             // Check incoming dt queue
          dtInQueue[dest][Q_STATE] = Q_INACTIVE;                                                // Handle the queued message
#ifdef DEBUG
          Serial.printf("%-16s: %d  Queue for dest %s executed\n", __func__, __LINE__, destIDTxt[dest]);
#endif
          handleTrain(dest, dtInQueue[dest][Q_TRACK], dtInQueue[dest][Q_ORDERCODE], dtInQueue[dest][Q_TRAIN]);
          break;
        }
      }
    }

    if (tamBoxIdle && (millis() - toggleTime > TIME_TOGGLE_TRACK)) {                            // Toggle double track view
      toggleTime = millis();
      toggleTrack();
    }

    if (millis() - pingTime > TIME_PING_INTERVAL) {                                             // Send Ping
      pingTime = millis();
      sendPing();
    }

    for (uint8_t dest = 0; dest < DEST_BUTTONS; dest++) {
      if (trackState[dest][requestTrack] == _INREQUEST) {
        if (millis() - tamTime > atoi(cfgTamTimeOut) * 1000) {                                  // TAM request times out
          tamTime = millis();
#ifdef DEBUG
          Serial.printf("%-16s: %d  - tamTime set to millis()\n", __func__, __LINE__);
#endif
          keyReceived('*');                                                                     // Send reject when timed out
        }

        else if (millis() - beepPaus > TIME_BEEP_PAUS) {
          beepPaus = millis();
#ifdef DEBUG
          Serial.printf("%-16s: %d  - beepPaus set to millis()\n", __func__, __LINE__);
#endif
          beep(TIME_BEEP_DURATION, BEEP_OK);
        }
      }
    }
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 * Key received
 *
 *  procedurs
 *  Incoming request on normal track from destination A
 *    # Accept      OK
 *    * Reject      NOK
 *  Report train in on normal track from destination A
 *    A#
 *  Report train in on other track from destination C
 *    CC#
 *  Request outgoing train on normal track to destination C
 *    C{train no}
 *    # Confirm
 *    * Reject
 *  Request outgoing train on other track destination C
 *    CC{train no}
 *    # Confirm
 *    * Reject
 *  
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void keyReceived(char key) {

#ifdef DEBUG
  Serial.printf("%-16s: %d  Key pushed: %c\n", __func__, __LINE__, key);
  Serial.printf("%-16s: %d  Destination in: %s\n", __func__, __LINE__, destIDTxt[destination]);
#endif

  StaticJsonDocument<255> doc;                                                                  // Create a json object
  uint8_t maxSize = 255;
  uint8_t ownTrack = LEFT_TRACK;                                                                // Default single track traffic
  uint8_t destinationTrack = LEFT_TRACK;                                                        // Default single track traffic
  String portId;
  char body[254];
  char receiver[32];
  uint8_t check;
  unsigned int timestamp = epochTime + millis() / 1000;
  size_t n;

  doc[TAM][VERSION]   = LCP_BODY_VER;
  doc[TAM][TIMESTAMP] = timestamp;

  switch (key) {
    case '*':                                                                                   // NOK, Not accepted button pushed
      if (destination < DEST_CONFIG) {                                                          // If valid destination has been selected
        if (tamBoxConfig[destination][TRACKS].toInt() == DOUBLE_TRACK) {                        // If double track to destination
          destinationTrack = RIGHT_TRACK;
          if (trackState[destination][RIGHT_TRACK] == _INREQUEST ||                             // If right track state is in request
              trackState[destination][RIGHT_TRACK] == _INTRAIN) {                               // If right track status is incoming train
            ownTrack = RIGHT_TRACK;
          }
        }

#ifdef DEBUG
        Serial.printf("%-16s: %d  Destination: %s, State in: %s for %s track\n", __func__, __LINE__, destIDTxt[destination], trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
        Serial.printf("%-16s: %d  Destination selected %d times\n", __func__, __LINE__, destBtnPushed);
#endif
        switch (trackState[destination][ownTrack]) {                                            // Check track state
          case _INREQUEST:                                                                      // If incoming request
            if (tamBoxConfig[destination][TRACKS].toInt() == DOUBLE_TRACK) {
              ownTrack = (destinationTrack == RIGHT_TRACK) ? RIGHT_TRACK : LEFT_TRACK;
            }

            trackState[destination][ownTrack] = _IDLE;                                          // Set track state to idle

            doc[TAM][NODE_ID]                       = reqCmd[destination][LCP_NODE_ID];
            doc[TAM][PORT_ID]                       = reqCmd[destination][LCP_PORT_ID];
            doc[TAM][TRACK]                         = String(useTrackTxt[destinationTrack]);
            doc[TAM][TRAIN_ID]                      = trainId[destination][ownTrack];
            doc[TAM][SESSION_ID]                    = reqCmd[destination][LCP_SESSION_ID];
            doc[TAM][STATE][DESIRED]                = reqCmd[destination][LCP_DESIRED_STATE];
            doc[TAM][STATE][REPORTED]               = REJECTED;

            strcpy(receiver, reqCmd[destination][LCP_RESPOND_TO].c_str());
/*
 * char body[512];
 * serializeJson(doc, body);
 * mqttPublish("putTopic", body);
 * 
 * några CPU cycler snabbare om storleken på payload skickas till mqttPublish
 * char body[512];
 * aize_t n = serializeJson(doc, body);
 * mqttPublish("outTopic", body, n);
 */
            n = serializeJson(doc, body);                                                       // Create a json body
            check = mqttClient.publish(receiver, body, NORETAIN);                               // Publish a tam rejected message
#ifdef DEBUG
            if (check == 1) {
              Serial.printf("%-16s: %d  Publish a TAM rejected message with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
              Serial.printf("%-16s: %d  Publish: %s - %s\n", __func__, __LINE__, receiver, body);
            }

            else {
              Serial.printf("%-16s: %d  Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
            }
#endif
            trainId[destination][ownTrack] = DEST_TRAIN_0;                                      // Clear the train number
#ifdef DEBUG
            Serial.printf("%-16s: %d  State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
#endif
            tamBoxIdle = true;                                                                  // Set tambox busy
#ifdef DEBUG
            Serial.printf("%-16s: %d  - tamBoxIdle set to true\n", __func__, __LINE__);
#endif
          break;
//----------------------------------------------------------------------------------------------
          case _TRAFDIR:                                                                        // Cancel Traffic direction request
            trackState[destination][ownTrack] = _IDLE;                                          // Set track state to idle
#ifdef DEBUG
            Serial.printf("%-16s: %d  State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
#endif
          break;
//----------------------------------------------------------------------------------------------
          case _OUTREQUEST:                                                                     // Cancel Outgoing request
            tamBoxIdle = false;                                                                 // Set tambox busy
#ifdef DEBUG
            Serial.printf("%-16s: %d  - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
            trainNumber = "";
            trackState[destination][ownTrack] = _IDLE;                                          // Set track state to idle
            setDirString(destination, ownTrack);
#ifdef DEBUG
            Serial.printf("%-16s: %d  State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
#endif
            if (trainId[destination][ownTrack] != DEST_TRAIN_0) {
              resCmd[destination][LCP_NODE_ID]        = tamBoxConfig[destination][ID];
              resCmd[destination][LCP_PORT_ID]        = tamBoxConfig[destination][EXIT];
              resCmd[destination][LCP_SESSION_ID]     = "req:" + String(timestamp);
              resCmd[destination][LCP_RESPOND_TO]     = "cmd/" + tamBoxMqtt[SCALE] + "/" +
                                                        "tam/" + tamBoxConfig[OWN][ID] + "/" +
                                                         resCmd[destination][LCP_PORT_ID] + "/res";
              resCmd[destination][LCP_TRACK]          = String(useTrackTxt[destinationTrack]);
              resCmd[destination][LCP_DESIRED_STATE]  = CANCEL;

              doc[TAM][NODE_ID]                       = resCmd[destination][LCP_NODE_ID];
              doc[TAM][PORT_ID]                       = resCmd[destination][LCP_PORT_ID];
              doc[TAM][TRACK]                         = resCmd[destination][LCP_TRACK];
              doc[TAM][TRAIN_ID]                      = trainId[destination][ownTrack];
              doc[TAM][SESSION_ID]                    = resCmd[destination][LCP_SESSION_ID];
              doc[TAM][RESPOND_TO]                    = resCmd[destination][LCP_RESPOND_TO];
              doc[TAM][STATE][DESIRED]                = resCmd[destination][LCP_DESIRED_STATE];

              strcpy(receiver, COMMAND); strcat(receiver, "/");
              strcat(receiver, tamBoxMqtt[SCALE].c_str()); strcat(receiver, "/");
              strcat(receiver, TAM); strcat(receiver, "/");
              strcat(receiver, tamBoxConfig[destination][ID].c_str()); strcat(receiver, "/");
              strcat(receiver, tamBoxConfig[destination][EXIT].c_str()); strcat(receiver, "/");
              strcat(receiver, REQUEST);
              n = serializeJson(doc, body);                                                     // Create a json body

              check = mqttClient.publish(receiver, body, NORETAIN);                             // Publish a tam cancel message

#ifdef DEBUG
              if (check == 1) {
                Serial.printf("%-16s: %d  Publish a TAM cancel message with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
                Serial.printf("%-16s: %d  Publish: %s - %s\n", __func__, __LINE__, receiver, body);
              }

              else {
                Serial.printf("%-16s: %d  Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
              }
#endif
            }

            lcd.noBlink();
            lcd.noCursor();
            printString(LCD_TAM_CANCELED, destination, DEST_TRAIN_0);
            trainId[destination][ownTrack] = DEST_TRAIN_0;
            showText = true;                                                                    // Show info string
#ifdef DEBUG
            Serial.printf("%-16s: %d  - ShowText set to true\n", __func__, __LINE__);
#endif
            showTime = millis();
#ifdef DEBUG
            Serial.printf("%-16s: %d  - showTime set to millis()\n", __func__, __LINE__);
#endif
          break;
        }
//----------------------------------------------------------------------------------------------
        setNodeString(destination, ownTrack);
        updateLcd(DEST_ALL_DEST);                                                               // Restore the LCD
        destination = DEST_NOT_SELECTED;                                                        // Set destination not selected
      }

      else if (destination == DEST_NOT_SELECTED) {                                              // No destination selected
        tamBoxIdle = false;                                                                     // Set tambox busy
#ifdef DEBUG
        Serial.printf("%-16s: %d  - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
        updateLcd(OWN);                                                                         // Show own station id and name

        showText = true;                                                                        // Show the string
#ifdef DEBUG
        Serial.printf("%-16s: %d  - ShowText set to true\n", __func__, __LINE__);
#endif
      }

      showTime = millis();
#ifdef DEBUG
      Serial.printf("%-16s: %d  - showTime set to millis()\n", __func__, __LINE__);
#endif
      destBtnPushed = 0;
      toggleTime = millis();
    break;
//----------------------------------------------------------------------------------------------
    case '#':                                                                                   // OK, Accepted button pushed
      if (destination < DEST_CONFIG) {
        if (tamBoxConfig[destination][TRACKS].toInt() == DOUBLE_TRACK) {                        // If double track to destination
          destinationTrack = RIGHT_TRACK;
          if (trackState[destination][destinationTrack] == _INREQUEST ||                        // If left track state is incoming request
              trackState[destination][destinationTrack] == _INTRAIN) {                          // If left track state is incoming train
              ownTrack = RIGHT_TRACK;
            if (destBtnPushed > 1) {
              ownTrack = LEFT_TRACK;
              destinationTrack = RIGHT_TRACK;
            }
          }
        }
#ifdef DEBUG
        Serial.printf("%-16s: %d  Destination: %s, State in: %s for %s track\n", __func__, __LINE__, destIDTxt[destination], trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
        Serial.printf("%-16s: %d  Destination selected %d times\n", __func__, __LINE__, destBtnPushed);
#endif
        portId = String(destIDTxt[destination]);
        portId.toLowerCase();

        switch (trackState[destination][ownTrack]) {                                            // Check track state
          case _INTRAIN:                                                                        // If track state is incoming train
            if (tamBoxConfig[destination][TRACKS].toInt() == DOUBLE_TRACK) {
              ownTrack = (destinationTrack == RIGHT_TRACK) ? RIGHT_TRACK : LEFT_TRACK;
            }

            trackState[destination][ownTrack] = _IDLE;                                          // Set track state to idle

            doc[TAM][NODE_ID]                       = tamBoxConfig[OWN][ID];
            doc[TAM][PORT_ID]                       = portId;
            doc[TAM][TRACK]                         = String(useTrackTxt[destinationTrack]);
            doc[TAM][TRAIN_ID]                      = trainId[destination][ownTrack];
            doc[TAM][STATE][REPORTED]               = IN;

            strcpy(receiver, DATA); strcat(receiver, "/");
            strcat(receiver, tamBoxMqtt[SCALE].c_str()); strcat(receiver, "/");
            strcat(receiver, TAM); strcat(receiver, "/");
            strcat(receiver, tamBoxConfig[OWN][ID].c_str()); strcat(receiver, "/");
            strcat(receiver, portId.c_str());

            n = serializeJson(doc, body);                                                       // Create a json body
            check = mqttClient.publish(receiver, body, NORETAIN);                               // Publish a train in message

#ifdef DEBUG
            if (check == 1) {
              Serial.printf("%-16s: %d  Publish a train in message with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
              Serial.printf("%-16s: %d  Publish: %s - %s\n", __func__, __LINE__, receiver, body);
            }

            else {
              Serial.printf("%-16s: %d  Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
            }
#endif
            trainId[destination][ownTrack] = DEST_TRAIN_0;                                      // Clear the train number
#ifdef DEBUG
            Serial.printf("%-16s: %d  State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
#endif
          break;
//----------------------------------------------------------------------------------------------
          case _INREQUEST:                                                                      // If track state is incoming request
            if (tamBoxConfig[destination][TRACKS].toInt() == DOUBLE_TRACK) {
              ownTrack = (destinationTrack == RIGHT_TRACK) ? RIGHT_TRACK : LEFT_TRACK;
            }

            trackState[destination][ownTrack] = _INACCEPT;                                      // Set state to incoming accept

            doc[TAM][NODE_ID]                       = reqCmd[destination][LCP_NODE_ID];
            doc[TAM][PORT_ID]                       = reqCmd[destination][LCP_PORT_ID];
            doc[TAM][TRACK]                         = String(useTrackTxt[destinationTrack]);
            doc[TAM][TRAIN_ID]                      = trainId[destination][ownTrack];
            doc[TAM][SESSION_ID]                    = reqCmd[destination][LCP_SESSION_ID];
            doc[TAM][STATE][DESIRED]                = reqCmd[destination][LCP_DESIRED_STATE];
            doc[TAM][STATE][REPORTED]               = ACCEPTED;

            strcpy(receiver, reqCmd[destination][LCP_RESPOND_TO].c_str());

            n = serializeJson(doc, body);                                                       // Create a json body
            check = mqttClient.publish(receiver, body, NORETAIN);                               // Publish a tam accepted message
#ifdef DEBUG
            if (check == 1) {
              Serial.printf("%-16s: %d Publish a TAM accepted message with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
              Serial.printf("%-16s: %d Publish: %s - %s\n", __func__, __LINE__, receiver, body);
            }

            else {
              Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__,receiver, check);
            }
#endif
            tamBoxIdle = true;                                                                  // Set tambox idle
#ifdef DEBUG
            Serial.printf("%-16s: %d - tamBoxIdle set to true\n", __func__, __LINE__);
            Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
#endif
          break;
//----------------------------------------------------------------------------------------------
          case _OUTREQUEST:                                                                     // If state is Outgoing request
            trainId[destination][ownTrack] = trainNumber.toInt();                               // set train number
            lcd.noCursor();
            lcd.noBlink();
            trainNumber = "";
            resCmd[destination][LCP_NODE_ID]        = tamBoxConfig[destination][ID];
            resCmd[destination][LCP_PORT_ID]        = tamBoxConfig[destination][EXIT];
            resCmd[destination][LCP_SESSION_ID]     = "req:" + String(timestamp);
            resCmd[destination][LCP_RESPOND_TO]     = "cmd/" + tamBoxMqtt[SCALE] + "/" +
                                                      "tam/" + tamBoxConfig[OWN][ID] + "/" +
                                                       portId + "/res";
            resCmd[destination][LCP_TRACK]          = String(useTrackTxt[ownTrack]);
            resCmd[destination][LCP_DESIRED_STATE]  = ACCEPT;

            doc[TAM][NODE_ID]                       = resCmd[destination][LCP_NODE_ID];
            doc[TAM][PORT_ID]                       = resCmd[destination][LCP_PORT_ID];
            doc[TAM][TRACK]                         = String(useTrackTxt[destinationTrack]);
            doc[TAM][TRAIN_ID]                      = trainId[destination][ownTrack];
            doc[TAM][SESSION_ID]                    = resCmd[destination][LCP_SESSION_ID];
            doc[TAM][RESPOND_TO]                    = resCmd[destination][LCP_RESPOND_TO];
            doc[TAM][STATE][DESIRED]                = resCmd[destination][LCP_DESIRED_STATE];

            strcpy(receiver, COMMAND); strcat(receiver, "/");
            strcat(receiver, tamBoxMqtt[SCALE].c_str()); strcat(receiver, "/");
            strcat(receiver, TAM); strcat(receiver, "/");
            strcat(receiver, tamBoxConfig[destination][ID].c_str()); strcat(receiver, "/");
            strcat(receiver, tamBoxConfig[destination][EXIT].c_str()); strcat(receiver, "/");
            strcat(receiver, REQUEST);

            n = serializeJson(doc, body);                                                       // Create a json body
            check = mqttClient.publish(receiver, body, NORETAIN);                               // Publish a tam request message
#ifdef DEBUG
            if (check == 1) {
              Serial.printf("%-16s: %d Publish a TAM request message with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
              Serial.printf("%-16s: %d Publish: %s - %s\n", __func__, __LINE__, receiver, body);
            }

            else {
              Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
            }
#endif
            tamBoxIdle = true;                                                                  // Set tambox idle
#ifdef DEBUG
            Serial.printf("%-16s: %d - tamBoxIdle set to true\n", __func__, __LINE__);
#endif
          break;
//----------------------------------------------------------------------------------------------
          case _OUTACCEPT:                                                                      // If state is Outgoing request accepted
            trackState[destination][ownTrack] = _OUTTRAIN;                                      // Set state to outgoing train

            doc[TAM][NODE_ID]                       = tamBoxConfig[OWN][ID];
            doc[TAM][PORT_ID]                       = portId;
            doc[TAM][TRACK]                         = String(useTrackTxt[ownTrack]);
            doc[TAM][TRAIN_ID]                      = trainId[destination][ownTrack];
            doc[TAM][STATE][REPORTED]               = OUT;

            strcpy(receiver, DATA); strcat(receiver, "/");
            strcat(receiver, tamBoxMqtt[SCALE].c_str()); strcat(receiver, "/");
            strcat(receiver, TAM); strcat(receiver, "/");
            strcat(receiver, tamBoxConfig[OWN][ID].c_str()); strcat(receiver, "/");
            strcat(receiver, portId.c_str());

            n = serializeJson(doc, body);                                                       // Create a json body
            check = mqttClient.publish(receiver, body, NORETAIN);                               // Publish a train out message
#ifdef DEBUG
            if (check == 1) {
              Serial.printf("%-16s: %d Publish a train out message with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
              Serial.printf("%-16s: %d Publish: %s - %s\n", __func__, __LINE__, receiver, body);
            }

            else {
              Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
            }
#endif
#ifdef DEBUG
            Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
#endif
          break;
        }
//----------------------------------------------------------------------------------------------
        setDirString(destination, ownTrack);
        setNodeString(destination, ownTrack);
        updateLcd(DEST_ALL_DEST);                                                               // Restore the LCD
        destination = DEST_NOT_SELECTED;                                                        // Set destination not selected
#ifdef DEBUG
        Serial.printf("%-16s: %d Destination out %s set\n", __func__, __LINE__, destIDTxt[destination]);
#endif
      }

      else if (destination == DEST_NOT_SELECTED) {                                              // No destination selected
        tamBoxIdle = false;                                                                     // Set tambox busy
#ifdef DEBUG
        Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
        updateLcd(OWN);                                                                         // Show own station id and name
        showText = true;                                                                        // Show the string
#ifdef DEBUG
        Serial.printf("%-16s: %d - showText set to true\n", __func__, __LINE__);
#endif
      }

      showTime = millis();
#ifdef DEBUG
      Serial.printf("%-16s: %d - showTime set to millis()\n", __func__, __LINE__);
#endif
      destBtnPushed = 0;
      toggleTime = millis();
    break;
//----------------------------------------------------------------------------------------------
    case 'A':                                                                                   // Destination key pressed
    case 'B':
    case 'C':
    case 'D':
      switch (key) {
        case 'A':
          destination = DEST_A;                                                                 // Set destination A
        break;
//----------------------------------------------------------------------------------------------
        case 'B':
          destination = DEST_B;                                                                 // Set destination B
        break;
//----------------------------------------------------------------------------------------------
        case 'C':
          destination = DEST_C;                                                                 // Set destination C
        break;
//----------------------------------------------------------------------------------------------
        case 'D':
          destination = DEST_D;                                                                 // Set destination D
        break;
      }
//----------------------------------------------------------------------------------------------
      destBtnPushed++;                                                                          // Destination button pushed again
      if (showText) {                                                                           // If showText is set clear the LCD and update it
        showText = false;
#ifdef DEBUG
        Serial.printf("%-16s: %d - showText set to false\n", __func__, __LINE__);
#endif
        updateLcd(DEST_ALL_DEST);
      }
#ifdef DEBUG
      Serial.printf("%-16s: %d Destination: %s, Current state: %s for %s track\n", __func__, __LINE__, destIDTxt[destination], trackStateTxt[trackState[destination][LEFT_TRACK]], useTrackTxt[LEFT_TRACK]);
      Serial.printf("%-16s: %d Destination: %s, Current state: %s for %s track\n", __func__, __LINE__, destIDTxt[destination], trackStateTxt[trackState[destination][RIGHT_TRACK]], useTrackTxt[RIGHT_TRACK]);
      Serial.printf("%-16s: %d Destination selected %d times\n", __func__, __LINE__, destBtnPushed);
#endif
      if (tamBoxConfig[destination][TRACKS].toInt() == DOUBLE_TRACK) {                          // If double track to destination
        destinationTrack = RIGHT_TRACK;
        if (trackState[destination][destinationTrack] == _INTRAIN) {                            // If right track state is incoming train
          ownTrack = RIGHT_TRACK;
          destinationTrack = LEFT_TRACK;
          if (destBtnPushed == 1) {
            ownTrack = LEFT_TRACK;
            destinationTrack = RIGHT_TRACK;
#ifdef DEBUG
            Serial.printf("%-16s: %d Destination: %s track %s is in state %s\n", __func__, __LINE__, destIDTxt[destination], useTrackTxt[ownTrack], trackStateTxt[trackState[destination][ownTrack]]);
#endif
          }
        }
      }

      switch (trackState[destination][ownTrack]) {                                              // Check track state
        case _OUTREQUEST:                                                                       // If state is outgoing request
          tamBoxIdle = false;                                                                   // Set tambox busy
#ifdef DEBUG
          Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
          printString(LCD_TAM_CANCEL, destination, DEST_TRAIN_0);                               // Report cancel?
        break;
//----------------------------------------------------------------------------------------------
        case _INTRAIN:                                                                          // If state is incoming train
          tamBoxIdle = false;                                                                   // Set tambox busy
#ifdef DEBUG
          Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
          showText = true;
#ifdef DEBUG
          Serial.printf("%-16s: %d - showText set to true\n", __func__, __LINE__);
#endif
          printString(LCD_ARRIVAL, destination, DEST_TRAIN_0);                                  // Report arrival?
        break;
//----------------------------------------------------------------------------------------------
        case _OUTACCEPT:                                                                        // If state is outgoing request accepted
          tamBoxIdle = false;                                                                   // Set tambox busy
#ifdef DEBUG
          Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
          showText = true;
#ifdef DEBUG
          Serial.printf("%-16s: %d - showText set to true\n", __func__, __LINE__);
#endif
          printString(LCD_DEPATURE_OK, destination, DEST_TRAIN_0);                              // Report train out!
        break;
//----------------------------------------------------------------------------------------------
        case _IDLE:                                                                             // If track state is idle
          portId = String(destIDTxt[destination]);
          portId.toLowerCase();
          traffDir[destination][ownTrack] = DIR_OUT;

          resCmd[destination][LCP_NODE_ID]        = tamBoxConfig[destination][ID];
          resCmd[destination][LCP_PORT_ID]        = tamBoxConfig[destination][EXIT];
          resCmd[destination][LCP_SESSION_ID]     = "req:" + String(timestamp);
          resCmd[destination][LCP_RESPOND_TO]     = "cmd/" + tamBoxMqtt[SCALE] + "/" +
                                                    "tam/" + tamBoxConfig[OWN][ID] + "/" +
                                                     portId + "/res";
          resCmd[destination][LCP_TRACK]          = String(useTrackTxt[destinationTrack]);
          resCmd[destination][LCP_DESIRED_STATE]  = IN;

          doc[TAM][NODE_ID]                       = resCmd[destination][LCP_NODE_ID];
          doc[TAM][PORT_ID]                       = resCmd[destination][LCP_PORT_ID];
          doc[TAM][TRACK]                         = resCmd[destination][LCP_TRACK];
          doc[TAM][SESSION_ID]                    = resCmd[destination][LCP_SESSION_ID];
          doc[TAM][RESPOND_TO]                    = resCmd[destination][LCP_RESPOND_TO];
          doc[TAM][STATE][DESIRED]                = resCmd[destination][LCP_DESIRED_STATE];

          strcpy(receiver, COMMAND); strcat(receiver, "/");
          strcat(receiver, tamBoxMqtt[SCALE].c_str()); strcat(receiver, "/");
          strcat(receiver, TAM); strcat(receiver, "/");
          strcat(receiver, tamBoxConfig[destination][ID].c_str()); strcat(receiver, "/");
          strcat(receiver, tamBoxConfig[destination][EXIT].c_str()); strcat(receiver, "/");
          strcat(receiver, REQUEST);

          n = serializeJson(doc, body);                                                         // Create a json body
          check = mqttClient.publish(receiver, body, NORETAIN);                                 // Publish a direction in request
#ifdef DEBUG
            if (check == 1) {
              Serial.printf("%-16s: %d Publish a direction in request with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
              Serial.printf("%-16s: %d Publish: %s - %s\n", __func__, __LINE__, receiver, body);
            }

            else {
              Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
            }
#endif
          trackState[destination][ownTrack] = _TRAFDIR;                                         // Set state to direction selection
          tamBoxIdle = true;                                                                    // Set tambox idle
#ifdef DEBUG
          Serial.printf("%-16s: %d - tamBoxIdle set to true\n", __func__, __LINE__);
#endif
#ifdef DEBUG
          Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
          Serial.printf("%-16s: %d Traffic direction %s set\n", __func__, __LINE__, useTrackTxt[traffDir[destination][ownTrack]]);
#endif
        break;
//----------------------------------------------------------------------------------------------
        default:                                                                                // Do nothing
          destination = DEST_NOT_SELECTED;                                                      // Set destination not selected
          tamBoxIdle = true;                                                                    // Set tambox idle
#ifdef DEBUG
          Serial.printf("%-16s: %d - tamBoxIdle set to true\n", __func__, __LINE__);
#endif
        break;
      }

    break;
//----------------------------------------------------------------------------------------------
    default:                                                                                    // Number key pressed
      if (destination < DEST_NOT_SELECTED) {                                                    // A destination has been selected
        switch (trackState[destination][LEFT_TRACK]) {                                          // Check left track state
          case _OUTREQUEST:                                                                     // If left track state is outgoing request
            tamBoxIdle = false;                                                                 // Set tambox busy
#ifdef DEBUG
            Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
            trainNumber = trainNumber + String(key);                                            // add the key to the train number
            printString(LCD_TRAIN_ID, destination, trainNumber.toInt());                        // show the number on the LCD
#ifdef DEBUG
            Serial.printf("%-16s: %d Train number: %s\n", __func__, __LINE__, trainNumber);
#endif
          break;
        }
//----------------------------------------------------------------------------------------------
      }

      else if (destination == DEST_NOT_SELECTED) {                                              // No destination selected
        tamBoxIdle = false;                                                                     // Set tambox busy
#ifdef DEBUG
        Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
        updateLcd(OWN);                                                                         // Show own station id and name
        showText = true;                                                                        // Show the string
#ifdef DEBUG
        Serial.printf("%-16s: %d - showText set to true\n", __func__, __LINE__);
#endif
        showTime = millis();
#ifdef DEBUG
        Serial.printf("%-16s: %d - showTime set to millis()\n", __func__, __LINE__);
#endif
      }
    break;
  }
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
  if (destination != DEST_NOT_SELECTED) {
    Serial.printf("%-16s: %d Destination out %s set\n", __func__, __LINE__, destIDTxt[destination]);
    Serial.printf("%-16s: %d Destination: %s, New state: %s for %s track\n", __func__, __LINE__, destIDTxt[destination], trackStateTxt[trackState[destination][ownTrack]], useTrackTxt[ownTrack]);
  }
#endif
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function that gets called when a info message is received
 *
 *  handleInfo(dest, {CODE_READY, CODE_LOST})
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void handleInfo(uint8_t dest, uint8_t orderCode) {

#ifdef DEBUG
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d %s(dest: %d, orderCode: %d)\n", __func__, __LINE__, __func__, dest, orderCode);
#endif

  switch (orderCode) {
    case CODE_LOST:                                                                             // Connection lost
      for (uint8_t track = 0; track < MAX_NUM_OF_TRACKS; track++) {
        lastTraffDir[dest][track] = traffDir[dest][track];
        traffDir[dest][track] = DIR_LOST;

        if (trackState[dest][track] != _NOTUSED) {                                              // If track state is used
          trackState[dest][track] = _LOST;                                                      // Set track state to lost
#ifdef DEBUG
          Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[dest][track]], useTrackTxt[track]);
#endif
        }

        setDirString(dest, track);                                                              // Set lost direction sign
      }

      updateLcd(dest);                                                                          // Refresh the LCD
#ifdef DEBUG
      Serial.printf("%-16s: %d Connection lost to %s\n", __func__, __LINE__, destIDTxt[dest]);
#endif
    break;

    case CODE_READY:                                                                            // Connection restored
      for (uint8_t track = 0; track < MAX_NUM_OF_TRACKS; track++) {
        traffDir[dest][track] = lastTraffDir[dest][track];

        if (trackState[dest][track] != _NOTUSED) {                                              // If track state is used
          trackState[dest][track] = _IDLE;                                                      // Set track state to idle
#ifdef DEBUG
          Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[dest][track]], useTrackTxt[track]);
#endif
          trainId[dest][track] = DEST_TRAIN_0;                                                  // Clear train id
          setNodeString(dest, track);                                                           // Set idle text
          setDirString(dest, track);                                                            // Set normal direction sign
        }
      }
     updateLcd(dest);                                                                           // Refresh the LCD
#ifdef DEBUG
      Serial.printf("%-16s: %d Connection restored to %s\n", __func__, __LINE__, destIDTxt[dest]);
    break;

    default:
      Serial.printf("%-16s: %d Unsupported orderCode: %d\n", __func__, __LINE__, orderCode);
#endif
    break;
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function that gets called when a direction message is received
 *
 *  handleDirection(dest, {LEFT_TRACK, RIGHT_TRACK}, {CODE_TRAFDIR_REQ_IN, CODE_TRAFDIR_RES_IN, CODE_TRAFDIR_RES_OUT})
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void handleDirection(uint8_t dest, uint8_t receivedTrack, uint8_t orderCode) {

#ifdef DEBUG
  String setTo;
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d %s(dest: %d, track: %d, orderCode: %d)\n", __func__, __LINE__, __func__, dest, receivedTrack, orderCode);
#endif

  String portId;
  char receiver[32];                                                                            // mqtt message receiver
  char body[254];                                                                               // json body
  uint8_t check;
  uint8_t ownTrack = LEFT_TRACK;                                                                // Default single track traffic
  size_t n;

  StaticJsonDocument<255> doc;                                                                  // Create a json object
  uint8_t maxSize = 255;
  doc[TAM][VERSION]   = LCP_BODY_VER;
  doc[TAM][TIMESTAMP] = epochTime + millis() / 1000;

  switch (orderCode) {
    case CODE_TRAFDIR_REQ_IN:                                                                   // Incoming traffic direction change
      if (tamBoxConfig[dest][TRACKS].toInt() == DOUBLE_TRACK) {
        ownTrack = (receivedTrack == RIGHT_TRACK) ? RIGHT_TRACK : LEFT_TRACK;
      }
#ifdef DEBUG
      Serial.printf("%-16s: %d Destination: %s, Current state: %s for %s track\n", __func__, __LINE__, destIDTxt[dest], trackStateTxt[trackState[dest][ownTrack]], useTrackTxt[ownTrack]);
#endif
      switch (trackState[dest][ownTrack]) {
        case _IDLE:                                                                             // Track idle
          if (tamBoxIdle) {                                                                     // Tambox idle, acknowledge direction in
            traffDir[dest][ownTrack] = DIR_IN;                                                  // Set traffic direction to in
            setDirString(dest, ownTrack);
            updateLcd(dest);

            doc[TAM][NODE_ID]         = reqCmd[dest][LCP_NODE_ID];
            doc[TAM][PORT_ID]         = reqCmd[dest][LCP_PORT_ID];
            doc[TAM][TRACK]           = String(useTrackTxt[receivedTrack]);
            doc[TAM][SESSION_ID]      = reqCmd[dest][LCP_SESSION_ID];
            doc[TAM][STATE][DESIRED]  = reqCmd[dest][LCP_DESIRED_STATE];
            doc[TAM][STATE][REPORTED] = IN;                                                     // in = accepted

            strcpy(receiver, reqCmd[dest][LCP_RESPOND_TO].c_str());

            n = serializeJson(doc, body);                                                       // Create a json body
            check = mqttClient.publish(receiver, body, NORETAIN);                               // Publish direction change accepted
#ifdef DEBUG
            if (check == 1) {
              Serial.printf("%-16s: %d Publish direction change accepted with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
              Serial.printf("%-16s: %d Publish: %s - %s\n", __func__, __LINE__, receiver, body);
            }

            else {
              Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
            }
            setTo = String(trainDirTxt[traffDir[dest][ownTrack]]);
#endif
          }

          else {                                                                                // Tambox busy, reject direction in
            traffDir[dest][ownTrack] = DIR_OUT;                                                 // Set traffic direction to out

            doc[TAM][NODE_ID]         = reqCmd[dest][LCP_NODE_ID];
            doc[TAM][PORT_ID]         = reqCmd[dest][LCP_PORT_ID];
            doc[TAM][TRACK]           = String(useTrackTxt[receivedTrack]);
            doc[TAM][SESSION_ID]      = reqCmd[dest][LCP_SESSION_ID];
            doc[TAM][STATE][DESIRED]  = reqCmd[dest][LCP_DESIRED_STATE];
            doc[TAM][STATE][REPORTED] = OUT;                                                    // out = rejected

            strcpy(receiver, reqCmd[dest][LCP_RESPOND_TO].c_str());

            n = serializeJson(doc, body);                                                       // Create a json body
            check = mqttClient.publish(receiver, body, NORETAIN);                               // Publish direction change accepted
#ifdef DEBUG
            if (check == 1) {
              Serial.printf("%-16s: %d Publish direction change rejected with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
              Serial.printf("%-16s: %d Publish: %s - %s\n", __func__, __LINE__, receiver, body);
            }

            else {
              Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
            }
            setTo = String(trainDirTxt[traffDir[dest][ownTrack]]);
#endif
          }
        break;
//----------------------------------------------------------------------------------------------
        case _OUTREQUEST:                                                                       // If track state is outgoing request
          doc[TAM][NODE_ID]         = reqCmd[dest][LCP_NODE_ID];
          doc[TAM][PORT_ID]         = reqCmd[dest][LCP_PORT_ID];
          doc[TAM][TRACK]           = String(useTrackTxt[receivedTrack]);
          doc[TAM][SESSION_ID]      = reqCmd[dest][LCP_SESSION_ID];
          doc[TAM][STATE][DESIRED]  = reqCmd[dest][LCP_DESIRED_STATE];
          doc[TAM][STATE][REPORTED] = OUT;                                                      // out = rejected

          strcpy(receiver, reqCmd[dest][LCP_RESPOND_TO].c_str());

          n = serializeJson(doc, body);                                                         // Create a json body
          check = mqttClient.publish(receiver, body, NORETAIN);                                 // Publish direction change accepted
#ifdef DEBUG
          if (check == 1) {
            Serial.printf("%-16s: %d Publish direction change rejected with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
            Serial.printf("%-16s: %d Publish: %s - %s\n", __func__, __LINE__, receiver, body);
          }

          else {
            Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
          }
#endif
        break;
#ifdef DEBUG
        Serial.printf("%-16s: %d Traffic direction from %s on track %s set to %s\n", __func__, __LINE__, destIDTxt[dest], useTrackTxt[ownTrack], setTo);
#endif
      }

    break;
//----------------------------------------------------------------------------------------------
    case CODE_TRAFDIR_RES_IN:                                                                   // Direction change accepted
      if (tamBoxConfig[dest][TRACKS].toInt() == DOUBLE_TRACK) {
        ownTrack = (receivedTrack == RIGHT_TRACK) ? LEFT_TRACK : RIGHT_TRACK;
      }
#ifdef DEBUG
      Serial.printf("%-16s: %d Destination: %s, Current state: %s for %s track\n", __func__, __LINE__, destIDTxt[dest], trackStateTxt[trackState[dest][ownTrack]], useTrackTxt[ownTrack]);
#endif
      if (trackState[dest][ownTrack] == _TRAFDIR) {
        tamBoxIdle = false;                                                                     // Set tambox busy
#ifdef DEBUG
        Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
        trackState[dest][ownTrack] = _OUTREQUEST;                                               // Set track state to outgoing request
#ifdef DEBUG
        Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[dest][ownTrack]], useTrackTxt[ownTrack]);
#endif
        printString(LCD_TRAIN, dest, DEST_TRAIN_0);
#ifdef DEBUG
        setTo = String(OUT);
#endif
      }
    break;

    case CODE_TRAFDIR_RES_OUT:                                                                  // Direction change rejected
      traffDir[dest][ownTrack] = DIR_OUT;                                                       // Set traffic direction to out
      setDirString(dest, ownTrack);
      tamBoxIdle = false;                                                                       // Set tambox busy
#ifdef DEBUG
      Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
      trackState[dest][ownTrack] = _IDLE;                                                       // Set track state to idle
#ifdef DEBUG
       Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[dest][ownTrack]], useTrackTxt[ownTrack]);
#endif
      printString(LCD_TRAINDIR_NOK, dest, DEST_TRAIN_0);
      beep(TIME_BEEP_DURATION, BEEP_NOK);                                                       // Beep nok
      showText = true;                                                                          // Show the string
#ifdef DEBUG
      Serial.printf("%-16s: %d - showText set to true\n", __func__, __LINE__);
#endif
      showTime = millis();
#ifdef DEBUG
      Serial.printf("%-16s: %d - showTime set to millis()\n", __func__, __LINE__);
      setTo = String(trainDirTxt[traffDir[dest][ownTrack]]);
    break;

    default:
      Serial.printf("%-16s: %d Unsupported orderCode: %d", __func__, __LINE__, orderCode);
#endif
    break;
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function that gets called when a message with train number is received
 *
 *  handleTrain(dest, {LEFT_TRACK,RIGHT_TRACK}, {CODE_ACCEPT,CODE_ACCEPTED,CODE_REJECTED,CODE_CANCELED,CODE_TRAIN_IN,CODE_TRAIN_OUT} ,train)
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void handleTrain(uint8_t dest, uint8_t receivedTrack, uint8_t orderCode, uint16_t train) {

#ifdef DEBUG
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d %s(dest: %d, receivedTrack: %d, orderCode: %d, train: %d)\n", __func__, __LINE__, __func__, dest, receivedTrack, orderCode, train);
  uint8_t maxSize = 255;
#endif

  StaticJsonDocument<255> doc;                                                                  // Create a json object
  char receiver[32];                                                                            // mqtt message receiver
  char body[254];                                                                               // json body
  uint8_t check;
  size_t n;
  uint8_t ownTrack = LEFT_TRACK;                                                                // Default single track traffic

  if (orderCode == CODE_CANCEL) {                                                               // Incoming cancel, overrides busy tambox
    switch (trackState[dest][ownTrack]) {                                                       // Check track state
      case _INREQUEST:                                                                          // If track state is incoming request
        if (trainId[dest][ownTrack] == train) {
#ifdef DEBUG
          Serial.printf("%-16s: %d Incoming request from: %s with train %d canceled\n", __func__, __LINE__, destIDTxt[dest], train);
#endif
          trainId[dest][ownTrack] = DEST_TRAIN_0;
          tamBoxIdle = false;                                                                   // Set tambox busy
#ifdef DEBUG
          Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
          trackState[dest][ownTrack] = _IDLE;                                                   // Set track state to idle
#ifdef DEBUG
          Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[dest][ownTrack]], useTrackTxt[ownTrack]);
#endif
          traffDir[dest][ownTrack] = DIR_IN;                                                    // Set traffic direction to in
          setDirString(dest, ownTrack);
          updateLcd(dest);

          doc[TAM][VERSION]         = LCP_BODY_VER;
          doc[TAM][TIMESTAMP]       = epochTime + millis() / 1000;
          doc[TAM][NODE_ID]         = reqCmd[dest][LCP_NODE_ID];
          doc[TAM][PORT_ID]         = reqCmd[dest][LCP_PORT_ID];
          doc[TAM][TRACK]           = String(useTrackTxt[receivedTrack]);
          doc[TAM][TRAIN_ID]        = train;
          doc[TAM][SESSION_ID]      = reqCmd[dest][LCP_SESSION_ID];
          doc[TAM][STATE][DESIRED]  = reqCmd[dest][LCP_DESIRED_STATE];
          doc[TAM][STATE][REPORTED] = CANCELED;                                                 // reply canceled

          strcpy(receiver, reqCmd[dest][LCP_RESPOND_TO].c_str());

          n = serializeJson(doc, body);                                                         // Create a json body
          check = mqttClient.publish(receiver, body, NORETAIN);                                 // Publish direction change accepted
#ifdef DEBUG
          if (check == 1) {
            Serial.printf("%-16s: %d Publish direction change accepted with body size: %d (%d to max size)\n", __func__, __LINE__, n, maxSize - n);
            Serial.printf("%-16s: %d Publish: %s - %s\n", __func__, __LINE__, receiver, body);
          }

          else {
            Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
          }
#endif
          printString(LCD_TAM_CANCELED, destination, DEST_TRAIN_0);                             // Set string to tam canceled
          beep(TIME_BEEP_DURATION, BEEP_NOK);                                                   // Beep not ok
          showText = true;                                                                      // Show the string
#ifdef DEBUG
          Serial.printf("%-16s: %d - ShowText set to true\n", __func__, __LINE__);
#endif
          setNodeString(dest, ownTrack);
          setDirString(dest, ownTrack);
          showTime = millis();
#ifdef DEBUG
          Serial.printf("%-16s: %d - showTime set to millis()\n", __func__, __LINE__);
#endif
        }
#ifdef DEBUG
        else {
          Serial.printf("%-16s: %d Wrong train for incoming cancel: %d\n", __func__, __LINE__, train);
        }
#endif
      break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
      default:                                                                                  // Do nothing
        Serial.printf("%-16s: %d Wrong state for incoming cancel\n", __func__, __LINE__);
      break;
#endif
    }
  }
//----------------------------------------------------------------------------------------------

  else if (tamBoxIdle) {
    destination = dest;                                                                         // Save dest

    if (tamBoxConfig[dest][TRACKS].toInt() == DOUBLE_TRACK) {
      ownTrack = (receivedTrack == RIGHT_TRACK) ? LEFT_TRACK : RIGHT_TRACK;
    }
#ifdef DEBUG
    Serial.printf("%-16s: %d Destination: %s\n", __func__, __LINE__, destIDTxt[dest]);
#endif

    switch (orderCode) {
      case CODE_ACCEPT:                                                                         // Incoming request
        if (tamBoxConfig[dest][TRACKS].toInt() == DOUBLE_TRACK) {
          ownTrack = (receivedTrack == RIGHT_TRACK) ? RIGHT_TRACK : LEFT_TRACK;
        }

        switch (trackState[dest][ownTrack]) {                                                   // Check track state
          case _IDLE:                                                                           // If track state is idle
            trainId[dest][ownTrack] = train;
#ifdef DEBUG
            Serial.printf("%-16s: %d Incoming request from: %s with train %d on %s track\n", __func__, __LINE__, destIDTxt[dest], trainId[dest][ownTrack], useTrackTxt[ownTrack]);
#endif
            tamBoxIdle = false;                                                                 // Set tambox busy
#ifdef DEBUG
            Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
            trackState[dest][ownTrack] = _INREQUEST;                                            // Set track state to incoming request
#ifdef DEBUG
            Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[dest][ownTrack]], useTrackTxt[ownTrack]);
#endif
            setDirString(dest, ownTrack);
            setNodeString(dest, ownTrack);
            updateLcd(dest);
            printString(LCD_TAM_ACCEPT, dest, DEST_TRAIN_0);                                    // Set string to tam accept?
            requestTrack = receivedTrack;                                                       // Used for checking TAM timeout
            tamTime = beepPaus = millis();                                                      // Used for checking TAM timeout
#ifdef DEBUG
            Serial.printf("%-16s: %d - tamTime set to millis()\n", __func__, __LINE__);
#endif
            beep(TIME_BEEP_DURATION, BEEP_OK);
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:                                                                              // Do nothing
            Serial.printf("%-16s: %d Wrong state for incoming request\n", __func__, __LINE__);
          break;
#endif
        }
      break;

      case CODE_ACCEPTED:                                                                       // incoming response
        switch (trackState[dest][ownTrack]) {                                                   // Check track state
          case _OUTREQUEST:                                                                     // If track state outgoing request
            if (trainId[dest][ownTrack] == train) {
#ifdef DEBUG
              Serial.printf("%-16s: %d Outgoing request to: %s with train %d accepted\n", __func__, __LINE__, destIDTxt[dest], trainId[dest][ownTrack]);
#endif
              tamBoxIdle = false;                                                               // Set tambox busy
#ifdef DEBUG
              Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
              trackState[dest][ownTrack] = _OUTACCEPT;                                          // Set track state to outgoing accept
#ifdef DEBUG
              Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[dest][ownTrack]], useTrackTxt[ownTrack]);
#endif
              printString(LCD_TAM_OK, dest, DEST_TRAIN_0);                                      // Set string to tam accepted
              beep(TIME_BEEP_DURATION, BEEP_OK);                                                // Beep ok
              showText = true;                                                                  // Show the string
#ifdef DEBUG
              Serial.printf("%-16s: %d - ShowText set to true\n", __func__, __LINE__);
#endif
              setDirString(dest, ownTrack);
              showTime = millis();
#ifdef DEBUG
              Serial.printf("%-16s: %d - showTime set to millis()\n", __func__, __LINE__);
#endif
            }
#ifdef DEBUG
            else {
              Serial.printf("%-16s: %d Wrong train for incoming accept: %d\n", __func__, __LINE__, train);
            }
#endif
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:                                                                              // Do nothing
            Serial.printf("%-16s: %d Wrong state for incoming accept\n", __func__, __LINE__);
          break;
#endif
        }
//----------------------------------------------------------------------------------------------
      break;

      case CODE_REJECTED:                                                                       // Incoming reject
        switch (trackState[dest][ownTrack]) {                                                   // Check track state
          case _OUTREQUEST:                                                                     // If track state is outgoing request
            if (trainId[dest][ownTrack] == train) {
#ifdef DEBUG
              Serial.printf("%-16s: %d Outgoing request to: %s with train %d rejected\n", __func__, __LINE__, destIDTxt[dest], train);
#endif
              trainId[dest][ownTrack] = DEST_TRAIN_0;
              tamBoxIdle = false;                                                               // Set tambox busy
#ifdef DEBUG
              Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
              trackState[dest][ownTrack] = _IDLE;                                               // Set track state to idle
#ifdef DEBUG
              Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[dest][ownTrack]], useTrackTxt[ownTrack]);
#endif
              printString(LCD_TAM_NOK, dest, DEST_TRAIN_0);                                     // Set string to tam rejected
              beep(TIME_BEEP_DURATION, BEEP_NOK);                                               // Beep not ok
              showText = true;                                                                  // Show the string
#ifdef DEBUG
              Serial.printf("%-16s: %d - ShowText set to true-\n", __func__, __LINE__);
#endif
              setDirString(dest, ownTrack);
              setNodeString(dest, ownTrack);
              showTime = millis();
#ifdef DEBUG
              Serial.printf("%-16s: %d - showTime set to millis()\n", __func__, __LINE__);
#endif
            }
#ifdef DEBUG
            else {
              Serial.printf("%-16s: %d Wrong train for incoming reject: %d\n", __func__, __LINE__, train);
            }
#endif
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:                                                                              // Do nothing
            Serial.printf("%-16s: %d Wrong state for incoming reject\n", __func__, __LINE__);
          break;
#endif
        }
//----------------------------------------------------------------------------------------------
      break;

      case CODE_TRAIN_IN:                                                                       // Incoming arrival report
        if (tamBoxConfig[dest][TRACKS].toInt() == DOUBLE_TRACK) {
          ownTrack = (receivedTrack == RIGHT_TRACK) ? LEFT_TRACK : RIGHT_TRACK;
        }

        switch (trackState[dest][ownTrack]) {                                                   // Check track state
          case _OUTTRAIN:                                                                       // If track state is outgoing train
            if (trainId[dest][ownTrack] == train) {
              trainId[dest][ownTrack] = DEST_TRAIN_0;
#ifdef DEBUG
              Serial.printf("%-16s: %d Outgoing train to: %s with train %d arrived\n", __func__, __LINE__, destIDTxt[dest], train);
#endif
              tamBoxIdle = false;                                                               // Set tambox busy
#ifdef DEBUG
              Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif
              trackState[dest][ownTrack] = _IDLE;                                               // Set track state to idle
#ifdef DEBUG
              Serial.printf("%-16s: %d State changed to %s for %s track!\n", __func__, __LINE__, trackStateTxt[trackState[dest][ownTrack]], useTrackTxt[ownTrack]);
#endif
              printString(LCD_ARRIVAL_OK, dest, DEST_TRAIN_0);                                  // Set string to train arrived
              beep(TIME_BEEP_DURATION, BEEP_OK);                                                // Beep ok
              showText = true;                                                                  // Show the string
#ifdef DEBUG
              Serial.printf("%-16s: %d - ShowText set to true\n", __func__, __LINE__);
#endif
              setNodeString(dest, ownTrack);
              setDirString(dest, ownTrack);
              showTime = millis();
#ifdef DEBUG
              Serial.printf("%-16s: %d - showTime set to millis()\n", __func__, __LINE__);
#endif
            }
#ifdef DEBUG
            else {
              Serial.printf("%-16s: %d Wrong train for incoming arrival report: %d\n", __func__, __LINE__, train);
            }
#endif
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:                                                                              // Do nothing
            Serial.printf("%-16s: %d Wrong state for incoming arrival report\n", __func__, __LINE__);
          break;
#endif
        }
//----------------------------------------------------------------------------------------------
      break;

      case CODE_TRAIN_OUT:                                                                      // Train out report
        if (tamBoxConfig[dest][TRACKS].toInt() == DOUBLE_TRACK) {
          ownTrack = (receivedTrack == RIGHT_TRACK) ? LEFT_TRACK : RIGHT_TRACK;
        }

        switch (trackState[dest][ownTrack]) {                                                   // Check track state
          case _INACCEPT:                                                                       // If track state is incoming accept
            if (trainId[dest][ownTrack] == train) {
              tamBoxIdle = false;                                                               // Set tambox busy
#ifdef DEBUG
              Serial.printf("%-16s: %d - tamBoxIdle set to false\n", __func__, __LINE__);
#endif

              trackState[dest][ownTrack] = _INTRAIN;                                            // Set track state to incoming train
              printString(LCD_DEPATURE_OK, dest, DEST_TRAIN_0);                                 // Set string to train out
              beep(TIME_BEEP_DURATION, BEEP_OK);                                                // Beep ok
              showText = true;                                                                  // Show the string
#ifdef DEBUG
              Serial.printf("%-16s: %d - ShowText set to true\n", __func__, __LINE__);
#endif
              setNodeString(dest, ownTrack);
              setDirString(dest, ownTrack);
              showTime = millis();
#ifdef DEBUG
              Serial.printf("%-16s: %d - showTime set to millis()\n", __func__, __LINE__);
#endif
            }
#ifdef DEBUG
            else {
              Serial.printf("%-16s: %d Wrong train for train out report: %d\n", __func__, __LINE__, train);
            }
#endif
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:                                                                              // Do nothing
            Serial.printf("%-16s: %d Wrong state for train out report\n", __func__, __LINE__);
          break;
#endif
        }

      break;
#ifdef DEBUG
      default:
        Serial.printf("%-16s: %d Unsupported orderCode: %d\n", __func__, __LINE__, orderCode);
      break;
#endif
    }
  }

  else {
    dtInQueue[dest][Q_STATE]      = Q_ACTIVE;                                                   // Set queue active
    dtInQueue[dest][Q_TRACK]      = receivedTrack;                                              // store values for current destination
    dtInQueue[dest][Q_ORDERCODE]  = orderCode;
    dtInQueue[dest][Q_TRAIN]      = train;
#ifdef DEBUG
    Serial.printf("%-16s: %d Incoming report queued for destination: %s\n", __func__, __LINE__, destIDTxt[dest]);
#endif
    showTime = millis();
#ifdef DEBUG
    Serial.printf("%-16s: %d - showTime set to millis()\n", __func__, __LINE__);
#endif
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Set string
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void printString(uint8_t str, uint8_t dest, uint16_t train) {

#ifdef DEBUG
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d %s(str: %d, dest: %d, train: %d)\n", __func__, __LINE__, __func__, str, dest, train);
#endif

  uint8_t cRow;                                                                                 // Clear row
  uint8_t cCol;                                                                                 // Clear stringpos start
  uint8_t iRow;                                                                                 // Insert row
  String text;
  bool escaped    = false;                                                                      // Set no escape needed

  if (str < 11) {
    text = String(stringTxt[atoi(cfgLanguage)][str]);
#ifdef NON_EU_CHAR_SET
    // Tempfix, when received unicode is not printed correctly
    // on the LCD with asian character set
    if (text.indexOf(String(escapeChar)) >= 0) {                                                // Special character in the string
      text    = removeEscapeChar(text);
      escaped = true;                                                                           // Escape character needed
    }
#endif
  }

  switch (dest) {
    case DEST_A:                                                                                // Destination on left side
      cRow    = LCD_FIRST_ROW;                                                                  // Destination on first row
      cCol    = atoi(cfgLcdChar) / 2;
      iRow    = (atoi(cfgLcdRows) == 4) ? LCD_FOURTH_ROW : LCD_SECOND_ROW;                      // Check if it is a four row LCD
    break;
//----------------------------------------------------------------------------------------------
    case DEST_B:                                                                                // Destination on right side
      cRow    = LCD_FIRST_ROW;                                                                  // Destination on first row
      cCol    = LCD_FIRST_COL;
      iRow    = (atoi(cfgLcdRows) == 4) ? LCD_FOURTH_ROW : LCD_SECOND_ROW;                      // Check if it is a four row LCD
    break;
//----------------------------------------------------------------------------------------------
    case DEST_C:                                                                                // Destination on left side
      cRow    = (atoi(cfgLcdRows) == 4) ? LCD_THIRD_ROW : LCD_SECOND_ROW;                       // Check if it is a four row LCD
      cCol    = atoi(cfgLcdChar) / 2;
      iRow    = LCD_FIRST_ROW;                                                                  // Info text on first row
    break;
//----------------------------------------------------------------------------------------------
    case DEST_D:                                                                                // Destination on right side
      cRow    = (atoi(cfgLcdRows) == 4) ? LCD_THIRD_ROW : LCD_SECOND_ROW;                       // Check if it is a four row LCD
      cCol    = LCD_FIRST_COL;
      iRow    = LCD_FIRST_ROW;                                                                  // Info text on first row
    break;
//----------------------------------------------------------------------------------------------
  }
  switch (str) {
    case LCD_TRAIN_ID:                                                                          // Train number
      text  = String(stringTxt[atoi(cfgLanguage)][LCD_TRAIN]);
      if (text.indexOf(String(escapeChar)) >= 0) {                                              // Special character in the string
        text = removeEscapeChar(text);
      }

      if (text.length() < atoi(cfgLcdChar) / 2) {
        lcd.setCursor(atoi(cfgLcdChar) / 2 - centerText(text) + text.length(), iRow);
      }

      else {
        lcd.setCursor(LCD_FIRST_COL + text.length(), iRow);
      }

      lcd.print(String(train));
    break;
//----------------------------------------------------------------------------------------------
    case LCD_TRAIN:                                                                             // Train string
      lcd.setCursor(cCol, cRow);
      lcd.print(addBlanks(atoi(cfgLcdChar) / 2));                                               // Clear the position
      lcd.setCursor(LCD_FIRST_COL, iRow);
      lcd.print(addBlanks(atoi(cfgLcdChar)));                                                   // Clear the row
      if (text.length() < atoi(cfgLcdChar) / 2) {
        lcd.setCursor(atoi(cfgLcdChar) / 2 - centerText(text), iRow);
      }

      else {
        lcd.setCursor(LCD_FIRST_COL, iRow);
      }

      lcd.print(text);
      if (escaped) {                                                                            // Special character in the string
        if (text.length() < atoi(cfgLcdChar) / 2) {
          fixSpecialChar(text, atoi(cfgLcdChar) / 2 - centerText(text), iRow);
        }

        else {
          fixSpecialChar(text, LCD_FIRST_COL, iRow);
        }
      }

      lcd.cursor();
      if (text.length() < atoi(cfgLcdChar) / 2) {
        lcd.setCursor(atoi(cfgLcdChar) / 2 - centerText(text) + text.length(), iRow);
      }

      else {
        lcd.setCursor(LCD_FIRST_COL + text.length(), iRow);
      }

      lcd.blink();
    break;
//----------------------------------------------------------------------------------------------
    default:
      lcd.setCursor(cCol, cRow);
      lcd.print(addBlanks(atoi(cfgLcdChar) / 2));                                               // Clear the position
      lcd.setCursor(LCD_FIRST_COL, iRow);
      lcd.print(addBlanks(atoi(cfgLcdChar)));                                                   // Clear the row
      lcd.setCursor(atoi(cfgLcdChar) / 2 - centerText(text), iRow);
      lcd.print(text);
      if (escaped) {                                                                            // Special character in the string
        fixSpecialChar(text, atoi(cfgLcdChar) / 2 - centerText(text), iRow);
      }
#ifdef DEBUG
      Serial.printf("%-16s: %d String: %s\n", __func__, __LINE__, text.c_str());
#endif
    break;
//----------------------------------------------------------------------------------------------
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Set destination text
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setNodeString(uint8_t dest, uint8_t track) {

#ifdef DEBUG_ALL
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d %s(dest: %d, track: %d)\n", __func__, __LINE__, __func__, dest, track);
#endif

  String destTxt  = tamBoxConfig[dest][SIGN];
  uint8_t nodeLen = atoi(cfgLcdChar) / 2 - (LCD_DEST_LEN + LCD_DIR_LEN);
#ifdef DEBUG
  String trackSymbol = "-";                                                                     // show track sign during debug
  if (tamBoxConfig[dest][TRACKS].toInt() == DOUBLE_TRACK) {
    if (tamBoxConfig[dest][TYPE] == TYPE_DOUBLE_T) {
      trackSymbol = "=";
    }

    else if (tamBoxConfig[dest][TYPE] == TYPE_SPLIT_T && track == RIGHT_TRACK) {
      destTxt = tamBoxConfig[dest + 5][SIGN];
    }
  }
#endif
  if (trainId[dest][track] != DEST_TRAIN_0) { destTxt = String(trainId[dest][track]); }

  switch (dest) {
    case DEST_A:                                                                                // Destination on left side
    case DEST_C:                                                                                // Destination on left side
#ifdef DEBUG
      if (trainId[dest][track] == DEST_TRAIN_0) {
        destTxt = destTxt + trackSymbol;                                                        // show track sign during debug
      }
#endif
      lcdString[dest][LCD_NODE] = destTxt + addBlanks(nodeLen - destTxt.length());              // Set destination sign
    break;
//----------------------------------------------------------------------------------------------
    default:                                                                                    // Destination on right side
#ifdef DEBUG
      if (trainId[dest][track] == DEST_TRAIN_0) {
        destTxt = trackSymbol + destTxt;                                                        // show track sign during debug
      }
#endif
      lcdString[dest][LCD_NODE] = addBlanks(nodeLen - destTxt.length()) + destTxt;              // Set destination sign
    break;
//----------------------------------------------------------------------------------------------
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Set direction for a destination
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setDirString(uint8_t dest, uint8_t track) {

#ifdef DEBUG_ALL
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d %s(dest: %d, track: %d)\n", __func__, __LINE__, __func__, dest, track);
  Serial.printf("%-16s: %d Traffic direction: %s\n", __func__, __LINE__, traffDir[dest][track]);
#endif

  switch (dest) {
    case DEST_A:                                                                                // Destination on left side
    case DEST_C:                                                                                // Destination on left side
      switch (traffDir[dest][track]) {                                                          // Check track traffic direction
        case DIR_OUT:                                                                           // Track direction is outgoing
          switch (trackState[dest][track]) {                                                    // Check track state
            case _OUTREQUEST:                                                                   // Track state in outgoing request
              lcdString[dest][LCD_DIR]  = DIR_QUERY_T;                                          // Show character for ongoing request
            break;
//----------------------------------------------------------------------------------------------
            case _OUTTRAIN:                                                                     // Track state in outgoing train
              lcdString[dest][LCD_DIR]  = DIR_LEFT_TRAIN_T;                                     // Show character for outgoing train
            break;
//----------------------------------------------------------------------------------------------
            default:                                                                            // Track state in other state
              lcdString[dest][LCD_DIR]  = DIR_LEFT_T;                                           // Show outgoing traffic direction character
            break;
//----------------------------------------------------------------------------------------------
          }
        break;
//----------------------------------------------------------------------------------------------
        case DIR_IN:                                                                            // Track direction is incoming
          switch (trackState[dest][track]) {                                                    // Check track state
            case _INTRAIN:                                                                      // Track state in incoming train
              lcdString[dest][LCD_DIR]  = DIR_RIGHT_TRAIN_T;                                    // Show character for incoming train
            break;
//----------------------------------------------------------------------------------------------
            default:                                                                            // Track state in other state
              lcdString[dest][LCD_DIR]  = DIR_RIGHT_T;                                          // Show incoming traffic direction character
            break;
//----------------------------------------------------------------------------------------------
          }
        break;
//----------------------------------------------------------------------------------------------
        default:                                                                                // Connection lost
          lcdString[dest][LCD_DIR]      = DIR_LOST_T;                                           // Show lost connection character
        break;
//----------------------------------------------------------------------------------------------
      }
    break;
//----------------------------------------------------------------------------------------------
    default:                                                                                    // Destination on right side
      switch (traffDir[dest][track]) {                                                          // Check track traffic direction
        case DIR_OUT:                                                                           // Track direction is outgoing
          switch (trackState[dest][track]) {                                                    // Check track state
            case _OUTREQUEST:                                                                   // Track state in outgoing request
              lcdString[dest][LCD_DIR]  = DIR_QUERY_T;                                          // Show character for ongoing request
            break;
//----------------------------------------------------------------------------------------------
            case _OUTTRAIN:                                                                     // Track state in outgoing train
              lcdString[dest][LCD_DIR]  = DIR_RIGHT_TRAIN_T;                                    // Show character for outgoing train
            break;
//----------------------------------------------------------------------------------------------
            default:                                                                            // Track state in other state
              lcdString[dest][LCD_DIR]  = DIR_RIGHT_T;                                          // Show outgoing traffic direction character
            break;
//----------------------------------------------------------------------------------------------
          }
        break;
//----------------------------------------------------------------------------------------------
        case DIR_IN:                                                                            // Track direction is incoming
          switch (trackState[dest][track]) {                                                    // Check track state
            case _INTRAIN:                                                                      // Track state in incoming train
              lcdString[dest][LCD_DIR]  = DIR_LEFT_TRAIN_T;                                     // Show character for incoming train
            break;
//----------------------------------------------------------------------------------------------
            default:                                                                            // Track state in other state
              lcdString[dest][LCD_DIR]  = DIR_LEFT_T;                                           // Show incoming traffic direction character
            break;
//----------------------------------------------------------------------------------------------
          }
        break;
//----------------------------------------------------------------------------------------------
        default:                                                                                // Connection lost
          lcdString[dest][LCD_DIR]      = DIR_LOST_T;                                           // Show lost connection character
        break;
//----------------------------------------------------------------------------------------------
      }
    break;
//----------------------------------------------------------------------------------------------
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Update the display
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void updateLcd(uint8_t dest) {

#ifdef DEBUG_ALL
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d %s(dest: %d)\n", __func__, __LINE__, __func__, dest);
#endif

  String stnName  = tamBoxConfig[OWN][NAME];
  String stnSign  = tamBoxConfig[OWN][SIGN];
  uint8_t signLen, useRow;
  bool escaped    = false;                                                                      // Set no escape needed

  switch (dest) {
    case OWN:                                                                                   // Show own station
#ifdef NON_EU_CHAR_SET
      // Tempfix, when received unicode is not printed correctly
      // on the LCD with asian character set
      if (tamBoxConfig[OWN][SIGN].indexOf(String(escapeChar)) >= 0) {
        stnSign = removeEscapeChar(tamBoxConfig[OWN][SIGN]);
        escaped = true;                                                                         // Special character in the station name
      }
#endif
      lcd.clear();
      if(stnSign.length() <= atoi(cfgLcdChar)) {
        lcd.setCursor(atoi(cfgLcdChar) / 2 - centerText(stnSign), LCD_FIRST_ROW);
      }

      else {                                                                                    // Station name longer than LCD max character
        lcd.setCursor(LCD_FIRST_COL, LCD_FIRST_ROW);
        lcd.autoscroll();                                                                       // scroll the text
      }

      lcd.print(stnSign);

      if (escaped) {
        fixSpecialChar(stnSign, atoi(cfgLcdChar) / 2 - centerText(stnSign), LCD_FIRST_ROW);
        escaped = false;
      }
#ifdef NON_EU_CHAR_SET
      // Tempfix, when received unicode is not printed correctly
      // on the LCD with asian character set
      if (tamBoxConfig[OWN][NAME].indexOf(String(escapeChar)) >= 0) {
        stnName = removeEscapeChar(tamBoxConfig[OWN][NAME]);
        escaped = true;                                                                         // Special character in the station name
      }
#endif
      lcd.setCursor(atoi(cfgLcdChar) / 2 - centerText(stnName), LCD_SECOND_ROW);
      lcd.print(stnName);

      if (escaped) {
        fixSpecialChar(stnName, atoi(cfgLcdChar) / 2 - centerText(stnName), LCD_SECOND_ROW);
        escaped = false;
      }
    break;
//----------------------------------------------------------------------------------------------
    case DEST_ALL_DEST:                                                                         // Refresh LCD
      // show running screen
      lcd.clear();
      lcd.noAutoscroll();
      lcd.home();
      for (uint8_t i = 0; i < DEST_BUTTONS; i++) {
        if (i == DEST_A || i == DEST_C) {
          stnSign = lcdString[i][LCD_NODE];
#ifdef NON_EU_CHAR_SET
          // Tempfix, when received unicode is not printed correctly
          // on the LCD with asian character set
          if (lcdString[i][LCD_NODE].indexOf(String(escapeChar)) >= 0) {
            signLen = lcdString[i][LCD_NODE].length();
            stnSign = removeEscapeChar(lcdString[i][LCD_NODE]);
            stnSign = stnSign + addBlanks(signLen-stnSign.length());
            escaped = true;                                                                     // Special character in the station signature
          }
#endif
          if (atoi(cfgLcdRows) == 4 && i == DEST_C) {                                           // Four row LCD
            useRow = LCD_THIRD_ROW;
          }

          else if (i == DEST_A) {
            useRow = LCD_FIRST_ROW;
          }

          else {
            useRow = LCD_SECOND_ROW;
          }

          lcd.setCursor(LCD_FIRST_COL, useRow);
          lcd.print(lcdString[i][LCD_DEST]);

          if (lcdString[i][LCD_DIR] == DIR_RIGHT_TRAIN_T ||
              lcdString[i][LCD_DIR] == DIR_LEFT_TRAIN_T) {
            fixSpecialChar(lcdString[i][LCD_DIR], 1, useRow);
          }

          else {
            lcd.print(lcdString[i][LCD_DIR]);
          }

          lcd.print(stnSign);

          if (escaped) {
            fixSpecialChar(stnSign, LCD_FIRST_COL + LCD_DEST_LEN + LCD_DIR_LEN, useRow);
            escaped = false;
          }
        }

        else {
          stnSign = lcdString[i][LCD_NODE];
#ifdef NON_EU_CHAR_SET
          // Tempfix, when received unicode is not printed correctly
          // on the LCD with asian character set
          if (lcdString[i][LCD_NODE].indexOf(String(escapeChar)) >= 0) {
            signLen = lcdString[i][LCD_NODE].length();
            stnSign = removeEscapeChar(lcdString[i][LCD_NODE]);
            stnSign = addBlanks(signLen-stnSign.length()) + stnSign;
            escaped = true;                                                                     // Special character in the station signature
          }
#endif
          if (atoi(cfgLcdRows) == 4 && i == DEST_D) {                                           // Four row LCD
            useRow = LCD_THIRD_ROW;
          }

          else if (i == DEST_B) {
            useRow = LCD_FIRST_ROW;
          }

          else {
            useRow = LCD_SECOND_ROW;
          }

          lcd.setCursor(atoi(cfgLcdChar) / 2, useRow);
          lcd.print(stnSign);

          if (escaped) {
            fixSpecialChar(stnSign, atoi(cfgLcdChar) / 2, useRow);
            lcd.setCursor(atoi(cfgLcdChar) - 2, useRow);
            escaped = false;
          }

          if (lcdString[i][LCD_DIR] == DIR_RIGHT_TRAIN_T ||
              lcdString[i][LCD_DIR] == DIR_LEFT_TRAIN_T) {
            fixSpecialChar(lcdString[i][LCD_DIR], atoi(cfgLcdChar) - 2, useRow);
          }

          else {
            lcd.print(lcdString[i][LCD_DIR]);
          }

          lcd.print(lcdString[i][LCD_DEST]);
        }
      }
    break;
//----------------------------------------------------------------------------------------------
    default:
      if (dest == DEST_A || dest == DEST_C) {
        stnSign = lcdString[dest][LCD_NODE];
#ifdef NON_EU_CHAR_SET
        // Tempfix, when received unicode is not printed correctly
        // on the LCD with asian character set
        if (lcdString[dest][LCD_NODE].indexOf(String(escapeChar)) >= 0) {
          signLen = lcdString[dest][LCD_NODE].length();
          stnSign = removeEscapeChar(lcdString[dest][LCD_NODE]);
          stnSign = stnSign + addBlanks(signLen-stnSign.length());
          escaped = true;                                                                       // Special character in the station signature
        }
#endif
        if (atoi(cfgLcdRows) == 4 && dest == DEST_C) {                                          // Four row LCD
          useRow = LCD_THIRD_ROW;
        }

        else if (dest == DEST_A) {
          useRow = LCD_FIRST_ROW;
        }

        else {
          useRow = LCD_SECOND_ROW;
        }

        lcd.setCursor(LCD_FIRST_COL, useRow);
        lcd.print(lcdString[dest][LCD_DEST]);

        if (lcdString[dest][LCD_DIR] == DIR_RIGHT_TRAIN_T ||
            lcdString[dest][LCD_DIR] == DIR_LEFT_TRAIN_T) {
          fixSpecialChar(lcdString[dest][LCD_DIR], 1, useRow);
        }

        else {
          lcd.print(lcdString[dest][LCD_DIR]);
        }

        lcd.print(stnSign);

        if (escaped) {
          fixSpecialChar(stnSign, LCD_FIRST_COL + LCD_DEST_LEN + LCD_DIR_LEN, useRow);
          escaped = false;
        }
      }

      else if (dest == DEST_B || dest == DEST_D) {
        stnSign = lcdString[dest][LCD_NODE];
#ifdef NON_EU_CHAR_SET
        // Tempfix, when received unicode is not printed correctly
        // on the LCD with asian character set
        if (lcdString[dest][LCD_NODE].indexOf(String(escapeChar)) >= 0) {
          signLen = lcdString[dest][LCD_NODE].length();
          stnSign = removeEscapeChar(lcdString[dest][LCD_NODE]);
          stnSign = addBlanks(signLen-stnSign.length()) + stnSign;
          escaped = true;                                                                       // Special character in the station name
        }
#endif
        if (atoi(cfgLcdRows) == 4 && dest == DEST_D) {                                          // Four row LCD
          useRow = LCD_THIRD_ROW;
        }
        else if (dest == DEST_B) {
          useRow =  LCD_FIRST_ROW;
        }

        else {
          useRow = LCD_SECOND_ROW;
        }

        lcd.setCursor(atoi(cfgLcdChar) / 2, useRow);
        lcd.print(stnSign);

        if (escaped) {
          fixSpecialChar(stnSign, atoi(cfgLcdChar) / 2, useRow);
          lcd.setCursor(atoi(cfgLcdChar) - 2, useRow);
          escaped = false;
        }

        if (lcdString[dest][LCD_DIR] == DIR_RIGHT_TRAIN_T ||
            lcdString[dest][LCD_DIR] == DIR_LEFT_TRAIN_T) {
          fixSpecialChar(lcdString[dest][LCD_DIR], atoi(cfgLcdChar) - 2, useRow);
        }

        else {
          lcd.print(lcdString[dest][LCD_DIR]);
        }

        lcd.print(lcdString[dest][LCD_DEST]);
      }
    break;
//----------------------------------------------------------------------------------------------
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Set defaults
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setDefaults() {

#ifdef DEBUG
  String debugTrack;                                                                            // show track sign during debug
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d Setting default values\n", __func__, __LINE__);
#endif

  String destTxt;
  uint8_t nodeLen   = atoi(cfgLcdChar) / 2 - (LCD_DEST_LEN + LCD_DIR_LEN);


  for (uint8_t dest = 0; dest < DEST_BUTTONS; dest++) {
#ifdef DEBUG
    debugTrack = (tamBoxConfig[dest][TRACKS].toInt() == SINGLE_TRACK) ? "-" : "=";              // Used for debug to show single track or double track
#endif
    destTxt = tamBoxConfig[dest][SIGN];                                                         // Set destinationens sign
    if (tamBoxConfig[dest][ID] == NOT_USED_T) {                                                 // Not used Destination
      trackState[dest][LEFT_TRACK]    = _NOTUSED;                                               // Set left track not used
      trackState[dest][RIGHT_TRACK]   = _NOTUSED;                                               // Set right track not used
      lcdString[dest][LCD_DEST]       = addBlanks(LCD_DEST_LEN);                                // Don't show destination letter on LCD
      lcdString[dest][LCD_DIR]        = addBlanks(LCD_DIR_LEN);                                 // Don't show direction character on LCD
      lcdString[dest][LCD_NODE]       = addBlanks(nodeLen);                                     // Don't show destinations sign on LCD
    }

    else {                                                                                      // Destination in use
      if (tamBoxConfig[dest][TYPE] == TYPE_SINGLE_T) {                                          // Single track Destination
        trackState[dest][LEFT_TRACK]  = _IDLE;                                                  // Set left track state to lost
        trackState[dest][RIGHT_TRACK] = _NOTUSED;                                               // Set right track not used
      }

      else if (tamBoxConfig[dest][TYPE] == TYPE_SPLIT_T) {                                      // Single track to two Destination (Not supported yet)
        trackState[dest][LEFT_TRACK]  = _IDLE;                                                  // Set left track lost to Destination 1
        trackState[dest][RIGHT_TRACK] = _IDLE;                                                  // Set right track lost to Destination 2
      }

      else {                                                                                    // Double track Destination
        trackState[dest][LEFT_TRACK]  = _IDLE;                                                  // Set left track state to lost
        setDirString(dest, LEFT_TRACK);
        trackState[dest][RIGHT_TRACK] = _IDLE;                                                  // Set right track state to lost
      }

      lcdString[dest][LCD_DEST] = String(destIDTxt[dest]);                                      // Set Destinaton letter

      switch (dest) {
        case DEST_A:                                                                            // Destination on left side
        case DEST_C:                                                                            // Destination on left side
#ifdef DEBUG
          destTxt = destTxt + debugTrack;                                                       // Show track sign during debug
#endif
//          lcdString[dest][LCD_DIR]    = DIR_LOST_T;
          lcdString[dest][LCD_DIR]    = DIR_LEFT_T;                                             // Show left traffic character
          lcdString[dest][LCD_NODE]   = destTxt + addBlanks(nodeLen - destTxt.length());        // Show destinatins sign
        break;
//----------------------------------------------------------------------------------------------
        default:                                                                                // Destination on right side
#ifdef DEBUG
          destTxt = debugTrack + destTxt;                                                       // Show track sign during debug
#endif
//          lcdString[dest][LCD_DIR]    = DIR_LOST_T;
          lcdString[dest][LCD_DIR]    = DIR_RIGHT_T;                                            // Show right traffic character
          lcdString[dest][LCD_NODE]   = addBlanks(nodeLen - destTxt.length()) + destTxt;        // Show destinatins sign
        break;
 //----------------------------------------------------------------------------------------------
     }
    }
//----------------------------------------------------------------------------------------------
#ifdef DEBUG_ALL
    switch (dest) {
      case DEST_A:
      case DEST_C:
        Serial.printf("%-16s: %d Destination %s: '%s %s %s'\n", __func__, __LINE__, dest, lcdString[dest][LCD_DEST], lcdString[dest][LCD_DIR], lcdString[dest][LCD_NODE]);
      break;
//----------------------------------------------------------------------------------------------
      default:
        Serial.printf("%-16s: %d Destination %s: '%s %s %s'\n", __func__, __LINE__, dest, lcdString[dest][LCD_NODE], lcdString[dest][LCD_DIR], lcdString[dest][LCD_DEST]);
      break;
//----------------------------------------------------------------------------------------------
    }
    Serial.printf("%-16s: %d trackState destination %s  track left  set to %s\n", __func__, __LINE__, destIDTxt[dest], trackStateTxt[trackState[dest][LEFT_TRACK]]]);
    Serial.printf("%-16s: %d trackState destination %s  track right set to %s\n", __func__, __LINE__, destIDTxt[dest], trackStateTxt[trackState[dest][RIGHT_TRACK]]);
#endif
  }

  destBtnPushed = 0;                                                                            // Set destination button not pushed
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  (Re)connects to MQTT broker and subscribes to one or more topics
 * ------------------------------------------------------------------------------------------------------------------------------
 */
bool mqttConnect() {

#ifdef DEBUG
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d Connecting to broker\n", __func__, __LINE__);
#endif

  char usr[11];                                     // size 10 in the db
  char pwd[11];                                     // size 10 in the db
  char tmpTopic[60];
  char tmpContent[20];
  char tmpID[clientID.length()];                    // For converting clientID
  const char* willMessage;
  String lost = LOST;
  willMessage  = lost.c_str();
  
  // Set up broker
  setupBroker();

  // check if user and password should be used
  if (tamBoxMqtt[USER] != NULL && tamBoxMqtt[PASS] != NULL) {
    tamBoxMqtt[USER].toCharArray(usr, tamBoxMqtt[USER].length() + 1);
    tamBoxMqtt[PASS].toCharArray(pwd, tamBoxMqtt[PASS].length() + 1);
  }

  // Convert String to char* for last will message
  clientID.toCharArray(tmpID, clientID.length() + 1);
  
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    lcd.home();
    lcd.print(LCD_STARTING_UP + addBlanks(atoi(cfgLcdChar) - strlen(LCD_STARTING_UP)));
    lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
    lcd.print(LCD_STARTING_MQTT + addBlanks(atoi(cfgLcdChar) - strlen(LCD_STARTING_MQTT)));
    delay(1000);
#ifdef DEBUG
    Serial.printf("%-16s: %d MQTT connecting... ", __func__, __LINE__);
#endif
    strcpy(tmpTopic, DATA); strcat(tmpTopic, "/");
    strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
    strcat(tmpTopic, NODE); strcat(tmpTopic, "/");
    strcat(tmpTopic, tamBoxConfig[OWN][ID].c_str()); strcat(tmpTopic, "/$state");

    // Attempt to connect
    // boolean connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
    if (mqttClient.connect(tmpID, usr, pwd, tmpTopic, 0, true, willMessage)) {
      lcd.home();
      lcd.print(LCD_STARTING_UP + addBlanks(atoi(cfgLcdChar) - strlen(LCD_STARTING_UP)));
      lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
      lcd.print(LCD_BROKER_CONNECTED + addBlanks(atoi(cfgLcdChar) - strlen(LCD_BROKER_CONNECTED)));
      delay(1000);
#ifdef DEBUG
      Serial.printf("%-16s: %d  ...connected\n", __func__, __LINE__);
      Serial.printf("%-16s: %d MQTT client id: %s\n", __func__, __LINE__, iotWebConf.getThingName());
#endif

      // Subscribe to all topics
#ifdef DEBUG
      Serial.printf("%-16s: %d Subscribing to:\n", __func__, __LINE__);
#endif

      for (uint8_t dest = 0; dest < DEST_BUTTONS; dest++) {
        if (trackState[dest][LEFT_TRACK] != _NOTUSED) {                                         // Destination used
          strcpy(tmpTopic, DATA); strcat(tmpTopic, "/");
          strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
          strcat(tmpTopic, TAM); strcat(tmpTopic, "/");
          strcat(tmpTopic, tamBoxConfig[dest][ID].c_str()); strcat(tmpTopic, "/#");
#ifdef DEBUG
          Serial.printf("%-16s: %d - %s\n", __func__, __LINE__, tmpTopic);
#endif
          mqttClient.subscribe(tmpTopic);                                                       // TAM messages for connected destination

          strcpy(tmpTopic, DATA); strcat(tmpTopic, "/");
          strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
          strcat(tmpTopic, NODE); strcat(tmpTopic, "/");
          strcat(tmpTopic, tamBoxConfig[dest][ID].c_str()); strcat(tmpTopic, "/#");
#ifdef DEBUG
          Serial.printf("%-16s: %d - %s\n", __func__, __LINE__, tmpTopic);
#endif
          mqttClient.subscribe(tmpTopic);                                                       // Node messages for connected destination
        }

        if (tamBoxConfig[dest][TYPE] == TYPE_SPLIT_T) {                                         // Double track split to two single tracks
          if (trackState[dest][RIGHT_TRACK] != _NOTUSED) {                                      // Destination used
            strcpy(tmpTopic, DATA); strcat(tmpTopic, "/");
            strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
            strcat(tmpTopic, TAM); strcat(tmpTopic, "/");
            strcat(tmpTopic, tamBoxConfig[dest + 5][ID].c_str()); strcat(tmpTopic, "/#");
#ifdef DEBUG
            Serial.printf("%-16s: %d - %s\n", __func__, __LINE__, tmpTopic);
#endif
            mqttClient.subscribe(tmpTopic);                                                     // TAM messages for connected destination

            strcpy(tmpTopic, DATA); strcat(tmpTopic, "/");
            strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
            strcat(tmpTopic, NODE); strcat(tmpTopic, "/");
            strcat(tmpTopic, tamBoxConfig[dest + 5][ID].c_str()); strcat(tmpTopic, "/#");
#ifdef DEBUG
            Serial.printf("%-16s: %d - %s\n", __func__, __LINE__, tmpTopic);
#endif
            mqttClient.subscribe(tmpTopic);                                                     // Node messages for connected destination
          }
        }
      }

      strcpy(tmpTopic, COMMAND); strcat(tmpTopic, "/");
      strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
      strcat(tmpTopic, TAM); strcat(tmpTopic, "/");
      strcat(tmpTopic, tamBoxConfig[OWN][ID].c_str()); strcat(tmpTopic, "/#");
#ifdef DEBUG
      Serial.printf("%-16s: %d - %s\n", __func__, __LINE__, tmpTopic);
#endif
      mqttClient.subscribe(tmpTopic);                                                           // Command for node

      strcpy(tmpTopic, COMMAND); strcat(tmpTopic, "/");
      strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
      strcat(tmpTopic, NODE); strcat(tmpTopic, "/");
      strcat(tmpTopic, tamBoxConfig[OWN][ID].c_str()); strcat(tmpTopic, "-");
      strcat(tmpTopic, NODE_SUPERVISOR); strcat(tmpTopic, "/#");
#ifdef DEBUG
      Serial.printf("%-16s: %d - %s\n", __func__, __LINE__, tmpTopic);
#endif
      mqttClient.subscribe(tmpTopic);                                                           // Command for node supervisor
    }

    else {
      // Show why the connection failed
      lcd.home();
      lcd.print(LCD_START_ERROR + addBlanks(atoi(cfgLcdChar) - strlen(LCD_START_ERROR)));
      lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
      lcd.print(LCD_BROKER_NOT_FOUND + addBlanks(atoi(cfgLcdChar) - strlen(LCD_BROKER_NOT_FOUND)));

#ifdef DEBUG
      Serial.printf("%-16s: %d  ...connection failed, rc: %d, retrying again in 5 seconds\n", __func__, __LINE__, mqttClient.state());
#endif
      delay(5000);                                                                              // Wait 5 seconds before retrying
    }

  // Set node status to "ready"
  updateLcd(OWN);                                                                               // Show own station id and name
  delay(4000);                                                                                  // Show it for 4 sec
  updateLcd(DEST_ALL_DEST);                                                                     // Restore the LCD

#ifdef DEBUG
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d Initial publishing:\n", __func__, __LINE__);
#endif
  strcpy(tmpTopic, DATA); strcat(tmpTopic, "/");
  strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
  strcat(tmpTopic, NODE); strcat(tmpTopic, "/");
  strcat(tmpTopic, tamBoxConfig[OWN][ID].c_str()); strcat(tmpTopic, "/$state");
  strcpy(tmpContent, READY);
#ifdef DEBUG
  Serial.printf("%-16s: %d - %s: %s\n", __func__, __LINE__, tmpTopic, tmpContent);
#endif
  mqttClient.publish(tmpTopic, tmpContent, RETAIN);                                             // Node state

  strcpy(tmpTopic, DATA); strcat(tmpTopic, "/");
  strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
  strcat(tmpTopic, NODE); strcat(tmpTopic, "/");
  strcat(tmpTopic, tamBoxConfig[OWN][ID].c_str()); strcat(tmpTopic, "/$software");
  strcpy(tmpContent, SW_TYPE);
#ifdef DEBUG
  Serial.printf("%-16s: %d - %s: %s\n", __func__, __LINE__, tmpTopic, tmpContent);
#endif
  mqttClient.publish(tmpTopic, tmpContent, NORETAIN);                                           // Node software

  strcpy(tmpTopic, DATA); strcat(tmpTopic, "/");
  strcat(tmpTopic, tamBoxMqtt[SCALE].c_str()); strcat(tmpTopic, "/");
  strcat(tmpTopic, NODE); strcat(tmpTopic, "/");
  strcat(tmpTopic, tamBoxConfig[OWN][ID].c_str()); strcat(tmpTopic, "/$softwareversion");
  strcpy(tmpContent, SW_VERSION);
#ifdef DEBUG
  Serial.printf("%-16s: %d - %s: %s\n", __func__, __LINE__, tmpTopic, tmpContent);
#endif
  mqttClient.publish(tmpTopic, tmpContent, NORETAIN);                                           // Node software version
  }

  tamboxReady = true;                                                                           // Set tambox ready
  tamBoxIdle  = true;                                                                           // Set tambox idle
#ifdef DEBUG
  Serial.printf("%-16s: %d - tamBoxIdle set to true\n", __func__, __LINE__);
#endif
  showTime = millis();
#ifdef DEBUG
  Serial.printf("%-16s: %d -- TamBox Ready! --\n\n", __func__, __LINE__);
#endif
  return true;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to handle MQTT messages sent to this node
 *    MQTT structure:
 *    TOPIC_MSGTYPE /TOPIC_SCALE /TOPIC_TYPE /TOPIC_NODE_ID /TOPIC_PORT_ID     payload
 *
 *  ex cmd/h0/tam/tambox-4/a/req    {state: {desired: out}}
 *     cmd/h0/tam/tambox-2/c/req    {loco-id: 233, state: {desired: accept}}
 *     cmd/h0/tam/tambox-3/b/res    {loco-id: 233, state: {desired: accept, reported: accepted}}
 *     cmd/h0/tam/tambox-2/a/res    {loco-id: 233, state: {desired: accept, reported: rejected}}
 *     dt/h0/node/tambox-2/$state   lost
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  // Don't know why this have to be done :-(
  payload[length] = '\0';

  // Make strings
  char* msg   = (char*)payload;
  String tpc  = String((char*)topic);

#ifdef DEBUG
  Serial.printf("%-16s: %d\n", __func__, __LINE__);
//  Serial.printf("%-16s: %d Recieved message: %s\n", __func__, __LINE__, topic);
   Serial.printf("%-16s: %d Recieved message: %s - %s\n", __func__, __LINE__, topic, msg);
#endif

  uint8_t i = 0;
  uint8_t p = 0;
  String s;
  String subTopic[NUM_OF_TOPICS];                                                               // Topic array

  for (uint8_t t = 0; t < NUM_OF_TOPICS; t++) {                                                 // Split topic string into an array
    i = tpc.indexOf("/", p);
    s = tpc.substring(p, i);
    p = i + 1;
    subTopic[t] = s;
  }

  if (subTopic[TOPIC_SCALE] == tamBoxMqtt[SCALE]) {                                             // Scale (1)
    if (subTopic[TOPIC_MSGTYPE] == COMMAND && subTopic[TOPIC_ORDER] == REQUEST) {               // Command (0) Request (5)
#ifdef DEBUG
//      Serial.printf("%-16s: %d cmd/../tam/../req\n", __func__, __LINE__);
#endif
      jsonReceived(_REQUEST, msg);                                                              // Handle the json body
    }

    else if (subTopic[TOPIC_MSGTYPE] == COMMAND && subTopic[TOPIC_ORDER] == RESPONSE) {         // Command (0) Response (5)
      if (subTopic[TOPIC_TYPE] == TAM) {                                                        // TAM
#ifdef DEBUG
//        Serial.printf("%-16s: %d cmd/../tam/../res\n", __func__, __LINE__);
#endif
        jsonReceived(_RESPONSE, msg);                                                           // Handle the json body
      }
#ifdef DEBUG
      else {
        Serial.printf("%-16s: %d Topic type not implemented, type: %s port id: %s\n", __func__, __LINE__, subTopic[TOPIC_TYPE], subTopic[TOPIC_PORT_ID]);
      }
#endif
    }

    else if (subTopic[TOPIC_MSGTYPE] == COMMAND && subTopic[TOPIC_TYPE] == NODE) {              // Node command
      if (subTopic[TOPIC_NODE_ID] == tamBoxConfig[OWN][ID] + "-" + NODE_SUPERVISOR) {           // Own supervisor
#ifdef DEBUG
//        Serial.printf("%-16s: %d cmd/../node/../req\n", __func__, __LINE__);
#endif
        jsonReceived(_REQUEST, msg);                                                            // Handle the json body
      }
	  
    }

    else if (subTopic[TOPIC_MSGTYPE] == DATA && subTopic[TOPIC_TYPE] == TAM) {                  // Data message received
#ifdef DEBUG
//      Serial.printf("%-16s: %d dt/../tam/..\n", __func__, __LINE__);
#endif
      jsonReceived(_DATA, msg);                                                                 // Handle the message
    }

    else if (subTopic[TOPIC_MSGTYPE] == DATA && subTopic[TOPIC_TYPE] == NODE) {                 // Node data message
#ifdef DEBUG
//      Serial.printf("%-16s: %d dt/../node/..\n", __func__, __LINE__);
#endif
      for (uint8_t dest = 0; dest < CONFIG_DEST; dest++) {
        if (subTopic[TOPIC_NODE_ID] == tamBoxConfig[dest][ID]) {                                // Subscribed client
          if (subTopic[TOPIC_PORT_ID].substring(0, 6) == "$state") {                            // State messages
            handleInfo(dest, (String(msg) == READY) ? CODE_READY : CODE_LOST);                  // Handle state message
            break;
          }
        }
      }
    }
#ifdef DEBUG
    else {
      Serial.printf("%-16s: %d Message type not implemented, message: %s type: %s\n", __func__, __LINE__, subTopic[TOPIC_MSGTYPE], subTopic[TOPIC_TYPE]);
    }
#endif
  }

#ifdef DEBUG
  else {
    Serial.printf("%-16s: %d Scale not implemented, scale: %s\n", __func__, __LINE__, subTopic[TOPIC_SCALE]);
  }
#endif
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to download config file
 * ------------------------------------------------------------------------------------------------------------------------------
 */
bool getConfigFile() {

  http.useHTTP10(true);
  http.begin(wifiClient, configPath);
  http.GET();
  /*
    Typical response is:
{
  "id":"tambox-1",
  "config":{
    "signature":"CDA",
    "name":"Charlottendal",
    "destinations":3,
    "destination":{
      "A":{
        "tracks":2,
        "type":"split",
        "left":{
          "id":"tambox-4",
          "tracks":1,
          "exit":"A",
          "signature":"SAL",
        },
        "right":{
          "id":"tambox-5",
          "tracks":1,
          "exit":"A",
          "signature":"SNS",
        }
      },
      "B":{
        "tracks":2,
        "type":"double",
        "double":{
          "id":"tambox-3",
          "tracks":2,
          "exit":"A",
          "signature":"VST",
        }
      },
      "C":{
        "tracks":2,
        "type":"double",
        "double":{
          "id":"tambox-2",
          "tracks":2,
          "exit":"A",
          "signature":"GLA"
        }
      }
    }
  },
  "mqtt":{
    "server":"mqtt-broker.local",
    "port":1883,
    "usr":"",
    "pwd":"",
    "scale":"h0",
    "epoch":1679333055
  }
}
 */
  // Allocate the JSON document
  // Use arduinojson.org/v6/assistant to compute the capacity.
//  const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
  DynamicJsonDocument doc(1024);                                                                // Create a json object

  // Parse JSON object
  DeserializationError error = deserializeJson(doc, http.getStream());

  if (error) {
#ifdef DEBUG
    Serial.printf("%-16s: %d deserializeJson() failed: %s\n", __func__, __LINE__, error.c_str());
#endif
    return false;
  }

  else {
    if (String(doc[ID_T]) == clientID) {
      for (uint8_t i = 0; i < DEST_BUTTONS; i++) {
        tamBoxConfig[i][ID]             = NOT_USED_T;
        tamBoxConfig[i][TOTTRACKS]      = "0";
      }

      JsonObject mqtt = doc[MQTT_T];
      tamBoxMqtt[SERVER]                = String(mqtt[SERVER_T]);                               // 25 characters in db
      tamBoxMqtt[PORT]                  = String(mqtt[PORT_T]);                                 // 5 characters in db
      tamBoxMqtt[USER]                  = String(mqtt[USER_T]);                                 // 10 characters in db
      tamBoxMqtt[PASS]                  = String(mqtt[PASS_T]);                                 // 10 characters in db
      tamBoxMqtt[SCALE]                 = String(mqtt[SCALE_T]);                                // 10 characters in db
      epochTime                         = mqtt[EPOCH_T];

      JsonObject config = doc[CONFIG_T];
      tamBoxConfig[OWN][ID]             = String(doc[ID_T]);                                    // 20 characters in db
      tamBoxConfig[OWN][SIGN]           = String(config[SIGN_T]);                               // 4 characters in db
      tamBoxConfig[OWN][NAME]           = String(config[NAME_T]);                               // 30 characters in db
      tamBoxConfig[OWN][NUMOFDEST]      = String(config[DESTS_T]);                              // tinyint in db (0-255)

      uint8_t i = 0;
      for (JsonPair dest : config[DEST_T].as<JsonObject>()) {                                   // Destinations
        if (i + 1 > DEST_BUTTONS) { break; }
        if (dest.key() == "B") { i=1; }
        else if (dest.key() == "C") { i=2; }
        else if (dest.key() == "D") { i=3; }

        if (dest.value()[TRACK_T] > 0) {
          tamBoxConfig[i][TOTTRACKS]    = String(dest.value()[TRACK_T]);                        // tinyint in db (0-255)
          tamBoxConfig[i][TYPE]         = String(dest.value()[TYPE_T]);                         // 6 characters in db
#ifdef DEBUG
          Serial.printf("%-16s: %d Dest: %s, Type: %s\n", __func__, __LINE__, destIDTxt[i], tamBoxConfig[i][TYPE].c_str());
#endif
          if (tamBoxConfig[i][TYPE] == TYPE_SPLIT_T) {                                          // Type split
            JsonObject destLeft = dest.value()[TYPE_LEFT_T];                                    // Left track
            tamBoxConfig[i][ID]         = String(destLeft[ID_T]);                               // 20 characters in db
            tamBoxConfig[i][SIGN]       = String(destLeft[SIGN_T]);                             // 4 characters in db
            tamBoxConfig[i][TRACKS]     = String(destLeft[TRACK_T]);                            // tinyint in db (0-255)
            tamBoxConfig[i][EXIT]       = String(destLeft[EXIT_T]);                             // 1 characters in db
            tamBoxConfig[i][EXIT].toLowerCase();

            JsonObject destRight = dest.value()[TYPE_RIGHT_T];                                  // Right track
            tamBoxConfig[i + 5][ID]     = String(destRight[ID_T]);                              // 20 characters in db
            tamBoxConfig[i + 5][SIGN]   = String(destRight[SIGN_T]);                            // 4 characters in db
            tamBoxConfig[i + 5][TRACKS] = String(destRight[TRACK_T]);                           // tinyint in db (0-255)
            tamBoxConfig[i + 5][EXIT]   = String(destRight[EXIT_T]);                            // 1 characters in db
            tamBoxConfig[i + 5][EXIT].toLowerCase();
          }

          else {                                                                                // Type single or double
            JsonObject destType = dest.value()[tamBoxConfig[i][TYPE]];
            tamBoxConfig[i][ID]         = String(destType[ID_T]);                               // 20 characters in db
            tamBoxConfig[i][SIGN]       = String(destType[SIGN_T]);                             // 4 characters in db
            tamBoxConfig[i][TRACKS]     = String(destType[TRACK_T]);                            // tinyint in db (0-255)
            tamBoxConfig[i][EXIT]       = String(destType[EXIT_T]);                             // 1 characters in db
            tamBoxConfig[i][EXIT].toLowerCase();
          }
        }
      }

#ifdef DEBUG
      Serial.printf("%-16s: %d Epoch: %d, Millis: %d\n", __func__, __LINE__, epochTime, millis());
#endif
      unsigned int sec = millis()/1000;
      epochTime -= sec;
#ifdef DEBUG
      Serial.printf("%-16s: %d Epoch: %d, reduced with %d seconds\n", __func__, __LINE__, epochTime, sec);
#endif
      return true;
    }

    return false;
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Prepare MQTT broker and define function to handle callbacks
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setupBroker() {

#ifdef DEBUG
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d Broker setup\n", __func__, __LINE__);
#endif
  tmpMqttServer = tamBoxMqtt[SERVER].c_str();
#ifdef DEBUG
  Serial.printf("%-16s: %d Broker used: %s:%d\n", __func__, __LINE__, tmpMqttServer, tamBoxMqtt[PORT].toInt());
#endif

  mqttClient.setServer(tmpMqttServer, tamBoxMqtt[PORT].toInt());
  mqttClient.setCallback(mqttCallback);                                                         // Set function for received MQTT messages
  if (mqttClient.getBufferSize() < 512) {
    mqttClient.setBufferSize(512);                                                              // increase buffer to 512 byte because of JSON
  }
#ifdef DEBUG
  Serial.printf("%-16s: %d Message buffer size set to: %d\n", __func__, __LINE__, mqttClient.getBufferSize());
  Serial.printf("%-16s: %d Broker setup done!\n", __func__, __LINE__);
#endif
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to show client web start page
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void handleRoot() {

  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal()) {

    // -- Captive portal request were already served.
    return;
  }

  char s[512];

  // Assemble web page content
  strcpy(s, "<!DOCTYPE html>\n<html lang=\"en\">\n<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>\n<title>Configuration</title></head>\n<body>\n<ul><li>mqttTamBox client: ");
  strcat(s, iotWebConf.getThingName());
  strcat(s, " running firmware: ");
  strcat(s, SW_VERSION);
  strcat(s, "</li></ul>\nGo to <a href='config'>configuration page</a> to change values.\n</body></html>\n");

  // Show web start page
  server.send(200, "text/html", s);
}

/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function beeing called when wifi connection is up and running
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void wifiConnected() {

  long rssi     = WiFi.RSSI();                                                                  //  -30 dBm   Excelent wifi connection
                                                                                                //  -67 dBm   Good wifi connection
                                                                                                //  -70 dBm   Fair wifi connection
                                                                                                //  -80 dBm   Week wifi connection
                                                                                                //  -90 dBm   Not working wifi connection
  lcd.home();
  lcd.print(LCD_STARTING_UP + addBlanks(atoi(cfgLcdChar) - strlen(LCD_STARTING_UP)));
  lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
  lcd.print(SW_VERSION + addBlanks(atoi(cfgLcdChar) - strlen(SW_VERSION)));
  delay(1000);

  lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
  lcd.print(LCD_WIFI_CONNECTED + addBlanks(atoi(cfgLcdChar) - strlen(LCD_WIFI_CONNECTED)));
  delay(1000);
#ifdef DEBUG
  Serial.printf("%-16s: %d Signal strength (RSSI): %d dBm\n", __func__, __LINE__, rssi);
#endif
  lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
  lcd.print(addBlanks(atoi(cfgLcdChar)));
  lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
  lcd.print(LCD_SIGNAL + String(rssi) + "dBm");
  delay(1000);

  String pathID = configHost + clientID;
  configPath    = pathID.c_str();
//  strcpy(configPath, configHost.c_str());
//  strcat(configPath, clientID.c_str());
#ifdef DEBUG
  Serial.printf("%-16s: %d configPath = %s\n", __func__, __LINE__, configPath);
#endif
  lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
  lcd.print(LCD_LOADING_CONF + addBlanks(atoi(cfgLcdChar) - strlen(LCD_LOADING_CONF)));
  delay(1000);

  if (getConfigFile()) {
#ifdef DEBUG
    Serial.printf("%-16s: %d Config loaded!\n\n", __func__, __LINE__);
#endif
    lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
    lcd.print(LCD_LOADING_CONF_OK + addBlanks(atoi(cfgLcdChar) - strlen(LCD_LOADING_CONF_OK)));
    notReceivedConfig = false;
    setDefaults();
    delay(1000);

    // We are ready to start the MQTT connection
    needMqttConnect = true;
  }

  else {
    lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
    lcd.print(LCD_LOADING_CONF_NOK + addBlanks(atoi(cfgLcdChar) - strlen(LCD_LOADING_CONF_NOK)));
    needReset = true;
    delay(1000);
  }
    
  // We are ready to start the MQTT connection
  //needMqttConnect = true;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function that gets called when web page config has been saved
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void configSaved() {

#ifdef DEBUG
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d IotWebConf config saved\n", __func__, __LINE__);
#endif

  clientID                  = String(iotWebConf.getThingName());
  clientID.toLowerCase();
  lcdBackLight              = atoi(cfgBackLight);
  strcpy(configHost, cfgConfServer);
  strcat(configHost, cfgConfFile);
  lcd.setBacklight(lcdBackLight);
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to replace characters missing in LCD HD44780 with own defined characters
 *  Å (\x85), Ä (\x84), Ö (\x96), É (\x89), Ü (\x9c),
 *  å (\xa5), ä (\xa4), ö (\xb6), é (\xa9),  ü (\xbc),
 *  << (\xab), >> (\xbb)
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void fixSpecialChar(String text, uint8_t col, uint8_t row) {

#ifdef DEBUG_ALL
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d Number of characters in String are %d\n", __func__, __LINE__, text.length());
  Serial.printf("%-16s: %d String: %s\n", __func__, __LINE__, text);
  Serial.printf("%-16s: %d String: ", __func__, __LINE__);
  for (uint8_t i = 0; i < text.length(); i++) {
    Serial.printf("%02x ", text.charAt(i));
  }

  Serial.printf("\n");
#endif
  for (uint8_t i = 0; i < text.length(); i++) {
    if (text.charAt(i) == '\x85') {                                                             // Å
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d Å found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      printSpecialChar(SWE_CAP_Å);
    }

    else if (text.charAt(i) == '\x84') {                                                        // Ä
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d Ä found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      printSpecialChar(SWE_CAP_Ä);
    }

    else if (text.charAt(i) == '\x96') {                                                        // Ö
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d Ö found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      printSpecialChar(SWE_CAP_Ö);
    }

    else if (text.charAt(i) == '\x89') {                                                        // É
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d É found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      lcd.print("E");
    }

    else if (text.charAt(i) == '\x9c') {                                                        // Ü
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d Ü found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      lcd.print("U");
    }

    else if (text.charAt(i) == '\xa5') {                                                        // å
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d å found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      printSpecialChar(SWE_LOW_Å);
    }

    else if (text.charAt(i) == '\xa4') {                                                        // ä
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d ä found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      printSpecialChar(SWE_LOW_Ä);
    }

    else if (text.charAt(i) == '\xb6') {                                                        // ö
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d ö found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      printSpecialChar(SWE_LOW_Ö);
    }

    else if (text.charAt(i) == '\xa9') {                                                        // é
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d é found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      lcd.print("e");
    }

    else if (text.charAt(i) == '\xbc') {                                                        // ü
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d ü found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      lcd.print("u");
    }

    else if (text.charAt(i) == '\xab') {                                                        // >>
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d >> found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      printSpecialChar(TRAIN_MOVING_RIGHT);
    }

    else if (text.charAt(i) == '\xbb') {                                                        // <<
#ifdef DEBUG_ALL
      Serial.printf("%-16s: %d << found at pos %d\n", __func__, __LINE__, i + 1);
#endif
      lcd.setCursor(col + i, row);
      printSpecialChar(TRAIN_MOVING_LEFT);
    }
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to remove escape character for the LCD
 * ------------------------------------------------------------------------------------------------------------------------------
 */
String removeEscapeChar(String text) {

#ifdef DEBUG_ALL
  Serial.printf("%-16s: %d \n", __func__, __LINE__);
  Serial.printf("%-16s: %d Character %s removed\n", __func__, __LINE__, escapeChar);
#endif
  text.replace(String(escapeChar), "");
  return text;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Toggle between right and left track when double track so both directions can be viewed
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void toggleTrack() {

  if (currentTrack == RIGHT_TRACK) {
    currentTrack = LEFT_TRACK;
  }

  else {
    currentTrack = RIGHT_TRACK;
  }

  if (tamBoxIdle) {
    for (uint8_t dest = 0; dest < DEST_BUTTONS; dest++) {
      if (tamBoxConfig[dest][ID] != NOT_USED_T) {
        if (tamBoxConfig[dest][TOTTRACKS].toInt() == 2) {
 //         if (tamBoxConfig[dest][TYPE] == TYPE_DOUBLE_T) {
            setDirString(dest, currentTrack);
            setNodeString(dest, currentTrack);
            updateLcd(dest);
 //         }
        }
      }
    }
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  mqtt-lcp function when cmd is received
 *  
 *  msg-type: MQTT-LCP follows the pattern recommended in the AWS:
 *    "cmd": a message requesting an action or a response to a request for an action. (see command type)
 *    "dt": a message containing data that may be of interest to other MQTT-LCP nodes
 *    
 *  "h0" : All MQTT-LCP topics have "h0" as their second level topic.
 *  
 *  topic_type: The type of message:
 *    "registry - registry message
 *    "fastclock" - fastclock message
 *    "ping" - a "heartbeat" type message published by all MQTT-LCP nodes periodically (every few seconds)
 *    "sensor" - a message published containing data from a sensor
 *    "cab" - a message published to control a loco direction/speed/functions
 *    “signal” - a message to set a signal to specific aspect
 *    "switch" - a message to control a binary device (on/off, throw/close, etc) like a turnout or a light bulb
 *    "node" - a message to a specific MQTT-LCP node application or "all" if message is for all nodes.
 *    "tam" - a message between tamboxes.
 *    
 *  cmd-type: only used on "cmd" type messages. Indicates type of command:
 *    "req": request for an action to be taken. "throw a turnout"
 *    "res": response to an action request. "turnout was thrown" or "Error, could not throw turnout"
 *    
 *    There are some common elements in all the MQTT-LCP JSON message bodies:
 *  
 *  Body
 *  root-element : Type of message ("sensor", "ping", "switch", etc), matches topic type in the message topic.
 *    "version" : version number of MQTT-LCP JSON format
 *    "timestamp" : time message was sent in seconds since the unix epoch (real time, not fastclock)
 *    "session-id" : a unique identifier for the message.
 *  Exception: The session-id in a response must match the session-id of the corresponding request message.
 *    "node-id" : The application node id to receive the message. Matches node-id in message topics.
 *    "port-id" : The port on the receiving node to which the message is to be applied. Matches port-id in the message topic
 *    "loco-id" : Optional. The identification of a specific item like a loco or rfid tag.
 *    "respond-to" " Message topic to be used in the response to a command request. The "return address".
 *    "state" : The state being requested to be changed or the current state being reported.
 *  State has two sub elements:
 *    desired : The state to which the port is to be changed: "close", "off", etc. Used in command type messages
 *    reported : The current state of a port: "closed", "off", etc. Used command response and data messages
 *    "metadata" : Optional. Varies by message type.
 *    
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void jsonReceived(uint8_t order, char* body) {

#ifdef DEBUG
  char db1Text[] = "jsonReceived    : ";
#endif
/*
{"tam": {"version": "1.0", "timestamp": 1590520093, "session-id": "req:1590520093",
    "node-id": "tambox-2", "port-id": "a", "track": "left",
    "respond-to": "cmd/h0/node/tambox-2/res",
    "state": {"desired": "in"}}
}

{"tam": {"version": "1.0", "timestamp": 1590520093, "session-id": "req:1590520093",
    "node-id": "tambox-2", "port-id": "b", "track": "left",
    "loco-id": 1234,
    "respond-to": "cmd/h0/node/tambox-2/res",
    "state": {"desired": "accept"}}
}

{"tam": {"version": "1.0", "timestamp": 1590520093, "session-id": "req:1590520093",
    "node-id": "tambox-2", "port-id": "b", "track": "left",
    "loco-id": 1234,
    "state": {"desired": "accept", "reported": "accepted"}}
}

{"tam": {"version": "1.0", "timestamp": 1590520093, "session-id": "req:1590520093",
    "node-id": "tambox-2", "port-id": "a", "track": "left",
    "loco-id": 1234,
    "state": {"reported": "in"}}
}

{"ping": {"version": "1.0", "timestamp": 1590520093, "session-id": "req:1590520093",
    "node-id": "tambox-2",
    "state": {"reported": "ping"}}
}

{"registry": {"version": "1.0", "timestamp": 1679147474, "session-id": "req:1679147474",
    "node-id": "mqtt-registry",
    "respond-to": "cmd/h0/node/mqtt-registry/res",
    "state": {"desired": {"report": "inventory"}}}
}

{"node": {"version": "1.0", "timestamp": 1590520093, "session-id": "req:1590520093",
    "node-id": "master-node",
    "state": {"desired": "reboot"}}
}
*/
  uint16_t train;
  uint8_t track, orderCode, i;
  char receiver[32];
  uint8_t MyP = 0;
  uint8_t MyI = 0;
  uint8_t dest = 255;
  String s, tpc, nodeId, portId;
  StaticJsonDocument<384> doc;                                                                  // Create a json object
  uint8_t maxSize = 384;
  deserializeJson(doc, body);                                                                   // Read the json body
//  JsonObject root = doc.to<JsonObject>();

  if (String(doc[TAM][VERSION]) == LCP_BODY_VER) {                                              // Type is tam
#ifdef DEBUG
    Serial.printf("%-16s: %d version   : %s\n", __func__, __LINE__, String(doc[TAM][VERSION]));
    Serial.printf("%-16s: %d session-id: ", __func__, __LINE__);
    Serial.println(String(doc[TAM][SESSION_ID]));
    Serial.printf("%-16s: %d timestamp : %d\n", __func__, __LINE__, doc[TAM][TIMESTAMP]);
    Serial.printf("%-16s: %d respond-to: ", __func__, __LINE__);
    Serial.println(String(doc[TAM][RESPOND_TO]));
    Serial.printf("%-16s: %d node-id   : %s\n", __func__, __LINE__, String(doc[TAM][NODE_ID]));
    Serial.printf("%-16s: %d port-id   : %s\n", __func__, __LINE__, String(doc[TAM][PORT_ID]));
    Serial.printf("%-16s: %d track     : %s\n", __func__, __LINE__, String(doc[TAM][TRACK]));
    Serial.printf("%-16s: %d train-id  : %s\n", __func__, __LINE__, String(doc[TAM][TRAIN_ID]));
    Serial.printf("%-16s: %d desired   : %s\n", __func__, __LINE__, String(doc[TAM][STATE][DESIRED]));
    Serial.printf("%-16s: %d reported  : %s\n", __func__, __LINE__, String(doc[TAM][STATE][REPORTED]));
#endif

    if (order == _REQUEST) {                                                                    // Tam request
      if (String(doc[TAM][NODE_ID]) == tamBoxConfig[OWN][ID]) {
        tpc = String(doc[TAM][RESPOND_TO]);

        for (i = 0; i < NUM_OF_TOPICS; i++) {                                                   // Get sending node-id
          MyI = tpc.indexOf("/", MyP);
          s   = tpc.substring(MyP, MyI);
          MyP = MyI + 1;
          if (i == TOPIC_NODE_ID) {
            nodeId = s;
          }

          else if (i == TOPIC_PORT_ID) {
            portId = s;
          }
        }

        for (i = 0; i < NUM_OF_DEST_STRINGS; i++) {
          if (tamBoxConfig[i][ID] == nodeId){
            dest = i;                                                                           // Set corresponding destination
            break;
          }
        }

        if (dest < 255) {
          track                           = (String(doc[TAM][TRACK]) == LEFT) ? LEFT_TRACK : RIGHT_TRACK;

          if (String(doc[TAM][STATE][DESIRED]) == ACCEPT) {
            orderCode                     = CODE_ACCEPT;
          }

          else if (String(doc[TAM][STATE][DESIRED]) == IN) {
            orderCode                     = CODE_TRAFDIR_REQ_IN;
          }

          else {
            orderCode                     = CODE_CANCEL;
          }

          reqCmd[dest][LCP_SESSION_ID]    = String(doc[TAM][SESSION_ID]);
          reqCmd[dest][LCP_RESPOND_TO]    = String(doc[TAM][RESPOND_TO]);
          reqCmd[dest][LCP_DESIRED_STATE] = String(doc[TAM][STATE][DESIRED]);
          reqCmd[dest][LCP_NODE_ID]       = nodeId;
          reqCmd[dest][LCP_PORT_ID]       = portId;
          reqCmd[dest][LCP_TRACK]         = String(doc[TAM][TRACK]);

          if (doc[TAM][TRAIN_ID]) {
            train                         = doc[TAM][TRAIN_ID];
#ifdef DEBUG
            Serial.printf("%-16s: %d TAM request received\n", __func__, __LINE__);
#endif
            handleTrain(dest, track, orderCode, train);                                         // Call train handler routine
          }

          else {
#ifdef DEBUG
            Serial.printf("%-16s: %d Direction request received\n", __func__, __LINE__);
#endif
            handleDirection(dest, track, orderCode);                                            // Call traffic direction handler routine
          }
        }

        else {
#ifdef DEBUG
          Serial.printf("%-16s: %d respond-to is missing\n", __func__, __LINE__);
#endif
        }
      }

      else {
#ifdef DEBUG
        Serial.printf("%-16s: %d Not to me, sent to: %s\n", __func__, __LINE__, String(doc[TAM][NODE_ID]));
#endif
      }
    }

    else if (order == _RESPONSE) {                                                              // Tam response
      if (String(doc[TAM][NODE_ID]) == tamBoxConfig[OWN][ID]) {
        for (dest = 0; dest < DEST_BUTTONS; dest++) {
          if (resCmd[dest][LCP_SESSION_ID] == String(doc[TAM][SESSION_ID])) {
            track                       = (String(doc[TAM][TRACK]) == LEFT) ? LEFT_TRACK : RIGHT_TRACK;

            if (doc[TAM][TRAIN_ID]) {
              train                     = doc[TAM][TRAIN_ID];
              orderCode                 = (String(doc[TAM][STATE][REPORTED]) == ACCEPTED) ? CODE_ACCEPTED : CODE_REJECTED;
#ifdef DEBUG
              Serial.printf("%-16s: %d TAM response received\n", __func__, __LINE__);
#endif
              handleTrain(dest, track, orderCode, train);                                       // Call train handler routine
            }

            else {
              orderCode                 = (String(doc[TAM][STATE][REPORTED]) == IN) ? CODE_TRAFDIR_RES_IN : CODE_TRAFDIR_RES_OUT;
#ifdef DEBUG
              Serial.printf("%-16s: %d Direction response received\n", __func__, __LINE__);
#endif
              handleDirection(dest, track, orderCode);                                          // Call traffic direction handler routine
            }

            break;
          }
        }
      }

      else {
#ifdef DEBUG
        Serial.printf("%-16s: %d Not to me, sent to: %s\n", __func__, __LINE__, String(doc[TAM][NODE_ID]));
#endif
      }
    }

    else if (order == _DATA) {                                                                  // message is data
      for (uint8_t dest = 0; dest < DEST_BUTTONS; dest++) {
        if (String(doc[TAM][NODE_ID]) == tamBoxConfig[dest][ID]) {
          track                         = (String(doc[TAM][TRACK]) == LEFT) ? LEFT_TRACK : RIGHT_TRACK;
          orderCode                     = (String(doc[TAM][STATE][REPORTED]) == IN) ? CODE_TRAIN_IN : CODE_TRAIN_OUT;
          train                         = doc[TAM][TRAIN_ID];
#ifdef DEBUG
          Serial.printf("%-16s: %d Train report received\n", __func__, __LINE__);
#endif
          handleTrain(dest, track, orderCode, train);                                           // Call train handler routine
          break;
        }
      }
    }
  }

  else if (String(doc[PING][VERSION]) == LCP_BODY_VER) {                                        // Type is Ping
    for (uint8_t dest = 0; dest < DEST_BUTTONS; dest++) {
      if (String(doc[TAM][NODE_ID]) == tamBoxConfig[dest][ID]) {
#ifdef DEBUG
        Serial.printf("%-16s: %d Ping object received from: %s\n", __func__, __LINE__, tamBoxConfig[dest][ID]);
#endif
      }
    }
  }

  else if (String(doc[TOWER][VERSION]) == LCP_BODY_VER &&
           String(doc[TOWER][PORT_ID]) == INVENTORY) {                                          // Type is Tower inventory
    if (String(doc[TAM][NODE_ID]) == tamBoxConfig[OWN][ID]) {
#ifdef DEBUG
      Serial.printf("%-16s: %d Inventory object received\n", __func__, __LINE__);
#endif
      JsonObject tower = doc[TOWER];
      char reportData[254];                                                                     // json body

      doc[TOWER][TIMESTAMP]               = epochTime + millis() / 1000;
      doc[TOWER][STATE][REPORTED]         = String(doc[TOWER][STATE][DESIRED]);
      doc[TOWER][NODE_ID]                 = tamBoxConfig[OWN][ID];
      strcpy(receiver, doc[TOWER][RESPOND_TO]);
      doc.remove(RESPOND_TO);                                                                   // Remove respond-to taggen

      size_t n = serializeJson(doc, reportData);                                                // Create a json body
      uint8_t check = mqttClient.publish(receiver, reportData, NORETAIN);                       // Publish an inventory response

#ifdef DEBUG
      if (check == 1) {
        Serial.printf("%-16s: %d Publish: %s with body size: %d (%d to max size)\n", __func__, __LINE__, receiver, n, maxSize - n);
      }

      else {
        Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
      }
#endif
    }

    else {
#ifdef DEBUG
      Serial.printf("%-16s: %d Not to me, sent to: %s\n", __func__, __LINE__, String(doc[TAM][NODE_ID]));
#endif
    }
  }

  else if (String(doc[NODE][VERSION]) == LCP_BODY_VER) {                                        // Type is Node
    if (order == _REQUEST) {
      if (String(doc[TAM][NODE_ID]) == (tamBoxConfig[OWN][ID] + "-" + NODE_SUPERVISOR)) {
#ifdef DEBUG
        Serial.printf("%-16s: %d Node object received\n", __func__, __LINE__);
#endif
        if (String(doc[NODE][STATE][DESIRED]) == LCP_BODY_REBOOT) {                             // Reboot the tambox
          needReset = true;
        }

        else if (String(doc[NODE][STATE][DESIRED]) == LCP_BODY_SHUTDOWN) {                      // Shutdown the tambox
#ifdef DEBUG
          Serial.printf("%-16s: %d Shuttingdown after 5 seconds.\n", __func__, __LINE__);
#endif
          lcd.clear();
          lcd.home();
          lcd.print(LCD_SHUTTINGDOWN);
          delay(5000);
          ESP.deepSleep(0);
        }
      }

      else {
#ifdef DEBUG
        Serial.printf("%-16s: %d Not to me, sent to: %s\n", __func__, __LINE__, String(doc[TAM][NODE_ID]));
#endif
      }
    }
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  mqtt-lcp function to send ping
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void sendPing() {

  const char* bodyType = PING;
  const char* action = "send";
  mqttJson((char*)action, (char*)bodyType);
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Handle json body
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void mqttJson(char* action, char* bodyType) {

//  const size_t capacity = JSON_OBJECT_SIZE(5) + JSON_ARRAY_SIZE(6) + 60;
  StaticJsonDocument<255> doc;                                                                  // Create a json object
  uint8_t maxSize = 255;
  char body[254];
  char receiver[32];

  if (action == "send" && bodyType == PING) {
  // Topic: dt/h0/ping/tambox-1
  strcpy(receiver, DATA); strcat(receiver, "/");
  strcat(receiver, tamBoxMqtt[SCALE].c_str()); strcat(receiver, "/");
  strcat(receiver, PING); strcat(receiver, "/");
  strcat(receiver, tamBoxConfig[OWN][ID].c_str());
/*
  {
    "ping": {
      "node-id": "tambox-1",
      "state": {
        "reported": "ping"
      },
      "version": "1.0",
      "timestamp": 1590520093,
      "metadata": {
      ...
      }
    }
  }
*/
    doc[PING][NODE_ID]          = tamBoxConfig[OWN][ID];
    doc[PING][STATE][REPORTED]  = PING;
    doc[PING][VERSION]          = LCP_BODY_VER;
    doc[PING][TIMESTAMP]        = epochTime + pingTime / 1000;
    doc[PING][METADATA][M_TYPE] = SW_TYPE;
    doc[PING][METADATA][M_VER]  = SW_VERSION;
    doc[PING][METADATA][M_NAME] = tamBoxConfig[OWN][NAME];
    doc[PING][METADATA][M_SIGN] = tamBoxConfig[OWN][SIGN];
    doc[PING][METADATA][M_RSSI] = String(WiFi.RSSI()) + " dBm";

    size_t n = serializeJson(doc, body);                                                        // Create a json body
    uint8_t check = mqttClient.publish(receiver, body, NORETAIN);                               // Publish a ping message

#ifdef DEBUG
    if (check == 1) {
      Serial.printf("%-16s: %d Publish: %s with body size: %d (%d to max size)\n", __func__, __LINE__, receiver, n, maxSize - n);
    }

    else {
      Serial.printf("%-16s: %d Publish %s failed with code: %d\n", __func__, __LINE__, receiver, check);
    }
#endif
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to center a string into the LCD
 * ------------------------------------------------------------------------------------------------------------------------------
 */
uint8_t centerText(String txt) {

  return (txt.length() % 2) ? (txt.length() + 1) / 2 : txt.length() / 2;                        // Return half length
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Insert x number of blanks
 * ------------------------------------------------------------------------------------------------------------------------------
 */
String addBlanks(uint8_t blanks) {

  String s = "";
  for (uint8_t i = 1; i <= blanks; i++) { s += " "; }
  return s;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to beep a buzzer
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void beep(unsigned char duration, unsigned int freq) {

  tone(buzzerPin, freq, duration);
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to print special characters on LCD
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void printSpecialChar(uint8_t character) {

   if (character < 8) {
      lcd.write(character);
   }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to validate the input data form
 * ------------------------------------------------------------------------------------------------------------------------------
 */
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper) {

#ifdef DEBUG
  Serial.printf("%-16s: %d IotWebConf validating form\n", __func__, __LINE__);
#endif

  bool valid = true;
/*  String msg = "Endast dessa är tillåtna signaltyper: ";

  uint8_t l = webRequestWrapper->arg(mqttServerParam.getId()).length();
  if (l < 3)
  {
    mqttServerParam.errorMessage = "Please provide at least 3 characters!";
    valid = false;
  }

  for (uint8_t i = 0; i < nbrSigTypes - 2; i++) {
    if (signalTypes[i] ==  server.arg(webSignal1Type.getId())) {
      valid = true;
      break;
    }

    else {
      valid = false;
      msg += signalTypes[i];
      if (i == nbrSigTypes -1){msg += ","; }
    }
  }

  if (!valid) {
    webSignal1Type.errorMessage = "Endast dessa är tillåtna signaltyper: Not used, Hsi2, Hsi3, Hsi4, Hsi5";
  }
*/
  return valid;
}
