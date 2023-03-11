/*******************************************************************************************************************************
 *
 *   mqttTamBox v1.0.0
 *   copyright (c) Benny Tjäder
 *
 *   This program is free software: you can redistribute it and/or modify it under the terms of the
 *   GNU General Public License as published  by the Free Software Foundation, either version 3 of the License,
 *   or (at your option) any later version.
 *
 *    Uses and have been tested with the following libraries
 *      ArduinoHttpClient     v0.4.0  by                     - https: *github.com/arduino-libraries/ArduinoHttpClient
 *      ArduinoJson           v6.20.0 by Benoit Blanchon     - https: *github.com/bblanchon/ArduinoJson
 *      IotWebConf            v3.2.1  by Balazs Kelemen      - https: *github.com/prampec/IotWebConf
 *      Keypad                v3.1.1  by Chris--A            - https: *github.com/Chris--A/Keypad
 *      Keypad_I2C            v2.3.1  by Joe Young           - https: *github.com/joeyoung/BensArduino-git
 *      LiquidCrystal_PCF8574 v2.1.0  by Matthisa Hertel     - https: *github.com/mathertel/LiquidCrystal_PCF8574
 *      PubSubClient          v2.8.0  by Nick O'Leary        - https: *pubsubclient.knolleary.net/
 *      FastLed               v3.5.0  by Daniel Garcia       - https: *github.com/FastLED/FastLED
 *      Wire                  v2.3.1  by Nicholas Zambetti   - https: *github.com/prampec/IotWebConf
 *
 *    This program is featuring the TAM Box.
 *    TAM Box is used to:
 *     - on sending side:   allocated a track between two stations and request to send a train.
 *     - on receiving side: accept/reject incomming request.
 *
 *    Each track direction is using key A-D. A and C is on left side, and B and D is on right side.
 *    Each A-D button has a RGB led which can show:
 *      - dark:         destination is idle or not in used
 *      - green:        direction is outgoing
 *      - red:          direction is incoming
 *      - flash green:  outgoing request sent
 *      - flash red:    incoming request received
 *      - yellow:       outgoing/incoming request accepted
 *
 *    MQTT structure:
 *    top     /client /destination/type   /track/item     /order   payload
 *    mqtt_h0/tambox-4/cda        /traffic/left /direction/state   up
 *
 *    mqtt_h0/tambox-4/cda/traffic/left/direction/state  out     Traffic direction to CDA on left track is out
 *    mqtt_h0/tambox-2/gla/traffic/left/train/request    2123    Request outgoing train 2123 to GLA on left track
 *    mqtt_h0/tambox-2/sal/traffic/left/train/accept     2123    Train 2123 to SAL accepted on left track
 *    mqtt_h0/tambox-1/vst/traffic/right/train/reject    342     Train 348 to VST rejected on right track
 *    mqtt_h0/tambox-1/vst/traffic/right/train/cancel    342     Train 348 to VST was canceled before VST send accept or reject
 *
 ********************************************************************************************************************************
 *    Flow TAM request-accept
 *    CDA (tambox-1)                                                GLA (tambox-4)
 *    mqtt_h0/tambox-1/gla/traffic/left/direction/state out
 *                                                                  mqtt_h0/tambox-4/cda/traffic/left/direction/state in
 *    mqtt_h0/tambox-1/gla/traffic/left/train/request 2123
 *                                                                  mqtt_h0/tambox-4/cda/traffic/left/train/accept 2123
 *
 *    Flow TAM request-reject
 *    CDA (tambox-1)                                                GLA (tambox-4)
 *    mqtt_h0/tambox-1/gla/traffic/left/direction/state out
 *                                                                  mqtt_h0/tambox-4/cda/traffic/left/direction/state in
 *    mqtt_h0/tambox-1/gla/traffic/left/train/request 2123
 *                                                                  mqtt_h0/tambox-4/cda/traffic/left/train/reject 2123
 *
 *    Flow TAM request-cancel
 *    CDA (tambox-1)                                                GLA (tambox-4)
 *    mqtt_h0/tambox-1/gla/traffic/left/direction/state out
 *                                                                  mqtt_h0/tambox-4/cda/traffic/left/direction/state in
 *    mqtt_h0/tambox-1/gla/traffic/left/train/request 2123
 *    mqtt_h0/tambox-1/gla/traffic/left/train/cancel 2123
 *
 *
 ********************************************************************************************************************************
 */
#include <ArduinoHttpClient.h>                                // Library to handle HTTP download of config file
#include <ArduinoJson.h>                                      // Library to handle JSON for the config file
#include <PubSubClient.h>                                     // Library to handle MQTT communication
#include <IotWebConf.h>                                       // Library to take care of wifi connection & client settings
#include <IotWebConfUsing.h>                                  // This loads aliases for easier class names.
#include <Wire.h>                                             // Library to handle i2c communication
#include <Keypad_I2C.h>                                       // Library to handle i2c keypad
#include <LiquidCrystal_PCF8574.h>                            // Library to handle the LCD
#define FASTLED_ESP8266_D1_PIN_ORDER                          // Needed for nodeMCU pin numbering
//#define FASTLED_ESP8266_NODEMCU_PIN_ORDER                     // Needed for nodeMCU pin numbering
#include <FastLED.h>                                          // Library for LEDs
#include "mqttTamBox.h"                                       // Some of the client settings

CRGB rgbLed[NUM_LED_DRIVERS];                                 // Define the array of leds

// -- Method declarations.
void keyReceived(char key);
void handleInfo(String clt, String msg);
void handleDirection(String clt, int trk, String msg);
void handleTrain(String clt, String trk, String ord, String msg);
void mqttPublish(String pbTopic, String pbPayload, byte retain);
bool getConfigFile();
void setupTopics();
void setupBroker();
void handleRoot();
void wifiConnected();
bool mqttConnect();
void setTxt(int str, int dest, int train);
void updateLcd(int dest);
void setDirectionTxt(int dest, int trk);
void setNodeTxt(int dest, int trk);
void fixSweChar(String text, int col, int row);
String removeEscapeChar(String text);
void testLed();
void setLed(int led, int stat);
void togleTrack();
String setBlanks(int blanks);
int centerText(String txt);
void beep(unsigned char delayms, int freq);
void printSpecialChar(int character, int col, int row);
void setDefaults();

// Callback method declarations
void mqttCallback(char* topic, byte* payload, unsigned int length);
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);

// Make objects for the PubSubClient
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ------------------------------------------------------------------------------------------------------------------------------
// Construct an LCD object and pass it the I2C address
LiquidCrystal_PCF8574 lcd(LCDI2CADDR);

// ------------------------------------------------------------------------------------------------------------------------------
const byte ROWS                     = 4;                      //four rows
const byte COLS                     = 4;                      //four columns
char keys[ROWS][COLS]               = {{'1', '2', '3', 'A'},
                                       {'4', '5', '6', 'B'},
                                       {'7', '8', '9', 'C'},
                                       {'*', '0', '#', 'D'}};

// Digitran keypad, bit numbers of PCF8574 I/O port
byte rowPins[ROWS]                  = {4, 5, 6, 7};           //connect to the row pinouts of the keypad
byte colPins[COLS]                  = {0, 1, 2, 3};           //connect to the column pinouts of the keypad

Keypad_I2C Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYI2CADDR);

// ------------------------------------------------------------------------------------------------------------------------------
// Variables set on the configuration web page

// Access point configuration
const char thingName[]              = APNAME;                 // Initial AP name, used as SSID of the own Access Point
const char wifiInitialApPassword[]  = APPASSWORD;             // Initial password, used to the own Access Point

// Name of the server we want to get config from
char cHost[]                        = CONFIG_HOST;
int cPort                           = CONFIG_HOST_PORT;
char cPath[]                        = CONFIG_HOST_FILE;
const char* configPath;
const char* tmpMqttServer;
String tamBoxMqtt[MQTT_PARAM];                                // [SERVER,PORT,USER,PASS,TOPIC]
String tamBoxConfig[CONFIG_DEST][CONFIG_PARAM];               // [DEST_A,DEST_B,DEST_C,DEST_D,OWN][ID,SIGN,NAME,NUMOFDEST,TRACKS,EXIT,TOTTRACKS,TYPE]
bool notReceivedConfig;

// Device configuration
char cfgMqttServer[HOST_LEN];
char cfgMqttPort[NUMBER_LEN];
char cfgMqttTopic[STRING_LEN];
char cfgMqttUserName[STRING_LEN];
char cfgMqttUserPass[STRING_LEN];
//char cfgConfServer[HOST_LEN];
//char cfgConfPort[NUMBER_LEN];
//char cfgConfFile[STRING_LEN];
char cfgClientID[STRING_LEN];
char cfgClientName[STRING_LEN];
char cfgClientSign[STRING_LEN];
char cfgDestA[STRING_LEN];
char cfgTrackNumA[NUMBER_LEN];
char cfgDestSignA[STRING_LEN];
char cfgDestExitA[STRING_LEN];
char cfgDestB[STRING_LEN];
char cfgTrackNumB[NUMBER_LEN];
char cfgDestSignB[STRING_LEN];
char cfgDestExitB[STRING_LEN];
char cfgDestC[STRING_LEN];
char cfgTrackNumC[NUMBER_LEN];
char cfgDestSignC[STRING_LEN];
char cfgDestExitC[STRING_LEN];
char cfgDestD[STRING_LEN];
char cfgTrackNumD[NUMBER_LEN];
char cfgDestSignD[STRING_LEN];
char cfgDestExitD[STRING_LEN];
char cfgLedBrightness[NUMBER_LEN];
char cfgBackLight[NUMBER_LEN];
//static char chooserValues[][NUMBER_LEN] = {1, 2};
//static char chooserNames[][STRING_LEN] = {"Enkelspår", "Dubbelspår"};

// ------------------------------------------------------------------------------------------------------------------------------
//Start phases
String topPhase[4]      = {"AP mode",                         // LCD_AP_MODE
                           "Starting up...",                  // LCD_STARTING_UP
                           "Error",                           // LCD_START_ERROR
                           "Rebooting..."};                   // LCD_REBOOTING
String subPhase[8]      = {"192.168.4.1",                     // LCD_AP_MODE
                           "Connecting WiFi",                 // LCD_WIFI_CONNECTING
                           "WiFi connected",                  // LCD_WIFI_CONNECTED
                           "Signal ",                         // LCD_SIGNAL
                           "Loading conf...",                 // LCD_LOADING_CONF
                           "Config loaded",                   // LCD_LOADING_CONF_OK
                           "Starting MQTT...",                // LCD_STARTING_MQTT
                           "Broker connected"};               // LCD_BROKER_CONNECTED
String startError[3]    = {"Broker not found",                // LCD_BROKER_NOT_FOUND
                           "WiFi not found",                  // LCD_WIFI_NOT_FOUND
                           "Local conf used"};                // LCD_LOADING_CONF_NOK

// ------------------------------------------------------------------------------------------------------------------------------
// Define MQTT topic variables

// Variable for topics to subscribe to
const int nbrSubTopics = 4;
String subscribeTopic[nbrSubTopics];

// Variable for topics to publish to
const int nbrPubTopics = 4;
String pubTopic[nbrPubTopics];
String pubTopicContent[nbrPubTopics];

// Often used topics
String pubDeviceStateTopic;                                   // Topic showing the state of device
String pubDirTopicFrwd[NUM_OF_DEST];                          // Traffic direction [upwards, downwards]
String pubDirTopicBckw[NUM_OF_DEST];                          // Traffic direction [upwards, downwards]
const byte NORETAIN     = 0;                                  // Used to publish topics as NOT retained
const byte RETAIN       = 1;                                  // Used to publish topics as retained

String topics[NUM_TOPICS_TXT] = {TOPIC_DID_TXT,               // TOPIC_DID
                                 TOPIC_DSW_TXT,               // TOPIC_DSW
                                 TOPIC_DNAME_TXT,             // TOPIC_DNAME
                                 TOPIC_DTYPE_TXT,             // TOPIC_DTYPE
                                 TOPIC_DSTATE_TXT,            // TOPIC_DSTATE
                                 TOPIC_LOST_TXT,              // TOPIC_LOST
                                 TOPIC_READY_TXT,             // TOPIC_READY
                                 TOPIC_STATE_TXT,             // TOPIC_STATE
                                 TOPIC_REQUEST_TXT,           // TOPIC_REQUEST
                                 TOPIC_ACCEPT_TXT,            // TOPIC_ACCEPT
                                 TOPIC_CANCELED_TXT,          // TOPIC_CANCELED
                                 TOPIC_REJECT_TXT,            // TOPIC_REJECT
                                 TOPIC_IN_TXT,                // TOPIC_IN
                                 TOPIC_OUT_TXT,               // TOPIC_OUT
                                 TOPIC_TRAIN_TXT,             // TOPIC_TRAIN
                                 TOPIC_TRAFFIC_TXT,           // TOPIC_TRAFFIC
                                 TOPIC_DIR_TXT,               // TOPIC_DIR
                                 TOPIC_LEFT_TXT,              // TOPIC_LEFT
                                 TOPIC_RIGHT_TXT};            // TOPIC_RIGHT

// ------------------------------------------------------------------------------------------------------------------------------
// IotWebConfig variables

// Indicate if it is time to reset the client or connect to MQTT
bool needMqttConnect  = false;
bool needReset        = false;

// Make objects for IotWebConf
DNSServer dnsServer;
WebServer server(80);
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

// ------------------------------------------------------------------------------------------------------------------------------
// Define settings to show up on configuration web page
// IotWebConfTextParameter(label, id, valueBuffer, length, type = "text", 
//                         placeholder = NULL, defaultValue = NULL,
//                         customHtml = NULL, visible = true)
    
// Add a new parameter section on the settings web page
IotWebConfParameterGroup webMqttGroup       = IotWebConfParameterGroup("webMqttGroup", "MQTT-inställningar");
IotWebConfTextParameter webMqttServer       = IotWebConfTextParameter(
                                              "MQTT IP-adress", "webMqttServer", cfgMqttServer, STRING_LEN, "", "192.168.1.7");
IotWebConfNumberParameter webMqttPort       = IotWebConfNumberParameter(
                                              "MQTT-port", "webMqttPort", cfgMqttPort, NUMBER_LEN);
IotWebConfTextParameter webMqttUser         = IotWebConfTextParameter(
                                              "MQTT användare", "webMqttUser", cfgMqttUserName, STRING_LEN, "");
IotWebConfPasswordParameter webMqttPass     = IotWebConfPasswordParameter(
                                              "MQTT lösenord", "webMqttPass", cfgMqttUserPass, STRING_LEN, "");
IotWebConfTextParameter webMqttTopic        = IotWebConfTextParameter(
                                              "MQTT root topic", "webMqttTopic", cfgMqttTopic, STRING_LEN, ROOTTOPIC);
/*
IotWebConfParameterGroup webConfGroup       = IotWebConfParameterGroup("webConfGroup", "Konfserver-inställningar");
IotWebConfTextParameter webConfServer       = IotWebConfTextParameter(
                                              "Server IP-adress", "webConfServer", cfgConfServer, STRING_LEN, "", "192.168.1.7");
IotWebConfNumberParameter webConfPort       = IotWebConfNumberParameter(
                                              "Port", "webConfPort", cfgConfPort, NUMBER_LEN);
IotWebConfTextParameter webConfFile         = IotWebConfTextParameter(
                                              "Frågesträng", "webConfFile", cfgConfFile, STRING_LEN, "", "/?id=");
*/
IotWebConfParameterGroup webDeviceGroup     = IotWebConfParameterGroup("webDeviceGroup", "Enhetens inställningar");
IotWebConfTextParameter webClientID         = IotWebConfTextParameter(
                                              "Tamboxens unika id", "webClientID", cfgClientID, STRING_LEN, APNAME);
IotWebConfTextParameter webClientName       = IotWebConfTextParameter(
                                              "Stationens namn", "webClientName", cfgClientName, STRING_LEN, "Charlottendal", "Charlottendal");
IotWebConfTextParameter webClientSign       = IotWebConfTextParameter(
                                              "Stationens signum", "webClientSign", cfgClientSign, STRING_LEN, CLIENTSIGN, "CDA");
IotWebConfNumberParameter webLedBrightness  = IotWebConfNumberParameter(
                                              "LED Ljusstyrka", "webLedBrightness", cfgLedBrightness, NUMBER_LEN, "200", "1..255");
IotWebConfNumberParameter webBackLight      = IotWebConfNumberParameter(
                                              "LCD Motljus", "webBackLight", cfgBackLight, NUMBER_LEN, "255", "1..255");

IotWebConfParameterGroup webModuleGroup     = IotWebConfParameterGroup("webModuleGroup", "Andra stationer");
IotWebConfTextParameter webDestA            = IotWebConfTextParameter(
                                              "Destination A, utfart till vänster", "webDestA", cfgDestA, STRING_LEN, "-", "tambox-4");
IotWebConfTextParameter webDestSignA        = IotWebConfTextParameter(
                                              "Stationens signum", "webDestSignA", cfgDestSignA, STRING_LEN, "-", "VST");
IotWebConfTextParameter webDestExitA        = IotWebConfTextParameter(
                                              "Stationens utfart", "webDestExitA", cfgDestExitA, STRING_LEN, "A", "A-D");
IotWebConfNumberParameter webTrackNumA      = IotWebConfNumberParameter(
                                              "Antal spår", "webTrackNumA", cfgTrackNumA, NUMBER_LEN, "2", "0..4", "min='0' max='4' step='1'");
IotWebConfTextParameter webDestB            = IotWebConfTextParameter(
                                              "Destination B, utfart till höger", "webDestB", cfgDestB, STRING_LEN, "-", "tambox-3");
IotWebConfTextParameter webDestSignB        = IotWebConfTextParameter(
                                              "Stationens signum", "webDestSignB", cfgDestSignB, STRING_LEN, "-", "SAL");
IotWebConfTextParameter webDestExitB        = IotWebConfTextParameter(
                                              "Stationens utfart", "webDestExitB", cfgDestExitB, STRING_LEN, "A", "A-D");
IotWebConfNumberParameter webTrackNumB      = IotWebConfNumberParameter(
                                              "Antal spår", "webTrackNumB", cfgTrackNumB, NUMBER_LEN, "2", "0..4", "min='0' max='4' step='1'");
IotWebConfTextParameter webDestC            = IotWebConfTextParameter(
                                              "Destination C, utfart till vänster", "webDestC", cfgDestC, STRING_LEN, "-", "tambox-2");
IotWebConfTextParameter webDestSignC        = IotWebConfTextParameter(
                                              "Stationens signum", "webDestSignC", cfgDestSignC, STRING_LEN, "-", "GLA");
IotWebConfTextParameter webDestExitC        = IotWebConfTextParameter(
                                              "Stationens utfart", "webDestExitC", cfgDestExitC, STRING_LEN, "A", "A-D");
IotWebConfNumberParameter webTrackNumC      = IotWebConfNumberParameter(
                                              "Antal spår", "webTrackNumC", cfgTrackNumC, NUMBER_LEN, "2", "0..4", "min='0' max='4' step='1'");
IotWebConfTextParameter webDestD            = IotWebConfTextParameter(
                                              "Destination D, utfart till höger", "webDestD", cfgDestD, STRING_LEN, "-", "tambox-5");
IotWebConfTextParameter webDestSignD        = IotWebConfTextParameter(
                                              "Stationens signum", "webDestSignD", cfgDestSignD, STRING_LEN, "-", "BLO");
IotWebConfTextParameter webDestExitD        = IotWebConfTextParameter(
                                              "Stationens utfart", "webDestExitD", cfgDestExitD, STRING_LEN, "A", "A-D");
IotWebConfNumberParameter webTrackNumD      = IotWebConfNumberParameter(
                                              "Antal spår", "webTrackNumD", cfgTrackNumC, NUMBER_LEN, "1", "0..4", "min='0' max='4' step='1'");
//IotWebConfSelectParameter webTrackNumD    = IotWebConfSelectParameter(
//                                              "Antal spår", "webTrackNumD", cfgTrackNumD, NUMBER_LEN, (char*)chooserNames, sizeof(chooserValues) / NUMBER_LEN, NUMBER_LEN);

// ------------------------------------------------------------------------------------------------------------------------------
// Other variables

// Variables to be set after getting configuration from file
String clientID;
String clientName;
int ledBrightness;
int lcdBackLight;
int buzzerPin = BUZZER_PIN;                                   // Define buzzerPin

int destination;
String trainNumber                = "";

// Variables for Train direction and Train Ids used in topics
String useTrack[NUM_DIR_STATES]      = {TOPIC_LEFT_TXT, TOPIC_RIGHT_TXT};
String trainDir[NUM_DIR_STATES]   = {TOPIC_OUT_TXT, TOPIC_IN_TXT};
String runningString[NUM_OF_DEST][3];
String nodeIDTxt[8]               = {DEST_A_TXT,
                                     DEST_B_TXT,
                                     DEST_C_TXT,
                                     DEST_D_TXT,
                                     DEST_CONFIG_TXT,
                                     DEST_ALL_DEST_TXT,
                                     DEST_OWN_STATION_TXT,
                                     DEST_NOT_SELECTED_TXT};
int trainId[NUM_OF_DEST][MAX_NUM_OF_TRACKS];                  // Two tracks per destination
int traffDir[NUM_OF_DEST][MAX_NUM_OF_TRACKS]      = {{DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}};
int lastTraffDir[NUM_OF_DEST][MAX_NUM_OF_TRACKS]  = {{DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}, {DIR_OUT, DIR_IN}};


// Variables to store actual states
int tamBoxState[NUM_OF_DEST][MAX_NUM_OF_TRACKS];              // State per track and destination
String tamBoxStateTxt[NUM_OF_STATES]  = {"Not Used",
                                         "Idle",
                                         "Traffic direction sent",
                                         "Incoming Request",
                                         "Incoming Train",
                                         "Outgoing Request",
                                         "Outgoing Train",
                                         "Lost Connection"};


int destBtnPushed;
int currentTrack                  = DEST_LEFT;
bool tamBoxIdle                   = false;

unsigned long currTime;
unsigned long flashTime;

// Custom character for LCD
byte chr1[8] = {0x4, 0x0, 0xe, 0x11, 0x1f, 0x11, 0x11, 0x0};  // Character Å
byte chr2[8] = {0xa, 0x0, 0xe, 0x11, 0x1f, 0x11, 0x11, 0x0};  // Character Ä
byte chr3[8] = {0xa, 0x0, 0xe, 0x11, 0x11, 0x11, 0xe, 0x0};   // Character Ö
byte chr4[8] = {0x4, 0x0, 0xe, 0x1, 0xf, 0x11, 0xf, 0x0};     // Character å
byte chr5[8] = {0xa, 0x0, 0xe, 0x1, 0xf, 0x11, 0xf, 0x0};     // Character ä
byte chr6[8] = {0xa, 0x0, 0xe, 0x11, 0x11, 0x11, 0xe, 0x0};   // Character ö
byte chr7[8] = {0x2, 0x4, 0x1f, 0x10, 0x1c, 0x10, 0x1f, 0x0}; // Character É
byte chr8[8] = {0x2, 0x4, 0xe, 0x11, 0x1f, 0x10, 0xe, 0x0};   // Character é

#ifdef DEBUG
String dbText                     = "Setup           : ";     // Debug text. Occurs first in every debug output
#endif

//HttpClient http = HttpClient(wifiClient, cHost, cPort);
HttpClient http(wifiClient, cHost, cPort);

/* ------------------------------------------------------------------------------------------------------------------------------
 *  Standard setup function
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setup() {

  // ----------------------------------------------------------------------------------------------------------------------------
  // Setup Arduino IDE serial monitor for "debugging"
#ifdef DEBUG
  Serial.begin(115200);Serial.println("");
  Serial.println(dbText+__LINE__+"  Starting setup");
#endif

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up the buzzer
  pinMode(buzzerPin, OUTPUT);                                 //Set buzzerPin as output

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up LED drivers
  FastLED.addLeds<LED_DRIVE_TYPE, FASTLED_PIN, COLOR_ORDER>(rgbLed, NUM_LED_DRIVERS);
  FastLED.setBrightness(LED_BRIGHTNESS);

  // ----------------------------------------------------------------------------------------------------------------------------
  // IotWebConfig start
#ifdef DEBUG
  Serial.println(dbText+__LINE__+"  IotWebConf setup");
#endif

  // Adding items to each group
  webMqttGroup.addItem(&webMqttServer);                       // MQTT Broker IP-address
  webMqttGroup.addItem(&webMqttPort);                         // MQTT Broker Port
  webMqttGroup.addItem(&webMqttUser);                         // MQTT Broker user
  webMqttGroup.addItem(&webMqttPass);                         // MQTT Broker password
  webMqttGroup.addItem(&webMqttTopic);                        // MQTT root topic
/*
  webConfGroup.addItem(&webConfServer);                       // Configuration server IP-address
  webConfGroup.addItem(&webConfPort);                         // Configuration server Port
  webConfGroup.addItem(&webConfFile);                         // Configuration server query string
*/
  webDeviceGroup.addItem(&webClientID);                       // MQTT Client uniqe ID
  webDeviceGroup.addItem(&webClientName);                     // Own Station name
  webDeviceGroup.addItem(&webClientSign);                     // Own Station signature
  webDeviceGroup.addItem(&webLedBrightness);                  // LED Brightness
  webDeviceGroup.addItem(&webBackLight);                      // LCD Backlight
  webModuleGroup.addItem(&webDestA);                          // Destination A id
  webModuleGroup.addItem(&webDestSignA);                      // Destination A signature
  webModuleGroup.addItem(&webDestExitA);                      // Destination A exit
  webModuleGroup.addItem(&webTrackNumA);                      // Destination A number of tracks
  webModuleGroup.addItem(&webDestB);                          // Destination B id
  webModuleGroup.addItem(&webDestSignB);                      // Destination B signature
  webModuleGroup.addItem(&webDestExitB);                      // Destination B exit
  webModuleGroup.addItem(&webTrackNumB);                      // Destination B number of tracks
  webModuleGroup.addItem(&webDestC);                          // Destination C id
  webModuleGroup.addItem(&webDestSignC);                      // Destination C signature
  webModuleGroup.addItem(&webDestExitC);                      // Destination C exit
  webModuleGroup.addItem(&webTrackNumC);                      // Destination C number of tracks
  webModuleGroup.addItem(&webDestD);                          // Destination D id
  webModuleGroup.addItem(&webDestSignD);                      // Destination D signature
  webModuleGroup.addItem(&webDestExitD);                      // Destination D exit
  webModuleGroup.addItem(&webTrackNumD);                      // Destination D number of tracks
  
  iotWebConf.setStatusPin(STATUS_PIN);
#ifdef DEBUG
  Serial.println(dbText+__LINE__+"  Status pin = "+STATUS_PIN);
#endif

//  iotWebConf.setConfigPin(CONFIG_PIN);
#ifdef DEBUG
//  Serial.println(dbText+__LINE__+"  Config pin = "+CONFIG_PIN);
#endif

  // Adding up groups to show on config web page
  iotWebConf.addParameterGroup(&webMqttGroup);                // MQTT Settings
//  iotWebConf.addParameterGroup(&webConfGroup);                // Configuration Server Settings
  iotWebConf.addParameterGroup(&webDeviceGroup);              // Device Settings
  iotWebConf.addParameterGroup(&webModuleGroup);              // Other Stations

  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);

  // Validate data input
  iotWebConf.setFormValidator(&formValidator);

  // Show/set AP timeout on web page
  iotWebConf.getApTimeoutParameter()->visible = true;

  // Setting default configuration
  bool validConfig = iotWebConf.init();
  if (!validConfig) {
    // Set configuration default values
    tamBoxMqtt[USER]                  = "";
    tamBoxMqtt[PASS]                  = "";
    tamBoxMqtt[TOPIC]                 = ROOTTOPIC;
//    cHost                             = CONFIG_HOST;
//    cPort                             = CONFIG_HOST_PORT;
//    cFile                             = CONFIG_HOST_FILE;
    clientName                        = APNAME;
    String tmpNo                      = String(random(2147483647));
    clientID                          = clientName+"-"+tmpNo;
    tamBoxConfig[OWN][SIGN]           = CLIENTSIGN;
    for (int i=0; i<NUM_OF_DEST; i++) {
      tamBoxConfig[i][ID]             = NOT_USED_TXT;
      tamBoxConfig[i][SIGN]           = NOT_USED_TXT;
      tamBoxConfig[i][NAME]           = NOT_USED_TXT;
      tamBoxConfig[i][EXIT]           = DEST_A_TXT;
      tamBoxConfig[i][TOTTRACKS]      = "0";
    }
    ledBrightness                     = LED_BRIGHTNESS;
    lcdBackLight                      = LCD_BACKLIGHT;
  }

  else {
    clientID                          = String(cfgClientID);
    clientID.toLowerCase();
    clientName                        = String(cfgClientName);
//    cHost                             = cfgConfServer;
//    cPort                             = cfgConfPort;
//    cFile                             = cfgConfFile;
    tamBoxConfig[OWN][SIGN]           = String(cfgClientSign);
    tamBoxMqtt[SERVER]                = String(cfgMqttServer);
    tamBoxMqtt[PORT]                  = String(cfgMqttPort);
    tamBoxMqtt[USER]                  = String(cfgMqttUserName);
    tamBoxMqtt[PASS]                  = String(cfgMqttUserPass);
    tamBoxMqtt[TOPIC]                 = String(cfgMqttTopic);
    tamBoxConfig[DEST_A][ID]          = String(cfgDestA);
    tamBoxConfig[DEST_A][SIGN]        = String(cfgDestSignA);
    tamBoxConfig[DEST_A][NAME]        = NOT_USED_TXT;
    tamBoxConfig[DEST_A][EXIT]        = String(cfgDestExitA);
    tamBoxConfig[DEST_A][TOTTRACKS]   = String(cfgTrackNumA);
    tamBoxConfig[DEST_B][ID]          = String(cfgDestB);
    tamBoxConfig[DEST_B][SIGN]        = String(cfgDestSignB);
    tamBoxConfig[DEST_B][NAME]        = NOT_USED_TXT;
    tamBoxConfig[DEST_B][EXIT]        = String(cfgDestExitB);
    tamBoxConfig[DEST_B][TOTTRACKS]   = String(cfgTrackNumB);
    tamBoxConfig[DEST_C][ID]          = String(cfgDestC);
    tamBoxConfig[DEST_C][SIGN]        = String(cfgDestSignC);
    tamBoxConfig[DEST_C][NAME]        = NOT_USED_TXT;
    tamBoxConfig[DEST_C][EXIT]        = String(cfgDestExitC);
    tamBoxConfig[DEST_C][TOTTRACKS]   = String(cfgTrackNumC);
    tamBoxConfig[DEST_D][ID]          = String(cfgDestD);
    tamBoxConfig[DEST_D][SIGN]        = String(cfgDestSignD);
    tamBoxConfig[DEST_D][NAME]        = NOT_USED_TXT;
    tamBoxConfig[DEST_D][EXIT]        = String(cfgDestExitD);
    tamBoxConfig[DEST_D][TOTTRACKS]   = String(cfgTrackNumD);
    ledBrightness                     = atoi(cfgLedBrightness);
    lcdBackLight                      = atoi(cfgBackLight);
  }

  notReceivedConfig = true;
  FastLED.setBrightness(ledBrightness);

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up LCD
  // The begin call takes the width and height. This
  // Should match the number provided to the constructor.

  subPhase[LCD_AP_MODE] = String(iotWebConf.getThingName());
  lcd.begin(LCD_MAX_LENGTH, LCD_MAX_ROWS);
  lcd.setBacklight(lcdBackLight);
  // Only 8 custom characters can be defined into the LCD
  lcd.createChar(0, chr1);                                    // Create character Å
  lcd.createChar(1, chr2);                                    // Create character Ä
  lcd.createChar(2, chr3);                                    // Create character Ö
  lcd.createChar(3, chr4);                                    // Create character å
  lcd.createChar(4, chr5);                                    // Create character ä
  lcd.createChar(5, chr6);                                    // Create character ö
  lcd.createChar(6, chr7);                                    // Create character É
  lcd.createChar(7, chr8);                                    // Create character é
  lcd.clear();
  lcd.setCursor(LCD_MAX_LENGTH / 2 - centerText(topPhase[LCD_AP_MODE]), LCD_FIRST_ROW);
  lcd.print(topPhase[LCD_AP_MODE]);
  lcd.setCursor(LCD_MAX_LENGTH / 2 - centerText(subPhase[LCD_AP_MODE]), LCD_SECOND_ROW);
  lcd.print(subPhase[LCD_AP_MODE]);


  // Set up required URL handlers for the config web pages
  server.on("/", handleRoot);
  server.on("/config", []{iotWebConf.handleConfig();});
  server.onNotFound([](){iotWebConf.handleNotFound();});

  delay(2000);                                                // Wait for IotWebServer to start network connection

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up i2c key board

  Keypad.begin();
    
  // ----------------------------------------------------------------------------------------------------------------------------
  // test LED

  testLed();

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set default values

  setDefaults();
  destination = DEST_NOT_SELECTED;
  currTime = millis();
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Main program loop
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void loop() {

#ifdef DEBUG
String db1Text = "loop            : ";
#endif
  // Check connection to the MQTT broker. If no connection, try to reconnect
  if (needMqttConnect) {
    if (mqttConnect()) {
      needMqttConnect = false;
    }
  }

  else if ((iotWebConf.getState() == iotwebconf::OnLine) && (!mqttClient.connected())) {
#ifdef DEBUG
    Serial.println(db1Text+__LINE__+"  MQTT reconnect");
#endif
    mqttConnect();
  }

  if (needReset) {
#ifdef DEBUG
    Serial.println("Rebooting after 1 second.");
#endif
    lcd.clear();
    lcd.home();
    lcd.print(topPhase[LCD_REBOOTING]);
    iotWebConf.delay(1000);
    ESP.restart();
  }

  // Run repetitive jobs
  mqttClient.loop();                                          // Wait for incoming MQTT messages
  iotWebConf.doLoop();                                        // Check for IotWebConfig actions

  char key = Keypad.getKey();                                 // Get key input
  if (key) {
    beep(4, KEY_CLK);                                         // Key click 4ms
    keyReceived(key);
  }

  if (millis() - currTime > LCD_TOOGLE_TRACK) {               // Toogle double track view every 2 sec
    toogleTrack();
    currTime = millis();
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 * Key received
 *
 *  procedurs
 *  Incomming request on normal track from destination A
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
  String db1Text = "keyReceived     : ";
  Serial.println(db1Text+__LINE__+"  Key pushed: "+key);
  Serial.println(db1Text+__LINE__+"  Destination in = "+nodeIDTxt[destination]);
#endif
  String signatur;
  int track = DEST_LEFT;
  int sendMqtt = TOPIC_NO_TOPIC;

  switch (key) {
    case '*':                                                                                   // NOK, Not accepted
      if (destination < DEST_CONFIG) {
        if (tamBoxConfig[destination][TRACKS].toInt() == DOUBLE_TRACK) {
          if (tamBoxState[destination][DEST_RIGHT] == STATE_INREQUEST ||
              tamBoxState[destination][DEST_RIGHT] == STATE_INTRAIN) {
            track = DEST_RIGHT;
          }
        }
#ifdef DEBUG
        Serial.println(db1Text+__LINE__+"  Destination: "+nodeIDTxt[destination]+
                                        ", State in: "+tamBoxStateTxt[tamBoxState[destination][track]]+
                                        " for "+useTrack[track]+" track");
        Serial.println(db1Text+__LINE__+"  Destination selected "+destBtnPushed+" times");
#endif

        switch (tamBoxState[destination][track]) {
          case STATE_INREQUEST:                                                                 // Reject Incoming request
            tamBoxState[destination][track] = STATE_IDLE;
            sendMqtt = TOPIC_REJECT;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
          break;

          case STATE_TRAFDIR:                                                                   // Cancel Traffic direction request
            tamBoxState[destination][track] = STATE_IDLE;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
          break;
//----------------------------------------------------------------------------------------------
          case STATE_OUREQUEST:                                                                 // Cancel Outgoing request
            tamBoxIdle = false;
            trainNumber = "";
            tamBoxState[destination][track] = STATE_IDLE;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
            if (trainId[destination][track] != DEST_ZERO_TRAIN) {
              sendMqtt = TOPIC_CANCELED;
            }
            lcd.noBlink();
            lcd.noCursor();
            setTxt(LCD_TAM_CANCELED, destination, DEST_ZERO_TRAIN);
            delay(LCD_SHOW_TEXT);                                                               // Show string for 3 sec
            tamBoxIdle = true;
          break;
        }
//----------------------------------------------------------------------------------------------
        if (sendMqtt != TOPIC_NO_TOPIC) {
          signatur = tamBoxConfig[destination][SIGN];
          signatur.toLowerCase();
          mqttPublish(tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+                                      // mqtt_h0
                      tamBoxConfig[OWN][ID]+TOPIC_DIVIDER_TXT+                                  // tambox-x
                      signatur+TOPIC_DIVIDER_TXT+                                               // Signature
                      topics[TOPIC_TRAFFIC]+TOPIC_DIVIDER_TXT+                                  // traffic
                      useTrack[traffDir[destination][track]]+TOPIC_DIVIDER_TXT+                 // Track {left,right}
                      TOPIC_TRAIN_TXT+TOPIC_DIVIDER_TXT+                                        // train
                      topics[sendMqtt],                                                         // Order {request, accept, reject, canceled}
                      String(trainId[destination][track]), NORETAIN);                           // Train number
        }

        if (sendMqtt == TOPIC_CANCELED || sendMqtt == TOPIC_REJECT) {
          trainId[destination][track] = DEST_ZERO_TRAIN;
        }
        setNodeTxt(destination, track);
        updateLcd(DEST_ALL_DEST);
        destination = DEST_NOT_SELECTED;
      }

      else if (destination == DEST_NOT_SELECTED) {
        tamBoxIdle = false;
        updateLcd(DEST_OWN_STATION);
        delay(LCD_SHOW_TEXT);
        updateLcd(DEST_ALL_DEST);
      }
      tamBoxIdle = true;
      destBtnPushed = 0;
    break;
//----------------------------------------------------------------------------------------------
    case '#':                                                                                   // OK, Accepted
      if (destination < DEST_CONFIG) {
        if (tamBoxConfig[destination][TRACKS].toInt() == DOUBLE_TRACK) {
          if (tamBoxState[destination][DEST_RIGHT] == STATE_INREQUEST ||
              tamBoxState[destination][DEST_RIGHT] == STATE_INTRAIN) {
            if (destBtnPushed < 2) {
              track = DEST_RIGHT;
            }
          }
        }
#ifdef DEBUG
        Serial.println(db1Text+__LINE__+"  Destination: "+nodeIDTxt[destination]+
                                        ", State in: "+tamBoxStateTxt[tamBoxState[destination][track]]+
                                        " for "+useTrack[track]+" track");
        Serial.println(db1Text+__LINE__+"  Destination selected "+destBtnPushed+" times");
#endif
        switch (tamBoxState[destination][track]) {
          case STATE_INTRAIN:                                                                   // Report Incoming train
            tamBoxState[destination][track] = STATE_IDLE;
            sendMqtt = TOPIC_IN;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
          break;
//----------------------------------------------------------------------------------------------
          case STATE_INREQUEST:                                                                 // Accept Incoming request
            tamBoxState[destination][track] = STATE_INTRAIN;
            sendMqtt = TOPIC_ACCEPT;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
          break;
//----------------------------------------------------------------------------------------------
          case STATE_OUREQUEST:                                                                 // Confirm Outgoing request
            trainId[destination][track] = trainNumber.toInt();
            lcd.noCursor();
            lcd.noBlink();
            trainNumber = "";
            sendMqtt = TOPIC_REQUEST;
          break;
        }
//----------------------------------------------------------------------------------------------
        if (sendMqtt != TOPIC_NO_TOPIC) {
          signatur = tamBoxConfig[destination][SIGN];
          signatur.toLowerCase();
          mqttPublish(tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+                                      // mqtt_h0
                      tamBoxConfig[OWN][ID]+TOPIC_DIVIDER_TXT+                                  // tambox-x
                      signatur+TOPIC_DIVIDER_TXT+                                               // Signature
                      TOPIC_TRAFFIC_TXT+TOPIC_DIVIDER_TXT+                                      // traffic
                      useTrack[traffDir[destination][track]]+TOPIC_DIVIDER_TXT+                 // Track
                      TOPIC_TRAIN_TXT+TOPIC_DIVIDER_TXT+                                        // Train
                      topics[sendMqtt],                                                         // Topic
                      String(trainId[destination][track]), NORETAIN);                           // Train number
        }

        if (sendMqtt == TOPIC_IN) {
          trainId[destination][track] = DEST_ZERO_TRAIN;
        }

        setDirectionTxt(destination, track);
        setNodeTxt(destination, track);
        updateLcd(DEST_ALL_DEST);
        destination = DEST_NOT_SELECTED;
#ifdef DEBUG
        Serial.println(db1Text+__LINE__+"  Destination out "+nodeIDTxt[destination]+" set");
#endif
      }

      else if (destination == DEST_NOT_SELECTED) {
        tamBoxIdle = false;
        updateLcd(DEST_OWN_STATION);
        delay(LCD_SHOW_TEXT);
        updateLcd(DEST_ALL_DEST);
      }
      tamBoxIdle = true;
      destBtnPushed = 0;
    break;
//----------------------------------------------------------------------------------------------
    case 'A':                                                                                   // Destinations
    case 'B':
    case 'C':
    case 'D':
      switch (key) {
        case 'A':                                                                               // Set Direction A
          destination = DEST_A;
        break;
//----------------------------------------------------------------------------------------------
        case 'B':                                                                               // Set Direction B
          destination = DEST_B;
        break;
//----------------------------------------------------------------------------------------------
        case 'C':                                                                               // Set Direction C
          destination = DEST_C;
        break;
//----------------------------------------------------------------------------------------------
        case 'D':                                                                               // Set Direction D
          destination = DEST_D;
        break;
      }
//----------------------------------------------------------------------------------------------
      destBtnPushed++;
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+"  Destination: "+nodeIDTxt[destination]+
                                      ", State in: "+tamBoxStateTxt[tamBoxState[destination][DEST_LEFT]]+
                                      " for "+useTrack[DEST_LEFT]+" track");
      Serial.println(db1Text+__LINE__+"  Destination: "+nodeIDTxt[destination]+
                                      ", State in: "+tamBoxStateTxt[tamBoxState[destination][DEST_RIGHT]]+
                                      " for "+useTrack[DEST_RIGHT]+" track");
      Serial.println(db1Text+__LINE__+"  Destination selected "+destBtnPushed+" times");
#endif
      if (tamBoxConfig[destination][TRACKS].toInt() == DOUBLE_TRACK) {
        if (tamBoxState[destination][DEST_RIGHT] == STATE_INTRAIN) {                            // Incoming train on right track
          if (destBtnPushed == 1) {
            track = DEST_RIGHT;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+"  Destination: "+nodeIDTxt[destination]+
                                            " track "+useTrack[track]+
                                            " is in state "+tamBoxStateTxt[tamBoxState[destination][track]]);
#endif
          }
        }
      }

      switch (tamBoxState[destination][track]) {
        case STATE_IDLE:
          traffDir[destination][track] = DIR_OUT;
          signatur = tamBoxConfig[destination][SIGN];
          signatur.toLowerCase();
          mqttPublish(tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+                                      // mqtt_h0
                      tamBoxConfig[OWN][ID]+TOPIC_DIVIDER_TXT+                                  // tambox-x
                      signatur+TOPIC_DIVIDER_TXT+                                               // Signature
                      TOPIC_TRAFFIC_TXT+TOPIC_DIVIDER_TXT+                                      // traffic
                      useTrack[traffDir[destination][track]]+TOPIC_DIVIDER_TXT+                 // Track
                      TOPIC_DIR_TXT+TOPIC_DIVIDER_TXT+                                          // Direction
                      TOPIC_STATE_TXT,                                                          // state
                      trainDir[DIR_OUT], NORETAIN);                                             // {out, in}
          tamBoxState[destination][track] = STATE_TRAFDIR;
#ifdef DEBUG
          Serial.println(db1Text+__LINE__+"  State change for "+useTrack[track]+"!");
          Serial.println(db1Text+__LINE__+"  Traffic direction "+
                                          useTrack[traffDir[destination][track]]+" set");
#endif
        break;
//----------------------------------------------------------------------------------------------
        case STATE_OUREQUEST: 
          tamBoxIdle = false;
          setTxt(LCD_TAM_CANCEL, destination, DEST_ZERO_TRAIN);
        break;
//----------------------------------------------------------------------------------------------
        case STATE_INTRAIN: 
          tamBoxIdle = false;
          setTxt(LCD_ARRIVAL, destination, DEST_ZERO_TRAIN);
        break;
//----------------------------------------------------------------------------------------------
        default:
          destination = DEST_NOT_SELECTED;
        break;
      }
    break;
//----------------------------------------------------------------------------------------------
    default:                                                                                    // Numbers
      if (destination < DEST_NOT_SELECTED) {
        switch (tamBoxState[destination][DEST_LEFT]) {
          case STATE_OUREQUEST:
            tamBoxIdle = false;
            trainNumber = trainNumber+key;
            setTxt(LCD_TRAIN_ID, destination, trainNumber.toInt());
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+"  Train number = "+trainNumber);
#endif
          break;
        }
//----------------------------------------------------------------------------------------------
      }

      else if (destination == DEST_NOT_SELECTED) {
        tamBoxIdle = false;
        updateLcd(DEST_OWN_STATION);
        delay(LCD_SHOW_TEXT);
        updateLcd(DEST_ALL_DEST);
        tamBoxIdle = true;
      }
    break;
  }
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
  Serial.println(db1Text+__LINE__+"  Destination out "+nodeIDTxt[destination]+" set");
  Serial.println(db1Text+__LINE__+"  Destination: "+nodeIDTxt[destination]+
                                  ", State out: "+tamBoxStateTxt[tamBoxState[destination][track]]+
                                  " for "+useTrack[track]+" track");
#endif
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function that gets called when a info message is received
 *
 *  handleInfo(tambox-4, {lost,ready})
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void handleInfo(String clt, String msg) {

#ifdef DEBUG
  String db1Text = "handleInfo      : ";
#endif
  for (int i=0; i<NUM_OF_DEST; i++) {
    if (tamBoxConfig[i][ID] == clt) {

      if (msg == TOPIC_LOST_TXT) {                                                              // Connection lost
        for (int j=0; j<MAX_NUM_OF_TRACKS; j++) {
          lastTraffDir[i][j]  = traffDir[i][j];
          traffDir[i][j]      = DIR_LOST;
          if (tamBoxState[i][j] != STATE_NOTUSED) {
            tamBoxState[i][j] = STATE_LOST;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+"  State change to Lost for "+
                                          useTrack[j]+" Track!");
#endif
            setLed(i, STATE_LOST);
          }
          setDirectionTxt(i, j);
        }
        updateLcd(i);
#ifdef DEBUG
        Serial.println(db1Text+__LINE__+"  Connection lost to "+clt);
#endif
      }

      else if (msg == TOPIC_READY_TXT) {                                                        // Connection restored
        for (int j=0; j<MAX_NUM_OF_TRACKS; j++) {
          traffDir[i][j] = lastTraffDir[i][j];
          if (tamBoxState[i][j] != STATE_NOTUSED) {
            tamBoxState[i][j] = STATE_IDLE;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+"  State change to Idle for "+
                                            useTrack[j]+" Track!");
#endif
            setLed(i, STATE_IDLE);
            trainId[i][j] = DEST_ZERO_TRAIN;
            setNodeTxt(i, j);
          }
          setDirectionTxt(i, j);
        }
       updateLcd(i);
#ifdef DEBUG
        Serial.println(db1Text+__LINE__+" Connection restored to "+clt);
#endif
      }
    }
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function that gets called when a direction message is received
 *
 *  handleDirection(tambox-4, left, {in,out})
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void handleDirection(String clt, String trk, String msg) {

#ifdef DEBUG
  String db1Text = "handleDirection : ";
  String setTo;
#endif
  String signatur;
  int sendMqtt = TOPIC_NO_TOPIC;
  int dir = DIR_IN;
  int track = DEST_LEFT;

  for (int i=0; i<NUM_OF_DEST; i++) {
    if (tamBoxConfig[i][ID] == clt) {
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination: "+nodeIDTxt[i]+
                                      ", State in: "+tamBoxStateTxt[tamBoxState[i][track]]+
                                      " for "+trk+" track");
#endif
      if (tamBoxConfig[i][TRACKS].toInt() == DOUBLE_TRACK) {
        if (trk == useTrack[DEST_LEFT]) {                                                       // If double track change track to the oposite track
          track = DEST_RIGHT;
        }
      }

      switch (tamBoxState[i][track]) {
        case STATE_IDLE:                                                                        // Track idle 
          if (tamBoxIdle) {                                                                     // Tambox idle, acknowledge direction out
            traffDir[i][track] = DIR_IN;
            setDirectionTxt(i, DIR_IN);
            updateLcd(i);
            sendMqtt = TOPIC_STATE;
#ifdef DEBUG
            setTo = trainDir[traffDir[i][track]];
#endif
          }
          else {                                                                                // Tambox busy, reject  direction out
            traffDir[i][track] = DIR_OUT;
            sendMqtt = TOPIC_STATE;
          }
        break;
//----------------------------------------------------------------------------------------------
        case STATE_TRAFDIR:
          if (msg == TOPIC_IN_TXT) {                                                            // Direction out acknowledged
            tamBoxIdle = false;
            tamBoxState[i][track] = STATE_OUREQUEST;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
            setTxt(LCD_TRAIN, i, DEST_ZERO_TRAIN);
#ifdef DEBUG
            setTo = msg;
#endif
          }

          else {                                                                                // Direction out rejected
            traffDir[i][track] = DIR_OUT;
            tamBoxIdle = false;
            tamBoxState[i][track] = STATE_IDLE;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
            setTxt(LCD_TRAINDIR_NOK, i, DEST_ZERO_TRAIN);
            beep(BEEP_DURATION, BEEP_OK);
            if (LCD_SHOW_TEXT-BEEP_DURATION > 0) {
              delay(LCD_SHOW_TEXT-BEEP_DURATION);                                               // Show string for 3 sec
            }
            tamBoxIdle = true;
            updateLcd(DEST_ALL_DEST);
#ifdef DEBUG
            setTo = trainDir[traffDir[i][track]];
#endif
          }
        break;
//----------------------------------------------------------------------------------------------
        case STATE_OUREQUEST:                                                                   // Reject Direction out
          sendMqtt = TOPIC_STATE;
        break;
#ifdef DEBUG
        Serial.println(db1Text+__LINE__+" Traffic direction from "+clt+" on track "+useTrack[track]+" set to "+setTo);
#endif
      }
//----------------------------------------------------------------------------------------------
      if (sendMqtt != TOPIC_NO_TOPIC) {
        signatur = tamBoxConfig[i][SIGN];
        signatur.toLowerCase();
        mqttPublish(tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+                                        // mqtt_h0
                    tamBoxConfig[OWN][ID]+TOPIC_DIVIDER_TXT+                                    // tambox-x
                    signatur+TOPIC_DIVIDER_TXT+                                                 // Signature
                    TOPIC_TRAFFIC_TXT+TOPIC_DIVIDER_TXT+                                        // traffic
                    useTrack[track]+TOPIC_DIVIDER_TXT+                                          // Track
                    TOPIC_DIR_TXT+TOPIC_DIVIDER_TXT+                                            // Direction
                    topics[sendMqtt],                                                           // state
                    trainDir[traffDir[i][track]], NORETAIN);                                    // {in,out}

      }
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination: "+nodeIDTxt[i]+
                                      ", State out: "+tamBoxStateTxt[tamBoxState[i][track]]+
                                      " for "+useTrack[track]+" track");
#endif
    }
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function that gets called when a train message is received
 *
 *  handleTrain(tambox-2, left, request, 233)
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void handleTrain(String clt, String trk, String ord, String msg) {

#ifdef DEBUG
  String db1Text = "handleTrain     : ";
#endif
  int track = DEST_LEFT;

  for (int i=0; i<NUM_OF_DEST; i++) {
    if (tamBoxConfig[i][ID] == clt) {
      destination = i;
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination: "+nodeIDTxt[i]+
                                      ", State in: "+tamBoxStateTxt[tamBoxState[i][track]]+
                                      " for "+useTrack[track]+" track");
#endif
      if (tamBoxConfig[i][TRACKS].toInt() == DOUBLE_TRACK) {
        if (trk == useTrack[DEST_LEFT]) {                                                       // If double track change track to the oposite track
          track = DEST_RIGHT;
        }
      }

      if (ord == TOPIC_REQUEST_TXT) {                                                           // Incoming request
        switch (tamBoxState[i][track]) {
          case STATE_IDLE:
            trainId[i][track] = msg.toInt();
            setNodeTxt(i, track);
            updateLcd(i);
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+" Incoming request from: "+clt+
                                            " with train "+trainId[i][track]+
                                            " on "+useTrack[track]+" track");
#endif
            tamBoxIdle = false;
            tamBoxState[i][track] = STATE_INREQUEST;
#ifdef DEBUG
            Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
            setTxt(LCD_TAM_ACCEPT, i, DEST_ZERO_TRAIN);
            beep(BEEP_DURATION, BEEP_OK);
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:
            Serial.println(db1Text+__LINE__+" Wrong state for incoming request");
          break;
#endif
        }
        break;
      }
//----------------------------------------------------------------------------------------------
      else if (ord == TOPIC_ACCEPT_TXT) {                                                       // Incoming accept
        switch (tamBoxState[i][track]) {
          case STATE_OUREQUEST:
            if (trainId[i][track] == msg.toInt()) {
#ifdef DEBUG
              Serial.println(db1Text+__LINE__+" Outgoing request to: "+clt+
                                              " with train "+trainId[i][track]+" accepted");
#endif
              tamBoxIdle = false;
              tamBoxState[i][track] = STATE_OUTTRAIN;
#ifdef DEBUG
              Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
              setTxt(LCD_TAM_OK, i, DEST_ZERO_TRAIN);
              beep(BEEP_DURATION, BEEP_OK);
              if (LCD_SHOW_TEXT-BEEP_DURATION > 0) {
                delay(LCD_SHOW_TEXT-BEEP_DURATION);                                             // Show string for 3 sec
              }
              updateLcd(DEST_ALL_DEST);
              tamBoxIdle = true;
            }

#ifdef DEBUG
            else {
              Serial.println(db1Text+__LINE__+" Wrong train for incoming accept: "+msg);
            }
#endif
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:
            Serial.println(db1Text+__LINE__+" Wrong state for incoming accept");
          break;
#endif
        }
        break;
      }
//----------------------------------------------------------------------------------------------
      else if (ord == TOPIC_REJECT_TXT) {                                                       // Incoming reject
        switch (tamBoxState[i][track]) {
          case STATE_OUREQUEST:
            if (trainId[i][track] == msg.toInt()) {
#ifdef DEBUG
              Serial.println(db1Text+__LINE__+" Outgoing request to: "+clt+
                                              " with train "+msg+" rejected");
#endif
              trainId[i][track] = DEST_ZERO_TRAIN;
              tamBoxIdle = false;
              tamBoxState[i][track] = STATE_IDLE;
#ifdef DEBUG
              Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
              setTxt(LCD_TAM_NOK, i, DEST_ZERO_TRAIN);
              beep(BEEP_DURATION, BEEP_NOK);
              if (LCD_SHOW_TEXT-BEEP_DURATION > 0) {
                delay(LCD_SHOW_TEXT-BEEP_DURATION);                                             // Show string for 3 sec
              }
              setNodeTxt(i,track);
              updateLcd(DEST_ALL_DEST);
              tamBoxIdle = true;
            }

#ifdef DEBUG
            else {
              Serial.println(db1Text+__LINE__+" Wrong train for incoming reject: "+msg);
            }
#endif
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:
            Serial.println(db1Text+__LINE__+" Wrong state for incoming reject");
          break;
#endif
        }
        break;
      }
//----------------------------------------------------------------------------------------------
      else if (ord == TOPIC_CANCELED_TXT) {                                                     // Incoming cancel
        switch (tamBoxState[i][track]) {
          case STATE_INREQUEST:
            if (trainId[i][track] == msg.toInt()) {
#ifdef DEBUG
              Serial.println(db1Text+__LINE__+" Incoming request from: "+clt+
                                              " with train "+msg+" canceled");
#endif
              trainId[i][track] = DEST_ZERO_TRAIN;
              tamBoxIdle = false;
              tamBoxState[i][track] = STATE_IDLE;
#ifdef DEBUG
              Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
              setTxt(LCD_TAM_CANCELED, i, DEST_ZERO_TRAIN);
              beep(BEEP_DURATION, BEEP_NOK);
              if (LCD_SHOW_TEXT-BEEP_DURATION > 0) {
                delay(LCD_SHOW_TEXT-BEEP_DURATION);                                             // Show string for 3 sec
              }
              setNodeTxt(i,track);
              updateLcd(DEST_ALL_DEST);
              tamBoxIdle = true;
            }

#ifdef DEBUG
            else {
              Serial.println(db1Text+__LINE__+" Wrong train for incoming cancel: "+msg);
            }
#endif
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:
            Serial.println(db1Text+__LINE__+" Wrong state for incoming cancel");
          break;
#endif
        }
      }
//----------------------------------------------------------------------------------------------
      else if (ord == TOPIC_IN_TXT) {                                                           // Incoming arrivel report
        switch (tamBoxState[i][track]) {
          case STATE_OUTTRAIN:
            if (trainId[i][track] == msg.toInt()) {
              trainId[i][track] = DEST_ZERO_TRAIN;
#ifdef DEBUG
              Serial.println(db1Text+__LINE__+" Outgoing train to: "+clt+
                                              " with train "+msg+" arrived");
#endif
              tamBoxIdle = false;
              tamBoxState[i][track] = STATE_IDLE;
#ifdef DEBUG
              Serial.println(db1Text+__LINE__+" State change for "+useTrack[track]+" track!");
#endif
              setTxt(LCD_ARRIVAL_OK, i, DEST_ZERO_TRAIN);
              beep(BEEP_DURATION, BEEP_OK);
              if (LCD_SHOW_TEXT-BEEP_DURATION > 0) {
                delay(LCD_SHOW_TEXT-BEEP_DURATION);                                             // Show string for 3 sec
              }
              setNodeTxt(i,track);
              updateLcd(DEST_ALL_DEST);
              tamBoxIdle = true;
            }

#ifdef DEBUG
            else {
              Serial.println(db1Text+__LINE__+" Wrong train for incoming arrivel report: "+msg);
            }
#endif
          break;
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
          default:
            Serial.println(db1Text+__LINE__+" Wrong state for incoming arrivel report");
          break;
#endif
        }
        break;
      }

#ifdef DEBUG
      else {
        Serial.println(db1Text+__LINE__+" Not supported order: "+ord);
        break;
      }
#endif
    }
  }
#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" Destination: "+nodeIDTxt[destination]+
                                  ", State out: "+tamBoxStateTxt[tamBoxState[destination][track]]+
                                  " for "+useTrack[track]+" track");
#endif
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Set string
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setTxt(int str, int dest, int train) {

  String string[10]   = {LCD_TRAIN_TXT,             // TRAIN
                         LCD_TRAINDIR_NOK_TXT,      // LCD_TRAINDIR_NOK
                         LCD_DEPATURE_TXT,          // LCD_DEPATURE
                         LCD_TAM_CANCEL_TXT,        // LCD_TAM_CANCEL
                         LCD_TAM_ACCEPT_TXT,        // LCD_TAM_ACCEPT
                         LCD_ARRIVAL_TXT,           // LCD_ARRIVAL
                         LCD_TAM_NOK_TXT,           // LCD_TAM_NOK
                         LCD_TAM_CANCELED_TXT,      // LCD_TAM_CANCELED
                         LCD_TAM_OK_TXT,            // LCD_TAM_OK
                         LCD_ARRIVAL_OK_TXT};       // LCD_ARRIVAL_OK
  int cRow;                                         // Clear row
  int cCol;                                         // Clear stringpos start
  int iRow;                                         // Insert row
  bool escaped        = false;

  if (string[str].indexOf("\xc3") >= 0) {
    string[str] = removeEscapeChar(string[str]);
    escaped = true;
  }

  switch (dest) {
    case DEST_A:
      cRow = LCD_FIRST_ROW;
      cCol = LCD_MAX_LENGTH / 2;
      iRow = LCD_SECOND_ROW;
    break;
//----------------------------------------------------------------------------------------------
    case DEST_B:
      cRow = LCD_FIRST_ROW;
      cCol = LCD_FIRST_COL;
      iRow = LCD_SECOND_ROW;
    break;
//----------------------------------------------------------------------------------------------
    case DEST_C:
      cRow = LCD_SECOND_ROW;
      cCol = LCD_MAX_LENGTH / 2;
      iRow = LCD_FIRST_ROW;
    break;
//----------------------------------------------------------------------------------------------
    case DEST_D:
      cRow = LCD_SECOND_ROW;
      cCol = LCD_FIRST_COL;
      iRow = LCD_FIRST_ROW;
    break;
  }
//----------------------------------------------------------------------------------------------
  switch (str) {
    case LCD_TRAIN_ID:
      if (string[LCD_TRAIN].indexOf("\xc3") >= 0) {
        string[LCD_TRAIN] = removeEscapeChar(string[LCD_TRAIN]);
      }

      if (string[LCD_TRAIN].length() < LCD_MAX_LENGTH / 2) {
        lcd.setCursor(LCD_MAX_LENGTH / 2 - centerText(string[LCD_TRAIN]) + string[LCD_TRAIN].length(), iRow);
      }

      else {
        lcd.setCursor(LCD_FIRST_COL + string[LCD_TRAIN].length(), iRow);
      }

      lcd.print(String(train));
    break;
//----------------------------------------------------------------------------------------------
    case LCD_TRAIN:
      lcd.setCursor(cCol, cRow);
      lcd.print(setBlanks(LCD_MAX_LENGTH / 2));                                                 // Clear the position
      lcd.setCursor(LCD_FIRST_COL, iRow);
      lcd.print(setBlanks(LCD_MAX_LENGTH));                                                     // Clear the row
      if (string[str].length() < LCD_MAX_LENGTH / 2) {
        lcd.setCursor(LCD_MAX_LENGTH / 2 - centerText(string[str]), iRow);
      }

      else {
        lcd.setCursor(LCD_FIRST_COL, iRow);
      }

      lcd.print(string[str]);
      if (escaped) {
        if (string[str].length() < LCD_MAX_LENGTH / 2) {
          fixSweChar(string[str], LCD_MAX_LENGTH / 2 - centerText(string[str]), iRow);
        }

        else {
          fixSweChar(string[str], LCD_FIRST_COL, iRow);
        }
      }

      lcd.cursor();
      if (string[str].length() < LCD_MAX_LENGTH / 2) {
        lcd.setCursor(LCD_MAX_LENGTH / 2 - centerText(string[str]) + string[str].length(), iRow);
      }

      else {
        lcd.setCursor(LCD_FIRST_COL + string[str].length(), iRow);
      }

      lcd.blink();
    break;
//----------------------------------------------------------------------------------------------
    default:
      lcd.setCursor(cCol, cRow);
      lcd.print(setBlanks(LCD_MAX_LENGTH / 2));                                                 // Clear the position
      lcd.setCursor(LCD_FIRST_COL, iRow);
      lcd.print(setBlanks(LCD_MAX_LENGTH));                                                     // Clear the row
      lcd.setCursor(LCD_MAX_LENGTH / 2 - centerText(string[str]), iRow);
      lcd.print(string[str]);
      if (escaped) {
        fixSweChar(string[str], LCD_MAX_LENGTH / 2 - centerText(string[str]), iRow);
      }
    break;
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Set destination text
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setNodeTxt(int dest, int trk) {

  String destTxt = tamBoxConfig[dest][SIGN];
#ifdef DEBUG
  String track = "-";                                                                           // show track sign during debug
  if (tamBoxConfig[dest][TRACKS].toInt() == DOUBLE_TRACK) { track = "="; }
#endif
  if (trainId[dest][trk] != DEST_ZERO_TRAIN) { destTxt = String(trainId[dest][trk]); }

  switch (dest) {
    case DEST_A:
    case DEST_C:
#ifdef DEBUG
      if (trainId[dest][trk] == DEST_ZERO_TRAIN) {
        destTxt = destTxt+track;                                                                // show track sign during debug
      }
#endif
      runningString[dest][LCD_NODE_TXT] = destTxt+setBlanks(LCD_NODE_LEN - destTxt.length());
    break;
//----------------------------------------------------------------------------------------------
    default:
#ifdef DEBUG
      if (trainId[dest][trk] == DEST_ZERO_TRAIN) {
        destTxt = track+destTxt;                                                                // show track sign during debug
      }
#endif
      runningString[dest][LCD_NODE_TXT] = setBlanks(LCD_NODE_LEN - destTxt.length())+destTxt;
    break;
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Set direction for a destination
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setDirectionTxt(int dest, int trk) {

  switch (dest) {
    case DEST_A:
    case DEST_C:
      switch (traffDir[dest][trk]) {
        case DIR_OUT:                                                                           // Outgoing direction
          runningString[dest][LCD_DIR_TXT] = DIR_LEFT_TXT;                                      // <
        break;

        case DIR_IN:                                                                            // Incomming direction
          runningString[dest][LCD_DIR_TXT] = DIR_RIGHT_TXT;                                     // >
        break;

        default:                                                                                // Connection lost
          runningString[dest][LCD_DIR_TXT] = DIR_LOST_TXT;                                      // ?
        break;
      }
    break;
//----------------------------------------------------------------------------------------------
    default:
      switch (traffDir[dest][trk]) {
        case DIR_OUT:                                                                           // Outgoing direction
          runningString[dest][LCD_DIR_TXT] = DIR_RIGHT_TXT;                                     // >
        break;

        case DIR_IN:                                                                            // Incomming direction
          runningString[dest][LCD_DIR_TXT] = DIR_LEFT_TXT;                                      // <
        break;

        default:                                                                                // Connection lost
          runningString[dest][LCD_DIR_TXT] = DIR_LOST_TXT;                                      // ?
        break;
      }
    break;
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Update the display
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void updateLcd(int dest) {

  String stationName = tamBoxConfig[OWN][NAME];
  bool escaped = false;

  switch (dest) {
    case DEST_A:
      lcd.setCursor(LCD_FIRST_COL, LCD_FIRST_ROW);
      lcd.print(runningString[DEST_A][LCD_DEST_TXT]+
                runningString[DEST_A][LCD_DIR_TXT]+
                runningString[DEST_A][LCD_NODE_TXT]);
    break;
//----------------------------------------------------------------------------------------------
    case DEST_B:
      lcd.setCursor(LCD_FIRST_COL + LCD_DEST_LEN + LCD_DIR_LEN + LCD_NODE_LEN, LCD_FIRST_ROW);
      lcd.print(runningString[DEST_B][LCD_NODE_TXT]+
                runningString[DEST_B][LCD_DIR_TXT]+
                runningString[DEST_B][LCD_DEST_TXT]);
    break;
//----------------------------------------------------------------------------------------------
    case DEST_C:
      lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
      lcd.print(runningString[DEST_C][LCD_DEST_TXT]+
                runningString[DEST_C][LCD_DIR_TXT]+
                runningString[DEST_C][LCD_NODE_TXT]);
    break;
//----------------------------------------------------------------------------------------------
    case DEST_D:
      lcd.setCursor(LCD_FIRST_COL + LCD_DEST_LEN + LCD_DIR_LEN + LCD_NODE_LEN, LCD_SECOND_ROW);
      lcd.print(runningString[DEST_D][LCD_NODE_TXT]+
                runningString[DEST_D][LCD_DIR_TXT]+
                runningString[DEST_D][LCD_DEST_TXT]);
    break;
//----------------------------------------------------------------------------------------------
    case DEST_OWN_STATION:
      // Tempfix, when received unicode is not printed correctly on the LCD
      if (tamBoxConfig[OWN][NAME].indexOf("\xc3") >= 0) {
        stationName = removeEscapeChar(tamBoxConfig[OWN][NAME]);
        escaped = true;
      }
      lcd.clear();
      lcd.setCursor(LCD_MAX_LENGTH / 2 - centerText(tamBoxConfig[OWN][SIGN]), LCD_FIRST_ROW);
      lcd.print(tamBoxConfig[OWN][SIGN]);
      lcd.setCursor(LCD_MAX_LENGTH / 2 - centerText(stationName), LCD_SECOND_ROW);
      lcd.print(stationName);
      if (escaped) {
        fixSweChar(stationName, LCD_MAX_LENGTH / 2 - centerText(stationName), LCD_SECOND_ROW);
      }
    break;
//----------------------------------------------------------------------------------------------
    case DEST_ALL_DEST:
      // show running screen
      lcd.clear();
      lcd.home();
      lcd.print(runningString[DEST_A][LCD_DEST_TXT]+runningString[DEST_A][LCD_DIR_TXT]+runningString[DEST_A][LCD_NODE_TXT]);
      lcd.print(runningString[DEST_B][LCD_NODE_TXT]+runningString[DEST_B][LCD_DIR_TXT]+runningString[DEST_B][LCD_DEST_TXT]);
      lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
      lcd.print(runningString[DEST_C][LCD_DEST_TXT]+runningString[DEST_C][LCD_DIR_TXT]+runningString[DEST_C][LCD_NODE_TXT]);
      lcd.print(runningString[DEST_D][LCD_NODE_TXT]+runningString[DEST_D][LCD_DIR_TXT]+runningString[DEST_D][LCD_DEST_TXT]);
    break;
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Set defaults
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setDefaults() {

  String destTxt;
#ifdef DEBUG
  String db1Text = "setDefaults     : ";
  String track;                                                                                 // show track sign during debug
  Serial.println(db1Text+__LINE__+" Setting default values");
#endif
  for (int i=0; i<NUM_OF_DEST; i++) {
#ifdef DEBUG
    if (tamBoxConfig[i][TRACKS].toInt() == SINGLE_TRACK) {
      track = "-";
    }

    else {
      track = "=";
    }
#endif
    destTxt = tamBoxConfig[i][SIGN];
    if (tamBoxConfig[i][ID] == NOT_USED_TXT) {                                                  // Not used Destination
      tamBoxState[i][DEST_LEFT]       = STATE_NOTUSED;
      tamBoxState[i][DEST_RIGHT]      = STATE_NOTUSED;
      setLed(i, STATE_NOTUSED);
      runningString[i][LCD_DEST_TXT]  = setBlanks(LCD_DEST_LEN);
      runningString[i][LCD_DIR_TXT]   = setBlanks(LCD_DIR_LEN);
      runningString[i][LCD_NODE_TXT]  = setBlanks(LCD_NODE_LEN);
    }

    else {
      if (tamBoxConfig[i][TYPE] == TYPE_SINGLE_TXT) {                                           // Single track Destination
        tamBoxState[i][DEST_LEFT]     = STATE_IDLE;                                             // Single track
        tamBoxState[i][DEST_RIGHT]    = STATE_NOTUSED;
      }

      else if (tamBoxConfig[i][TYPE] == TYPE_SPLIT_TXT) {                                       // Single track to two Destination (Not supported yet)
        tamBoxState[i][DEST_LEFT]     = STATE_IDLE;                                             // Single track Destination 1
        tamBoxState[i][DEST_RIGHT]    = STATE_IDLE;                                             // Single track Destination 2
      }

      else {                                                                                    // Double track Destination
        tamBoxState[i][DEST_LEFT]     = STATE_IDLE;
        
        setDirectionTxt(i, DEST_LEFT);
        tamBoxState[i][DEST_RIGHT]    = STATE_IDLE;
      }
      runningString[i][LCD_DEST_TXT]  = nodeIDTxt[i];

      switch (i) {
        case DEST_A:
        case DEST_C:
#ifdef DEBUG
          destTxt = destTxt+track;                                                              // show track sign during debug
#endif
//          runningString[i][LCD_DIR_TXT]   = DIR_IDLE_TXT;
          runningString[i][LCD_DIR_TXT]   = DIR_LEFT_TXT;
          runningString[i][LCD_NODE_TXT]  = destTxt+setBlanks(LCD_NODE_LEN - destTxt.length());
        break;
//----------------------------------------------------------------------------------------------
        default:
#ifdef DEBUG
          destTxt = track+destTxt;                                                              // show track sign during debug
#endif
//          runningString[i][LCD_DIR_TXT]   = DIR_IDLE_TXT;
          runningString[i][LCD_DIR_TXT]   = DIR_RIGHT_TXT;
          runningString[i][LCD_NODE_TXT]  = setBlanks(LCD_NODE_LEN - destTxt.length())+destTxt;
        break;
      }
      setLed(i, STATE_IDLE);
    }
//----------------------------------------------------------------------------------------------
#ifdef DEBUG
    switch (i) {
      case DEST_A:
      case DEST_C:
        Serial.println(db1Text+__LINE__+" Destination "+nodeIDTxt[i]+" = '"+
                                        runningString[i][LCD_DEST_TXT]+
                                        runningString[i][LCD_DIR_TXT]+
                                        runningString[i][LCD_NODE_TXT]+"'");
      break;
//----------------------------------------------------------------------------------------------
      default:
        Serial.println(db1Text+__LINE__+" Destination "+nodeIDTxt[i]+" = '"+
                                        runningString[i][LCD_NODE_TXT]+
                                        runningString[i][LCD_DIR_TXT]+
                                        runningString[i][LCD_DEST_TXT]+"'");
      break;
    }
//----------------------------------------------------------------------------------------------
    Serial.println(db1Text+__LINE__+" TamBoxState destination "+nodeIDTxt[i]+
                                    " track left set to "+tamBoxStateTxt[tamBoxState[i][0]]);
    Serial.println(db1Text+__LINE__+" TamBoxState destination "+nodeIDTxt[i]+
                                    " track right set to "+tamBoxStateTxt[tamBoxState[i][0]]);
#endif
  }
  destBtnPushed = 0;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  (Re)connects to MQTT broker and subscribes to one or more topics
 * ------------------------------------------------------------------------------------------------------------------------------
 */
bool mqttConnect() {

#ifdef DEBUG
  String db1Text = "mqttConnect     : ";
#endif
  char tmpTopic[254];
  char tmpContent[254];
  char tmpID[clientID.length()];                    // For converting clientID
  char* tmpMessage = "lost";                        // Status message in Last Will
  
  // Set up broker and assemble topics to subscribe to and publish
  setupBroker();
  setupTopics();

  // Convert String to char* for last will message
  clientID.toCharArray(tmpID, clientID.length()+1);
  pubDeviceStateTopic.toCharArray(tmpTopic, pubDeviceStateTopic.length()+1);
  
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    lcd.home();
    lcd.print(topPhase[LCD_STARTING_UP] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_STARTING_UP].length()));
    lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
    lcd.print(subPhase[LCD_STARTING_MQTT] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_STARTING_MQTT].length()));
    delay(1000);
#ifdef DEBUG
    Serial.print(db1Text+__LINE__+" MQTT connection...");
#endif

    // Attempt to connect
    // boolean connect (tmpID, pubDeviceStateTopic, willQoS, willRetain, willMessage)
    if (mqttClient.connect(tmpID, tmpTopic, 0, true, tmpMessage)) {

      lcd.home();
      lcd.print(topPhase[LCD_STARTING_UP] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_STARTING_UP].length()));
      lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
      lcd.print(subPhase[LCD_BROKER_CONNECTED] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_BROKER_CONNECTED].length()));
      delay(1000);
#ifdef DEBUG
      Serial.println("connected");
      Serial.print(db1Text+__LINE__+" MQTT client id = ");
      Serial.println(cfgClientID);
#endif

      // Subscribe to all topics
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Subscribing to:");
#endif

      for (int i=0; i<nbrSubTopics; i++){
        // Convert String to char* for the mqttClient.subribe() function to work
        if (subscribeTopic[i].length() > 0) {
          subscribeTopic[i].toCharArray(tmpTopic, subscribeTopic[i].length()+1);

          // ... print topic
#ifdef DEBUG
          Serial.println(db1Text+__LINE__+" - "+tmpTopic);
#endif

          //   ... and subscribe to topic
          mqttClient.subscribe(tmpTopic);
        }
      }

      // Publish to all topics
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Publishing:");
#endif

      for (int i=0; i<nbrPubTopics; i++){
        // Convert String to char* for the mqttClient.publish() function to work
        pubTopic[i].toCharArray(tmpTopic, pubTopic[i].length()+1);
        pubTopicContent[i].toCharArray(tmpContent, pubTopicContent[i].length()+1);

        // ... print topic
#ifdef DEBUG
        Serial.print(db1Text+__LINE__+" - "+tmpTopic);
        Serial.print(" = ");
        Serial.println(tmpContent);
#endif

        // ... and subscribe to topic
        mqttClient.publish(tmpTopic, tmpContent, true);
      }
    }

    else {
      // Show why the connection failed
      lcd.home();
      lcd.print(topPhase[LCD_START_ERROR] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_START_ERROR].length()));
      lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
      lcd.print(startError[LCD_BROKER_NOT_FOUND] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_BROKER_NOT_FOUND].length()));

#ifdef DEBUG
      Serial.print(db1Text+__LINE__+"  failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(", try again in 5 seconds");
#endif
      delay(5000);                                                                              // Wait 5 seconds before retrying
    }
  }

  // Set device status to "ready"
  updateLcd(DEST_OWN_STATION);
  delay(4000);                                                                                  // Show station name for 4 sec
  updateLcd(DEST_ALL_DEST);
  mqttPublish(pubDeviceStateTopic, "ready", RETAIN);                                            // Node ready

  tamBoxIdle = true;
#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" TamBox Redy!");
#endif

  return true;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to handle MQTT messages sent to this node
 *    MQTT structure:
 *    TOPIC_ROOT /TOPIC_CLIENT /TOPIC_DEST /TOPIC_TYPE /TOPIC_TRACK /TOPIC_ITEM /TOPIC_ORDER   payload
 *
 *  ex mqtt_h0/tambox-4/cda/traffic/left/direction/state   out
 *     mqtt_h0/tambox-2/blo/traffic/left/train/request     233
 *     mqtt_h0/tambox-3/sns/traffic/right/train/accept     123
 *     mqtt_h0/tambox-2/cda/traffic/left/train/reject      233
 *     mqtt_h0/tambox-2/$state                             lost
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {

#ifdef DEBUG
  String db1Text = "mqttCallback    : ";
#endif
  // Don't know why this have to be done :-(
  payload[length] = '\0';

  // Make strings
  String msg = String((char*)payload);
  String tpc = String((char*)topic);

  int MyP = 0;
  int MyI = 0;
  int trk = 0;                                                                                  // "left"
  String subTopic[NUM_OF_TOPICS];                                                               // Topic array

  // Split topic string into an array of the seven parts
  for (int i=0; i<NUM_OF_TOPICS; i++) {
    MyI = tpc.indexOf(TOPIC_DIVIDER_TXT, MyP);
    String s = tpc.substring(MyP, MyI);
    MyP = MyI + 1;
    if (i == TOPIC_DEST && s.substring(0, 1) != "$") { s.toUpperCase(); }
    subTopic[i] = s;
  }
#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" Recieved: "+tpc+" = "+msg);
#endif

  if (subTopic[TOPIC_ROOT] == tamBoxMqtt[TOPIC]) {                                              // Our Root Topic
    for (int i=0; i<NUM_OF_DEST; i++) {
      if (subTopic[TOPIC_CLIENT] == tamBoxConfig[i][ID]) {                                      // Subscribed client
        if (subTopic[TOPIC_DEST].substring(0, 1) == "$") {                                      // Info messages
          handleInfo(subTopic[TOPIC_CLIENT], msg);
          break;
        }

        else if (subTopic[TOPIC_DEST] == tamBoxConfig[OWN][SIGN]) {                             // To us
          if (subTopic[TOPIC_TYPE] == TOPIC_TRAFFIC_TXT) {                                      // Type is traffic
            if (subTopic[TOPIC_ITEM] == TOPIC_DIR_TXT) {                                        // Direction
              handleDirection(subTopic[TOPIC_CLIENT],
                              subTopic[TOPIC_TRACK], msg);
              break;
            }

            else if (subTopic[TOPIC_ITEM] == TOPIC_TRAIN_TXT) {                                 // Train order
              handleTrain(subTopic[TOPIC_CLIENT],
                          subTopic[TOPIC_TRACK],
                          subTopic[TOPIC_ORDER], msg);
              break;
            }

#ifdef DEBUG
            else {                                                                              // Not supported item
              Serial.println(db1Text+__LINE__+" Not supported item: "+subTopic[TOPIC_ITEM]);
            }
#endif
          }

#ifdef DEBUG
          else {                                                                                // Not supported type
            Serial.println(db1Text+__LINE__+" Not supported type: "+subTopic[TOPIC_TYPE]);
          }
#endif
        }

#ifdef DEBUG
        else {                                                                                  // Not to us
          Serial.println(db1Text+__LINE__+" Not supported destination: "+subTopic[TOPIC_DEST]);
        }
#endif
      }
#ifdef DEBUG
      else {                                                                                    // Not subscribed client
        Serial.println(db1Text+__LINE__+" Not subscribed client: "+subTopic[TOPIC_CLIENT]);
      }
#endif
    }
  }

#ifdef DEBUG
  else {
    Serial.println(db1Text+__LINE__+" Not supported root: "+subTopic[TOPIC_ROOT]);
  }
#endif
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Publish message to MQTT broker
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void mqttPublish(String pbTopic, String pbPayload, byte retain) {

#ifdef DEBUG
  String db1Text = "mqttPublish     : ";
#endif
  // Convert String to char* for the mqttClient.publish() function to work
  char msg[pbPayload.length()+1];
  pbPayload.toCharArray(msg, pbPayload.length()+1);
  char tpc[pbTopic.length()+1];
  pbTopic.toCharArray(tpc, pbTopic.length()+1);

  // Report back to pubTopic[]
  int check = mqttClient.publish(tpc, msg, retain);

  // TODO check "check" integer to see if all went ok

  // Print information
#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" Publish: "+pbTopic+" = "+pbPayload);
#endif
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to download config file
 * ------------------------------------------------------------------------------------------------------------------------------
 */
bool getConfigFile() {

#ifdef DEBUG
  String db1Text = "getConfigFile   : ";
#endif
// http://configserver.000webhostapp.com/?id=tambox-1

//  http.beginRequest();
  http.get(configPath);
//  http.sendHeader("Host",cHost);
//  http.endRequest();
  int statusCode = http.responseStatusCode();
  String response = http.responseBody();
//  response.replace("\u", "\\u");
  /*
    Typical response is:
{
  "id":"tambox-1",
  "config":{
    "signature":"CDA",
    "name":"Charlottendal",
    "destinations":4,
    "destination":{
      "A":{
        "tracks":2,
        "type":"split",
        "left":{
          "id":"tambox-4",
          "tracks":1,
          "exit":"A",
          "signature":"SAL",
          "name":"Salsborg"
        }
        "right":{
          "id":"tambox-5",
          "tracks":1,
          "exit":"A",
          "signature":"SNS",
          "name":"Sylten\u00e4s"
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
          "name":"Vangsta"
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
          ,"name":"G\u00e4ssl\u00f6sa"
        }
      },
      "D":{
        "tracks":1,
        "type":"single",
        "single":{
          "id":"-",
          "tracks":1,
          "exit":"-",
          "signature":"-",
          "name":"-"
        }
      }
    }
  },"mqtt":{
    "server":"192.168.1.7",
    "port":1883,
    "user":"",
    "password":"",
    "rootTopic":"mqtt_h0"
  }
}
 */
  // Allocate the JSON document
  // Use arduinojson.org/v6/assistant to compute the capacity.
//  const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
  DynamicJsonDocument doc(2048);

#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" Status Code: "+statusCode);
  Serial.println(db1Text+__LINE__+" Response: "+response);
#endif
  // Parse JSON object
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
#ifdef DEBUG
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
#endif
    return false;
  }

  else {
    if (String(doc[ID_TXT].as<const char*>()) == clientID) {
      tamBoxMqtt[SERVER]                = String(doc[MQTT_TXT][SERVER_TXT].as<const char*>());
      tamBoxMqtt[PORT]                  = String(doc[MQTT_TXT][PORT_TXT].as<int>());
      tamBoxMqtt[USER]                  = String(doc[MQTT_TXT][USER_TXT].as<const char*>());
      tamBoxMqtt[PASS]                  = String(doc[MQTT_TXT][PASS_TXT].as<const char*>());
      tamBoxMqtt[TOPIC]                 = String(doc[MQTT_TXT][ROOT_TXT].as<const char*>());
      tamBoxConfig[OWN][ID]             = String(doc[ID_TXT].as<const char*>());                // Own station
      tamBoxConfig[OWN][SIGN]           = String(doc[CONFIG_TXT][SIGN_TXT].as<const char*>());
      tamBoxConfig[OWN][NAME]           = String(doc[CONFIG_TXT][NAME_TXT].as<const char*>());
      tamBoxConfig[OWN][NUMOFDEST]      = String(doc[CONFIG_TXT][DESTS_TXT].as<int>());
      for (int i=0; i<tamBoxConfig[OWN][NUMOFDEST].toInt(); i++) {                              // Destinations
        if (i > NUM_OF_DEST) { break; }
        if (doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][TRACK_TXT].as<int>() > 0) {
          tamBoxConfig[i][TOTTRACKS]    = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][TRACK_TXT].as<int>());
          tamBoxConfig[i][TYPE]         = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][TYPE_TXT].as<const char*>());
          String destType = tamBoxConfig[i][TYPE];
          if (destType == TYPE_SPLIT_TXT) {                                                     // Type split
            destType = TYPE_LEFT_TXT;                                                           // Left track
            tamBoxConfig[i][ID]         = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][ID_TXT].as<const char*>());
            tamBoxConfig[i][SIGN]       = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][SIGN_TXT].as<const char*>());
            tamBoxConfig[i][NAME]       = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][NAME_TXT].as<const char*>());
            tamBoxConfig[i][TRACKS]     = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][TRACK_TXT].as<int>());
            tamBoxConfig[i][EXIT]       = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][EXIT_TXT].as<const char*>());
            destType = TYPE_RIGHT_TXT;                                                          // Right track
            tamBoxConfig[i+5][ID]       = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][ID_TXT].as<const char*>());
            tamBoxConfig[i+5][SIGN]     = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][SIGN_TXT].as<const char*>());
            tamBoxConfig[i+5][NAME]     = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][NAME_TXT].as<const char*>());
            tamBoxConfig[i+5][TRACKS]   = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][TRACK_TXT].as<int>());
            tamBoxConfig[i+5][EXIT]     = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][EXIT_TXT].as<const char*>());
          }

          else {                                                                                // Type single or double
            tamBoxConfig[i][ID]         = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][ID_TXT].as<const char*>());
            tamBoxConfig[i][SIGN]       = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][SIGN_TXT].as<const char*>());
            tamBoxConfig[i][NAME]       = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][NAME_TXT].as<const char*>());
            tamBoxConfig[i][TRACKS]     = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][TRACK_TXT].as<int>());
            tamBoxConfig[i][EXIT]       = String(doc[CONFIG_TXT][DEST_TXT][nodeIDTxt[i]][destType][EXIT_TXT].as<const char*>());
          }
        }
      }
      return true;
    }
  }
  return false;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to set-up all topics
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setupTopics() {

#ifdef DEBUG
  String db1Text = "setupTopics     : ";
  Serial.println(db1Text+__LINE__+" Topics setup");
#endif

  // Subscribe
  // Topic                körning/client/#
  for (int i=0; i<NUM_OF_DEST; i++) {
    if (tamBoxState[i][DEST_LEFT] != STATE_NOTUSED) {
      subscribeTopic[i] = tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+
                          tamBoxConfig[i][ID]+TOPIC_DIVIDER_TXT+
                          TOPIC_HASH_TXT;
    }
  }

  // Publish - Client
  // Topic                körning/client/$
  pubTopic[0]           = tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+
                          tamBoxConfig[OWN][ID]+TOPIC_DIVIDER_TXT+
                          TOPIC_DID_TXT;
  pubTopicContent[0]    = tamBoxConfig[OWN][ID];                                                // client ID
  pubTopic[1]           = tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+
                          tamBoxConfig[OWN][ID]+TOPIC_DIVIDER_TXT+
                          TOPIC_DNAME_TXT;
  pubTopicContent[1]    = tamBoxConfig[OWN][NAME];                                              // client name
  pubTopic[2]           = tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+
                          tamBoxConfig[OWN][ID]+TOPIC_DIVIDER_TXT+
                          TOPIC_DTYPE_TXT;
  pubTopicContent[2]    = APTYPE;                                                               // Client type
  pubTopic[3]           = tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+
                          tamBoxConfig[OWN][ID]+TOPIC_DIVIDER_TXT+
                          TOPIC_DSW_TXT;
  pubTopicContent[3]    = SW_VERSION;                                                           // Client SW version
  pubDeviceStateTopic   = tamBoxMqtt[TOPIC]+TOPIC_DIVIDER_TXT+
                          tamBoxConfig[OWN][ID]+TOPIC_DIVIDER_TXT+
                          TOPIC_DSTATE_TXT;

#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" Topics setup done!");
#endif
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Prepare MQTT broker and define function to handle callbacks
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setupBroker() {

#ifdef DEBUG
  String db1Text = "setupBroker     : ";
#endif
  tmpMqttServer = tamBoxMqtt[SERVER].c_str();

#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" MQTT setup");
#endif

  mqttClient.setServer(tmpMqttServer, tamBoxMqtt[PORT].toInt());
  mqttClient.setCallback(mqttCallback);

#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" MQTT setup done!");
#endif
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to show AP web start page
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void handleRoot() {

  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal()) {

    // -- Captive portal request were already served.
    return;
  }

  // Assemble web page content
  String page  = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" ";
         page += "content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";

         page += "<title>MQTT-inst&auml;llningar</title></head><body>";
         page += "<h1>Inst&auml;llningar</h1>";

         page += "<p>V&auml;lkommen till MQTT-enheten med namn: '";
         page += cfgClientID;
         page += "'</p>";

         page += "<p>P&aring; sidan <a href='config'>Inst&auml;llningar</a> ";
         page += "kan du best&auml;mma hur just denna MQTT-klient ska fungera.";
         page += "</p>";

         page += "<p>M&ouml;jligheten att &auml;ndra dessa inst&auml;llningar &auml;r ";
         page += "alltid tillg&auml;nglig de f&ouml;rsta 30 sekunderna efter start av enheten.";
         page += "</p>";

         page += "</body></html>\n";

  // Show web start page
  server.send(200, "text/html", page);
}

/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function beeing called when wifi connection is up and running
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void wifiConnected() {

#ifdef DEBUG
  String db1Text = "wifiConnected   : ";
#endif
  long rssi = WiFi.RSSI();
  lcd.home();
  lcd.print(topPhase[LCD_STARTING_UP] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_STARTING_UP].length()));
  lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
  lcd.print(subPhase[LCD_WIFI_CONNECTED] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_WIFI_CONNECTED].length()));
  delay(1000);

#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" Signal strength (RSSI): "+rssi+" dBm");
#endif
  lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
  lcd.print(setBlanks(LCD_MAX_LENGTH));
  lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
  lcd.print(subPhase[LCD_SIGNAL] + rssi + "dBm");
  delay(1000);

  String pathID = cPath+clientID;
  configPath = pathID.c_str();
#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" configPath ="+configPath);
#endif
  lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
  lcd.print(subPhase[LCD_LOADING_CONF] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_LOADING_CONF].length()));
  delay(1000);

  if (getConfigFile()) {
#ifdef DEBUG
    Serial.println(db1Text+__LINE__+" config received");
#endif
    lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
    lcd.print(subPhase[LCD_LOADING_CONF_OK] + setBlanks(LCD_MAX_LENGTH - topPhase[LCD_LOADING_CONF_OK].length()));
    notReceivedConfig = false;
    setDefaults();
    delay(1000);
  }
  else {
    lcd.setCursor(LCD_FIRST_COL, LCD_SECOND_ROW);
    lcd.print(startError[LCD_LOADING_CONF_NOK] + setBlanks(LCD_MAX_LENGTH - startError[LCD_LOADING_CONF_NOK].length()));
    delay(1000);
  }
    
  // We are ready to start the MQTT connection
  needMqttConnect = true;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function that gets called when web page config has been saved
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void configSaved() {

#ifdef DEBUG
  String db1Text = "configSaved     : ";
  Serial.println(db1Text+__LINE__+" IotWebConf config saved");
#endif
  clientID                            = String(cfgClientID);
  clientID.toLowerCase();
  clientName                          = String(cfgClientName);
  if (notReceivedConfig) {
    tamBoxMqtt[SERVER]                = String(cfgMqttServer);
    tamBoxMqtt[PORT]                  = String(cfgMqttPort);
    tamBoxMqtt[USER]                  = String(cfgMqttUserName);
    tamBoxMqtt[PASS]                  = String(cfgMqttUserPass);
    tamBoxMqtt[TOPIC]                 = String(cfgMqttTopic);
//    cHost                             = cfgConfServer;
//    cPort                             = cfgConfPort;
//    cFile                             = cfgConfFile;
    tamBoxConfig[OWN][SIGN]           = String(cfgClientSign);
    tamBoxConfig[DEST_A][ID]          = String(cfgDestA);
    tamBoxConfig[DEST_A][SIGN]        = String(cfgDestSignA);
    tamBoxConfig[DEST_A][NAME]        = NOT_USED_TXT;
    tamBoxConfig[DEST_A][EXIT]        = String(cfgDestExitA);
    tamBoxConfig[DEST_A][TOTTRACKS]   = String(cfgTrackNumA);
    tamBoxConfig[DEST_B][ID]          = String(cfgDestB);
    tamBoxConfig[DEST_B][SIGN]        = String(cfgDestSignB);
    tamBoxConfig[DEST_B][NAME]        = NOT_USED_TXT;
    tamBoxConfig[DEST_B][EXIT]        = String(cfgDestExitB);
    tamBoxConfig[DEST_B][TOTTRACKS]   = String(cfgTrackNumB);
    tamBoxConfig[DEST_C][ID]          = String(cfgDestC);
    tamBoxConfig[DEST_C][SIGN]        = String(cfgDestSignC);
    tamBoxConfig[DEST_C][NAME]        = NOT_USED_TXT;
    tamBoxConfig[DEST_C][EXIT]        = String(cfgDestExitC);
    tamBoxConfig[DEST_C][TOTTRACKS]   = String(cfgTrackNumC);
    tamBoxConfig[DEST_D][ID]          = String(cfgDestD);
    tamBoxConfig[DEST_D][SIGN]        = String(cfgDestSignD);
    tamBoxConfig[DEST_D][NAME]        = NOT_USED_TXT;
    tamBoxConfig[DEST_D][EXIT]        = String(cfgDestExitD);
    tamBoxConfig[DEST_D][TOTTRACKS]   = String(cfgTrackNumD);
  }
  ledBrightness                       = atoi(cfgLedBrightness);
  lcdBackLight                        = atoi(cfgBackLight);

  FastLED.setBrightness(ledBrightness);
  lcd.setBacklight(lcdBackLight);

}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to fix swedish characters
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void fixSweChar(String text, int col, int row) {

#ifdef DEBUG
  String db1Text = "fixSweChar      : ";
#endif

#ifdef DEBUG
  Serial.println(db1Text+__LINE__+" Number of characters in String are "+text.length());
  Serial.println(db1Text+__LINE__+" String: "+text);
  Serial.print(db1Text+__LINE__+" String: ");
  for (int i=0; i<text.length(); i++) {
    Serial.printf("%02x", text.charAt(i));
    Serial.print(" ");
  }
  Serial.println("");
#endif
  for (int i=0; i<text.length(); i++) {
    if (text.charAt(i) == '\x85' || text.charAt(i) == 'Å') {
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Å found at pos "+(i + 1));
#endif
      printSpecialChar(0, col + i, row);
    }

    if (text.charAt(i) == '\x84' || text.charAt(i) == 'Ä') {
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Ä found at pos "+(i + 1));
#endif
      printSpecialChar(1, col + i, row);
    }

    if (text.charAt(i) == '\x96' || text.charAt(i) == 'Ö') {
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Ö found at pos "+(i + 1));
#endif
      printSpecialChar(2, col + i, row);
    }

    if (text.charAt(i) == '\xa5' || text.charAt(i) == 'å') {
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" å found at pos "+(i + 1));
#endif
      printSpecialChar(3, col + i, row);
    }

    if (text.charAt(i) == '\xa4' || text.charAt(i) == 'ä') {
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" ä found at pos "+(i + 1));
#endif
      printSpecialChar(4, col + i, row);
    }

    if (text.charAt(i) == '\xb6' || text.charAt(i) == 'ö') {
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" ö found at pos "+(i + 1));
#endif
      printSpecialChar(5, col + i, row);
    }

    if (text.charAt(i) == '\x89' || text.charAt(i) == 'É') {
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" É found at pos "+(i + 1));
#endif
      printSpecialChar(6, col + i, row);
    }

    if (text.charAt(i) == '\xa9' || text.charAt(i) == 'é') {
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" é found at pos "+(i + 1));
#endif
      printSpecialChar(7, col + i, row);
    }
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to remove escape character for the LCD
 * ------------------------------------------------------------------------------------------------------------------------------
 */
String removeEscapeChar(String text) {

#ifdef DEBUG
  String db1Text = "removeEscapeChar: ";
  Serial.println(db1Text+__LINE__+" Character c3 removed");
#endif

  text.replace("\xc3", "");
  return text;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to set color on a LED
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void testLed() {

#ifdef DEBUG
  String db1Text = "testLed         : ";
#endif
  for (int i=0; i<NUM_LED_DRIVERS; i++) {
    rgbLed[i] = CRGB::Red;
    FastLED.show();
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination led "+nodeIDTxt[i]+" set to Red");
#endif
    delay(500);
    rgbLed[i] = CRGB::Green;
    FastLED.show();
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination led "+nodeIDTxt[i]+" set to Green");
#endif
    delay(500);
    rgbLed[i] = CRGB::Blue;
    FastLED.show();
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination led "+nodeIDTxt[i]+" set to Blue");
#endif
    delay(500);
    rgbLed[i] = CRGB::Black;
    FastLED.show();
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to set color on a LED
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void setLed(int dest, int stat) {

#ifdef DEBUG
  String db1Text = "setLed          : ";
#endif
  switch (stat) {
    case STATE_INREQUEST:
    case STATE_OUREQUEST:
      rgbLed[dest] = CRGB::Yellow;
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination led "+nodeIDTxt[dest]+" set to Yellow");
#endif
    break;
//-------------------------------------------------------------------------------------------------------------------------------
    case STATE_INTRAIN:
    case STATE_OUTTRAIN:
    case STATE_LOST:
      rgbLed[dest] = CRGB::Red;
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination led "+nodeIDTxt[dest]+" set to Red");
#endif
    break;
//-------------------------------------------------------------------------------------------------------------------------------
    case STATE_IDLE:
      rgbLed[dest] = CRGB::Black;
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination led "+nodeIDTxt[dest]+" set to Dark");
#endif
    break;
//-------------------------------------------------------------------------------------------------------------------------------
    default:
      rgbLed[dest] = CRGB::Black;
#ifdef DEBUG
      Serial.println(db1Text+__LINE__+" Destination led "+nodeIDTxt[dest]+" set to Dark");
#endif
    break;
  }
//-------------------------------------------------------------------------------------------------------------------------------
  FastLED.show();
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Toogle between right and left track when double track so both directions can be viewed
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void toogleTrack(){

  if (currentTrack == DEST_RIGHT) {
    currentTrack = DEST_LEFT;
  }

  else {
    currentTrack = DEST_RIGHT;
  }

  if (tamBoxIdle) {
    for (int i=0; i<=NUM_OF_DEST; i++) {
      if (tamBoxConfig[i][ID] != NOT_USED_TXT) {
        if (tamBoxConfig[i][TRACKS].toInt() == 2) {
          setDirectionTxt(i, currentTrack);
          setNodeTxt(i, currentTrack);
          updateLcd(i);
        }
      }
    }
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to set a string into center on display
 * ------------------------------------------------------------------------------------------------------------------------------
 */
int centerText(String txt) {

  if (txt.length() % 2) {
    return (txt.length() + 1) / 2;
  }

  else {
    return txt.length() / 2;
  }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Insert x number of blanks
 * ------------------------------------------------------------------------------------------------------------------------------
 */
String setBlanks(int blanks) {

  String s = "";
  for (int i=1; i<=blanks; i++) { s += " "; }
  return s;
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to beep the buzzer
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void beep(unsigned char delayms, int freq) {

  tone(buzzerPin, freq, delayms);
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to print special characters on LCD
 * ------------------------------------------------------------------------------------------------------------------------------
 */
void printSpecialChar(int character, int col, int row) {

   if (character < 8) {
      lcd.setCursor(col, row);
      lcd.write(character);
   }
}


/* ------------------------------------------------------------------------------------------------------------------------------
 *  Function to validate the input data form
 * ------------------------------------------------------------------------------------------------------------------------------
 */
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper) {

#ifdef DEBUG
  Serial.println(dbText+ "__LINE__+IotWebConf validating form");
#endif
  bool valid = true;
/*  String msg = "Endast dessa är tillåtna signaltyper: ";

  int l = webRequestWrapper->arg(mqttServerParam.getId()).length();
  if (l < 3)
  {
    mqttServerParam.errorMessage = "Please provide at least 3 characters!";
    valid = false;
  }

  for (int i=0; i<nbrSigTypes - 2; i++) {
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
