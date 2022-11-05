// ------------------------------------------------------------------------------------------------------------------------------
//
//    mqttTamBox v1.0.0
//    copyright (c) Benny Tjäder
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published
//    by the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    Uses and have been tested with the following libraries
//      ArduinoHttpClient v0.4.0  by                     - https://github.com/arduino-libraries/ArduinoHttpClient
//      ArduinoJson       v6.19.4 by Benoit Blanchon     - https://github.com/bblanchon/ArduinoJson
//      IotWebConf        v3.2.1  by Balazs Kelemen      - https://github.com/prampec/IotWebConf
//      Keypad            v3.1.1  by Chris--A            - https://github.com/Chris--A/Keypad
//      Keypad_I2C        v2.3.1  by Joe Young           - https://github.com/joeyoung/BensArduino-git
//      LiquidCrystal_I2C v2.3.1  by Marco Schwartz      - https://github.com/johnrickman/LiquidCrystal_I2C
//      PubSubClient      v2.8.0  by Nick O'Leary        - https://pubsubclient.knolleary.net/
//      FastLed           v3.5.0  by Daniel Garcia       - https://github.com/FastLED/FastLED
//      Wire              v2.3.1  by Nicholas Zambetti   - https://github.com/prampec/IotWebConf
//
//    This program is realizing a TAM Box.
//    TAM Box is used to:
//     - on sending side:   allocated a track between two stations and request to send a train.
//     - on receiving side: accept/reject incomming request.
//
//    Each track direction is using key A-D. A and C is direction left, and B and D is direction right.
//    Each A-D button has a RGB led which can show:
//      - green:        direction is outgoing
//      - red:          direction is incoming
//      - flash green:  outgoing request sent
//      - flash red:    incoming request received
//      - yellow:       outgoing/incoming request accepted
//
//    MQTT structure:
//    top   /client  /exit/type   /track/item     /order   payload
//    mqtt_n/tambox-4/a   /traffic/up   /direction/state   up
//
//    mqtt_n/tambox-4/b/traffic/up/direction/state  up      Traffic direction on up track is up (right)
//    mqtt_n/tambox-2/a/traffic/up/train/request    2123    Request outgoing train 2123 on up track
//    mqtt_n/tambox-2/a/traffic/up/train/accept     2123    Train 2123 accepted on up track
//    mqtt_n/tambox-4/c/traffic/down/train/reject   342     Train 348 rejected on down track
//
// ------------------------------------------------------------------------------------------------------------------------------
#include <ArduinoHttpClient.h>                    // Library to handle HTTP download of config file
#include <ArduinoJson.h>                          // Library to handle JSON for the config file
#include <PubSubClient.h>                         // Library to handle MQTT communication
#include <IotWebConf.h>                           // Library to take care of wifi connection & client settings
#include <IotWebConfUsing.h>                      // This loads aliases for easier class names.
#include <Wire.h>                                 // Library to handle i2c communication
#include <Keypad_I2C.h>                           // Library to handle i2c keypad
#include <LiquidCrystal_PCF8574.h>                // Library to handle the LCD
#define FASTLED_ESP8266_D1_PIN_ORDER              // Needed for nodeMCU pin numbering
#include <FastLED.h>                              // Library for LEDs
#include "mqttTamBox.h"                           // Some of the client settings

CRGB leds[NUM_LED_DRIVERS];                       // Define the array of leds

// -- Method declarations.
bool mqttConnect();
void mqttPublish(String pbTopic, String pbPayload, byte retain);
bool getConfigFile();
void setupTopics();
void setupBroker();
void handleRoot();
void wifiConnected();
void setLcdString();
void setRunningString();
void handleDirection();
void handleTrain();
String setLcdBlanks();
void keyReceived(char key);

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
const byte ROWS                     = 4;            //four rows
const byte COLS                     = 4;            //four columns
char keys[ROWS][COLS]               = {{'1', '2', '3', 'A'},
                                       {'4', '5', '6', 'B'},
                                       {'7', '8', '9', 'C'},
                                       {'*', '0', '#', 'D'}};

// Digitran keypad, bit numbers of PCF8574 I/O port
byte rowPins[ROWS]                  = {4, 5, 6, 7}; //connect to the row pinouts of the keypad
byte colPins[COLS]                  = {0, 1, 2, 3}; //connect to the column pinouts of the keypad

Keypad_I2C Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYI2CADDR);

// ------------------------------------------------------------------------------------------------------------------------------
// Variables set on the configuration web page

// Access point configuration
const char thingName[]              = APNAME;       // Initial AP name, used as SSID of the own Access Point
const char wifiInitialApPassword[]  = APPASSWORD;   // Initial password, used to the own Access Point

// Name of the server we want to get config from
const char cHost[]                  = CONFIG_HOST;
const int cPort                     = CONFIG_HOST_PORT;
const char cPath[]                  = CONFIG_HOST_FILE;
const char* configPath;
const char* tmpMqttServer;
String tamBox[CONFIG_NODES][5][CONFIG_PARAM]; // [MQTT, ][SERVER, , , , ][IP,PORT,USER,PASS,TOPIC, , ]
                                                    // [ ,NODE][DEST_A,DEST_B,DEST_C,DEST_D,OWN][ID,SIGN,NAME,TRACKS,EXIT,TOTTRACKS,TYPE]
bool notReceivedConfig;

// Device configuration
char cfgMqttServer[STRING_LEN];
char cfgMqttPort[NUMBER_LEN];
char cfgMqttTopic[STRING_LEN];
char cfgMqttUserName[STRING_LEN];
char cfgMqttUserPass[STRING_LEN];
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
String startPhase[4]    = {"    AP mode     ",      // AP_MODE
                           "Starting up...  ",      // STARTING_UP
                           "Error           ",      // ERROR
                           "Rebooting...    "};     // REBOOTING
String subPhase[4]      = {"  192.168.4.1   ",      // AP_MODE
                           "Loading config..",      // LOADING_CONF
                           "Starting MQTT   ",      // STARTING_MQTT
                           "Broker connected"};     // BROKER_CONNECTED
String startError[1]    = {"Broker not found"};     // BROKER_NOT_FOUND

// ------------------------------------------------------------------------------------------------------------------------------
// Define MQTT topic variables

// Variable for topics to subscribe to
const int nbrSubTopics = 4;
String subscripeTopic[nbrSubTopics];

// Variable for topics to publish to
const int nbrPubTopics = 3;
String pubTopic[nbrPubTopics];
String pubTopicContent[nbrPubTopics];

// Often used topics
String pubDeviceStateTopic;                         // Topic showing the state of device
String pubDirTopicFrwd[NUM_OF_DEST];                // Traffic direction [upwards, downwards]
String pubDirTopicBckw[NUM_OF_DEST];                // Traffic direction [upwards, downwards]
const byte NORETAIN   = 0;                          // Used to publish topics as NOT retained
const byte RETAIN     = 1;                          // Used to publish topics as retained

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

int destination                   = NOT_SELECTED;
int lcdRow                        = 0;
String trainNumber                = "";

// Variables for Train direction and Train Ids used in topics
String trainDir[NUM_DIR_STATES]   = {TOPIC_DIR_RIGHT,
                                     TOPIC_DIR_LEFT};
int trainId[NUM_OF_DEST][2];                         // Two tracks per destination

String nodeID[NUM_OF_DEST];
String nodeIDSuffix[NUM_OF_DEST]  = {DEST_A_ID, DEST_B_ID, DEST_C_ID, DEST_D_ID};

// Variables to store actual states
int tamBoxState[NUM_OF_DEST][2];                     // State per track and destination
String tamBoxStateTxt[NUM_OF_STATES]  = {"Not Used", "Idle", "Incoming Request", "Incoming Train", "Outgoing Request", "Outgoing Train"};

int traffDir[NUM_OF_DEST];                           // Traffic direction

unsigned long currTime;
unsigned long flashTime;

// Custom character for LCD
byte chr1[8]                      = {B00100,         // Character Å
                                     B01010,
                                     B00100,
                                     B01110,
                                     B10001,
                                     B11111,
                                     B10001,
                                     B10001};

// Debug
bool debug                        = SET_DEBUG;      // Set to true for debug messages in Serial monitor (9600 baud)
String dbText                     = "Main   : ";    // Debug text. Occurs first in every debug output

//HttpClient http = HttpClient(wifiClient, cHost, cPort);
HttpClient http(wifiClient, cHost, cPort);

// ------------------------------------------------------------------------------------------------------------------------------
//  Standard setup function
// ------------------------------------------------------------------------------------------------------------------------------
void setup() {

  // ----------------------------------------------------------------------------------------------------------------------------
  // Setup Arduino IDE serial monitor for "debugging"
  if (debug) {Serial.begin(115200);Serial.println("");}
  if (debug) {Serial.println(dbText+"Starting setup");}

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up LCD
  // The begin call takes the width and height. This
  // Should match the number provided to the constructor.
  lcd.begin(16,2);
  lcd.createChar(1, chr1);                          // Create character Å
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(startPhase[AP_MODE]);
  lcd.setCursor(0, 1);
  lcd.print(subPhase[AP_MODE]);

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up LED drivers
  FastLED.addLeds<WS2811, SIG_DATA_PIN, RGB>(leds, NUM_LED_DRIVERS);

  // ----------------------------------------------------------------------------------------------------------------------------
  // IotWebConfig start
  if (debug) {Serial.println(dbText+"IotWebConf setup");}

  // Adding items to each group
  webMqttGroup.addItem(&webMqttServer);             // MQTT Broker IP-adress
  webMqttGroup.addItem(&webMqttPort);               // MQTT Broker Port
  webMqttGroup.addItem(&webMqttUser);               // MQTT Broker user
  webMqttGroup.addItem(&webMqttPass);               // MQTT Broker password
  webMqttGroup.addItem(&webMqttTopic);              // MQTT root topic
  webDeviceGroup.addItem(&webClientID);             // MQTT Client uniqe ID
  webDeviceGroup.addItem(&webClientName);           // Own Station name
  webDeviceGroup.addItem(&webClientSign);           // Own Station signature
  webDeviceGroup.addItem(&webLedBrightness);        // LED Brightness
  webDeviceGroup.addItem(&webBackLight);            // LCD Backlight
  webModuleGroup.addItem(&webDestA);                // Destination A id
  webModuleGroup.addItem(&webDestSignA);            // Destination A signature
  webModuleGroup.addItem(&webDestExitA);            // Destination A exit
  webModuleGroup.addItem(&webTrackNumA);            // Destination A number of tracks
  webModuleGroup.addItem(&webDestB);                // Destination B id
  webModuleGroup.addItem(&webDestSignB);            // Destination B signature
  webModuleGroup.addItem(&webDestExitB);            // Destination B exit
  webModuleGroup.addItem(&webTrackNumB);            // Destination B number of tracks
  webModuleGroup.addItem(&webDestC);                // Destination C id
  webModuleGroup.addItem(&webDestSignC);            // Destination C signature
  webModuleGroup.addItem(&webDestExitC);            // Destination C exit
  webModuleGroup.addItem(&webTrackNumC);            // Destination C number of tracks
  webModuleGroup.addItem(&webDestD);                // Destination D id
  webModuleGroup.addItem(&webDestSignD);            // Destination D signature
  webModuleGroup.addItem(&webDestExitD);            // Destination D exit
  webModuleGroup.addItem(&webTrackNumD);            // Destination D number of tracks
  
  iotWebConf.setStatusPin(STATUS_PIN);
  if (debug) {Serial.println(dbText+"Status pin = "+STATUS_PIN);}

  iotWebConf.setConfigPin(CONFIG_PIN);
  if (debug) {Serial.println(dbText+"Config pin = "+CONFIG_PIN);}

  // Adding up groups to show on config web page
  iotWebConf.addParameterGroup(&webMqttGroup);      // MQTT Settings
  iotWebConf.addParameterGroup(&webDeviceGroup);    // Device Settings
  iotWebConf.addParameterGroup(&webModuleGroup);    // Other Stations

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
    tamBox[MQTT][SERVER][USER]    = "";
    tamBox[MQTT][SERVER][PASS]    = "";
    tamBox[MQTT][SERVER][TOPIC]   = ROOTTOPIC;
    clientName                    = APNAME;
    String tmpNo                  = String(random(2147483647));
    clientID                      = clientName+"-"+tmpNo;
    tamBox[NODE][OWN][SIGN]       = CLIENTSIGN;
    for (int i=0; i<NUM_OF_DEST; i++) {
      tamBox[NODE][i][ID]         = "-";
      tamBox[NODE][i][SIGN]       = "-";
      tamBox[NODE][i][NAME]       = "-";
      tamBox[NODE][i][EXIT]       = "A";
      tamBox[NODE][i][TOTTRACKS]  = "0";
    }
    ledBrightness                 = LED_BRIGHTNESS;
    lcdBackLight                  = LCD_BACKLIGHT;
  }

  else {
    clientID                        = String(cfgClientID);
    clientID.toLowerCase();
    clientName                      = String(cfgClientName);
    tamBox[NODE][OWN][SIGN]         = String(cfgClientSign);
    tamBox[MQTT][SERVER][IP]        = String(cfgMqttServer);
    tamBox[MQTT][SERVER][PORT]      = String(cfgMqttPort);
    tamBox[MQTT][SERVER][USER]      = String(cfgMqttUserName);
    tamBox[MQTT][SERVER][PASS]      = String(cfgMqttUserPass);
    tamBox[MQTT][SERVER][TOPIC]     = String(cfgMqttTopic);
    tamBox[NODE][DEST_A][ID]        = String(cfgDestA);
    tamBox[NODE][DEST_A][SIGN]      = String(cfgDestSignA);
    tamBox[NODE][DEST_A][NAME]      = "-";
    tamBox[NODE][DEST_A][EXIT]      = String(cfgDestExitA);
    tamBox[NODE][DEST_A][TOTTRACKS] = String(cfgTrackNumA);
    tamBox[NODE][DEST_B][ID]        = String(cfgDestB);
    tamBox[NODE][DEST_B][SIGN]      = String(cfgDestSignB);
    tamBox[NODE][DEST_B][NAME]      = "-";
    tamBox[NODE][DEST_B][EXIT]      = String(cfgDestExitB);
    tamBox[NODE][DEST_B][TOTTRACKS] = String(cfgTrackNumB);
    tamBox[NODE][DEST_C][ID]        = String(cfgDestC);
    tamBox[NODE][DEST_C][SIGN]      = String(cfgDestSignC);
    tamBox[NODE][DEST_C][NAME]      = "-";
    tamBox[NODE][DEST_C][EXIT]      = String(cfgDestExitC);
    tamBox[NODE][DEST_C][TOTTRACKS] = String(cfgTrackNumC);
    tamBox[NODE][DEST_D][ID]        = String(cfgDestD);
    tamBox[NODE][DEST_D][SIGN]      = String(cfgDestSignD);
    tamBox[NODE][DEST_D][NAME]      = "-";
    tamBox[NODE][DEST_D][EXIT]      = String(cfgDestExitD);
    tamBox[NODE][DEST_D][TOTTRACKS] = String(cfgTrackNumD);
    ledBrightness                   = atoi(cfgLedBrightness);
    lcdBackLight                    = atoi(cfgBackLight);
  }

  notReceivedConfig = true;

  FastLED.setBrightness(ledBrightness);
  lcd.setBacklight(lcdBackLight);

  for (int i=0; i<NUM_OF_DEST; i++) {
    nodeID[i] = clientID+nodeIDSuffix[i];
  }

  // Set up required URL handlers for the config web pages
  server.on("/", handleRoot);
  server.on("/config", []{iotWebConf.handleConfig();});
  server.onNotFound([](){iotWebConf.handleNotFound();});

  delay(2000);                                      // Wait for IotWebServer to start network connection

  // ----------------------------------------------------------------------------------------------------------------------------
  // Set-up i2c communication
  //Wire.begin();
  Keypad.begin();
    
  // ----------------------------------------------------------------------------------------------------------------------------
  // Set default values

  for (int i=0; i<NUM_OF_DEST; i++) {
    traffDir[i] = DIR_RIGHT;                        // Default traffic direction
    if (tamBox[NODE][i][ID] == "-") {
      tamBoxState[i][LEFT_TRACK] = STATE_NOTUSED;
      tamBoxState[i][RIGHT_TRACK] = STATE_NOTUSED;
    }

    else {
      if (tamBox[NODE][i][TOTTRACKS].toInt() == 1) {
        tamBoxState[i][LEFT_TRACK] = STATE_IDLE;
        tamBoxState[i][RIGHT_TRACK] = STATE_NOTUSED;
      }

      else {
        tamBoxState[i][LEFT_TRACK] = STATE_IDLE;
        tamBoxState[i][RIGHT_TRACK] = STATE_IDLE;
      }
    }
  }

  currTime = millis();
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Main program loop
// ------------------------------------------------------------------------------------------------------------------------------
void loop() {

  // Check connection to the MQTT broker. If no connection, try to reconnect
  if (needMqttConnect) {
    if (mqttConnect()) {
      needMqttConnect = false;
    }
  }

  else if ((iotWebConf.getState() == iotwebconf::OnLine) && (!mqttClient.connected())) {
    if (debug) {Serial.println(dbText+"MQTT reconnect");}
    mqttConnect();
  }

  if (needReset) {
    if (debug) {Serial.println("Rebooting after 1 second.");}
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(startPhase[REBOOTING]);
    iotWebConf.delay(1000);
    ESP.restart();
  }

  // Run repetitive jobs
  mqttClient.loop();                                // Wait for incoming MQTT messages
  iotWebConf.doLoop();                              // Check for IotWebConfig actions

  char key = Keypad.getKey();                       // Get key input
  if (key) {
    keyReceived(key);
  }
}


// ------------------------------------------------------------------------------------------------------------------------------
// Insert x number of blanks
// ------------------------------------------------------------------------------------------------------------------------------
String setLcdBlanks(int blanks) {

  String s = "";
  for (int i=1; i<=blanks; i++) {s += " ";}
  return s;
}


// ------------------------------------------------------------------------------------------------------------------------------
// Set string
// ------------------------------------------------------------------------------------------------------------------------------
void setLcdString(int str, int dest) {

//                                  111111
//stringpos               0123456789012345
  String string[3]    = {"     TAG#       ",        // TRAIN
                         "  *=ANGRA #=OK  ",        // CANCEL_OK
                         "   *=NOK #=OK   "};       // NOK_OK
  int sweCharTrain    = 6;                          // Swedish character Å in TÅG
  int sweCharCancel   = 4;                          // Swedish character Å in ÅNGRA
  int iCol            = 9;                          // Cursor position when inserting Train number
  int cRow;                                         // Clear row
  int cCol;                                         // Clear stringpos start
  int iRow;                                         // Insert row

  switch (dest) {
    case DEST_A:
      cRow = 0;
      cCol = 8;
      iRow = 1;
    break;

    case DEST_B:
      cRow = 0;
      cCol = 0;
      iRow = 1;
    break;

    case DEST_C:
      cRow = 1;
      cCol = 8;
      iRow = 0;
    break;

    case DEST_D:
      cRow = 1;
      cCol = 0;
      iRow = 0;
    break;
  }

  switch (str) {
    case TRAIN:
      lcd.setCursor(cCol, cRow);
      lcd.print(setLcdBlanks(8));                   // 8 Blanks
      lcd.setCursor(0, iRow);
      lcd.print(string[str]);
      lcd.setCursor(sweCharTrain, iRow);
      lcd.write(1);                                 // Å
      lcd.cursor();
      lcd.setCursor(iCol, iRow);
      lcd.blink();
    break;

    case CANCEL_OK:
      lcd.setCursor(cCol, cRow);
      lcd.print(setLcdBlanks(8));                   // 8 Blanks
      lcd.setCursor(0, iRow);
      lcd.print(string[str]);
      lcd.setCursor(sweCharCancel, iRow);
      lcd.write(1);                                 // Å
    break;

    case NOK_OK:
      lcd.setCursor(cCol, cRow);
      lcd.print(setLcdBlanks(8));                   // 8 Blanks
      lcd.setCursor(0, iRow);
      lcd.print(string[str]);
    break;
  }
}


// ------------------------------------------------------------------------------------------------------------------------------
// Key received
//
//  procedurs
//  Incomming request on normal track from destination A
//    # Accept      OK
//    * Deny        NOK
//  Report train in on normal track from destination A
//    A#
//  Report train in on other track from destination C
//    CC#
//  Request outgoing train on normal track to destination C
//    C{train no}
//    # Confirm
//    * Reject
//  Request outgoing train on other track destination C
//    CC{train no}
//    # Confirm
//    * Reject
//  
// ------------------------------------------------------------------------------------------------------------------------------
void keyReceived(char key) {

  switch (key) {
    case '*':                                       // NOK, Not accepted
      if (destination < NOT_SELECTED) {
        switch (tamBoxState[destination][LEFT_TRACK]) {
          case STATE_INREQUEST:                     // Reject Incoming request
            if (debug) {Serial.println(dbText+"State change: "+nodeIDSuffix[destination]+" From:"+tamBoxStateTxt[tamBoxState[destination][LEFT_TRACK]]+" To: "+tamBoxStateTxt[STATE_IDLE]);}
            tamBoxState[destination][LEFT_TRACK] = STATE_IDLE;
            destination = NOT_SELECTED;
            if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
            mqttPublish(tamBox[MQTT][SERVER][TOPIC]+"/"+
                        tamBox[NODE][destination][ID]+"/"+
                        nodeIDSuffix[destination]+"/"+
                        TOPIC_TRAFFIC+"/"+
                        trainDir[traffDir[destination]]+"/"+
                        TOPIC_TRAIN+"/"+
                        TOPIC_REJECT, String(trainId[destination][LEFT_TRACK]), NORETAIN);
            setRunningString();
          break;

          case STATE_OUREQUEST:                     // Reject Outgoing request
            trainNumber = "";
            if (debug) {Serial.println(dbText+"State change: "+nodeIDSuffix[destination]+" From:"+tamBoxStateTxt[tamBoxState[destination][LEFT_TRACK]]+" To: "+tamBoxStateTxt[STATE_IDLE]);}
            tamBoxState[destination][LEFT_TRACK] = STATE_IDLE;
            destination = NOT_SELECTED;
            if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
            setRunningString();
            trainNumber = "";
          break;

          default:
            destination = NOT_SELECTED;
            if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
          break;
        }
      }
    break;

    case '#':                                       // OK, Accepted
      if (destination < NOT_SELECTED) {
        switch (tamBoxState[destination][LEFT_TRACK]) {
          case STATE_INTRAIN:                       // Report Incoming train
            if (debug) {Serial.println(dbText+"State change: "+nodeIDSuffix[destination]+" From:"+tamBoxStateTxt[tamBoxState[destination][LEFT_TRACK]]+" To: "+tamBoxStateTxt[STATE_IDLE]);}
            tamBoxState[destination][LEFT_TRACK] = STATE_IDLE;
            destination = NOT_SELECTED;
            if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
            mqttPublish(tamBox[MQTT][SERVER][TOPIC]+"/"+
                        tamBox[NODE][destination][ID]+"/"+
                        nodeIDSuffix[destination]+"/"+
                        TOPIC_TRAFFIC+"/"+
                        trainDir[traffDir[destination]]+"/"+
                        TOPIC_TRAIN+"/"+
                        TOPIC_ARRIVED, String(trainId[destination][LEFT_TRACK]), NORETAIN);
            setRunningString();
          break;

          case STATE_INREQUEST:                     // Accept Incoming request
            if (debug) {Serial.println(dbText+"State change: "+nodeIDSuffix[destination]+" From:"+tamBoxStateTxt[tamBoxState[destination][LEFT_TRACK]]+" To: "+tamBoxStateTxt[STATE_INTRAIN]);}
            tamBoxState[destination][LEFT_TRACK] = STATE_INTRAIN;
            destination = NOT_SELECTED;
            if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
            mqttPublish(tamBox[MQTT][SERVER][TOPIC]+"/"+
                        tamBox[NODE][destination][ID]+"/"+
                        nodeIDSuffix[destination]+"/"+
                        TOPIC_TRAFFIC+"/"+
                        trainDir[traffDir[destination]]+"/"+
                        TOPIC_TRAIN+"/"+
                        TOPIC_ACCEPT, String(trainId[destination][LEFT_TRACK]), NORETAIN);
            setRunningString();
          break;

          case STATE_OUREQUEST:                     // Confirm Outgoing request
//            if (debug) {Serial.println(dbText+"State change: "+nodeIDSuffix[destination]+" From:"+tamBoxStateTxt[tamBoxState[destination][LEFT_TRACK]]+" To: "+tamBoxStateTxt[STATE_OUTTRAIN]);}
//            tamBoxState[destination][0] = STATE_OUTTRAIN;
            trainId[destination][LEFT_TRACK] = trainNumber.toInt();
            lcd.noCursor();
            lcd.noBlink();
            destination = NOT_SELECTED;
            if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
            mqttPublish(tamBox[MQTT][SERVER][TOPIC]+"/"+
                        tamBox[NODE][destination][ID]+"/"+
                        nodeIDSuffix[destination]+"/"+
                        TOPIC_TRAFFIC+"/"+
                        trainDir[traffDir[destination]]+"/"+
                        TOPIC_TRAIN+"/"+
                        TOPIC_REQUEST, String(trainId[destination][LEFT_TRACK]), NORETAIN);
            setRunningString();
            trainNumber = "";
          break;

          default:
            destination = NOT_SELECTED;
            if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
          break;
        }
      }
    break;

    case 'A':                                       // Set Direction A
      destination = DEST_A;
      if (debug) {Serial.println(dbText+"Destination "+nodeIDSuffix[destination]+" set");}
      lcdRow = 1;
      switch (tamBoxState[destination][LEFT_TRACK]) {
        case STATE_IDLE:
          setLcdString(TRAIN, destination);
          traffDir[destination] = DIR_LEFT;
          tamBoxState[destination][LEFT_TRACK] = STATE_OUREQUEST;
          if (debug) {Serial.println(dbText+"State change: "+nodeIDSuffix[destination]+" = "+tamBoxStateTxt[tamBoxState[destination][LEFT_TRACK]]);}
        break;

        case STATE_OUREQUEST: 
          setLcdString(CANCEL_OK, destination);
        break;

        default:
          destination = NOT_SELECTED;
          if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
        break;
      }
    break;

    case 'B':                                       // Set Direction B
      destination = DEST_B;
      if (debug) {Serial.println(dbText+"Destination "+nodeIDSuffix[destination]+" set");}
      lcdRow = 1;
      switch (tamBoxState[destination][LEFT_TRACK]) {
        case STATE_IDLE:
          setLcdString(TRAIN, destination);
          traffDir[destination] = DIR_RIGHT;
          tamBoxState[destination][LEFT_TRACK] = STATE_OUREQUEST;
          if (debug) {Serial.println(dbText+"State change: "+nodeIDSuffix[destination]+" = "+tamBoxStateTxt[tamBoxState[destination][LEFT_TRACK]]);}
        break;

        case STATE_OUREQUEST: 
          setLcdString(CANCEL_OK, destination);
        break;

        default:
          destination = NOT_SELECTED;
          if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
        break;
      }
    break;

    case 'C':                                       // Set Direction C
      destination = DEST_C;
      if (debug) {Serial.println(dbText+"Destination "+nodeIDSuffix[destination]+" set");}
      lcdRow = 0;
      switch (tamBoxState[destination][LEFT_TRACK]) {
        case STATE_IDLE:
          setLcdString(TRAIN, destination);
          traffDir[destination] = DIR_LEFT;
          tamBoxState[destination][LEFT_TRACK] = STATE_OUREQUEST;
          if (debug) {Serial.println(dbText+"State change: "+nodeIDSuffix[destination]+" = "+tamBoxStateTxt[tamBoxState[destination][LEFT_TRACK]]);}
        break;

        case STATE_OUREQUEST: 
          setLcdString(CANCEL_OK, destination);
        break;

        default:
          destination = NOT_SELECTED;
          if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
        break;
      }
    break;

    case 'D':                                       // Set Direction D
      destination = DEST_D;
      if (debug) {Serial.println(dbText+"Destination "+nodeIDSuffix[destination]+" set");}
      lcdRow = 0;
      switch (tamBoxState[destination][LEFT_TRACK]) {
        case STATE_IDLE:
          setLcdString(TRAIN, destination);
          traffDir[destination] = DIR_RIGHT;
          tamBoxState[destination][LEFT_TRACK] = STATE_OUREQUEST;
          if (debug) {Serial.println(dbText+"State change: "+nodeIDSuffix[destination]+" = "+tamBoxStateTxt[tamBoxState[destination][LEFT_TRACK]]);}
        break;

        case STATE_OUREQUEST: 
          setLcdString(CANCEL_OK, destination);
        break;

        default:
          destination = NOT_SELECTED;
          if (debug) {Serial.println(dbText+"Destination NOT_SELECTED set");}
        break;
      }
    break;

    default:                                        // Numbers
      if (destination < NOT_SELECTED) {
        switch (tamBoxState[destination][LEFT_TRACK]) {
          case STATE_OUREQUEST:
            trainNumber = trainNumber+key;
            lcd.print(key);
            lcd.setCursor(9+trainNumber.length(), lcdRow);
            lcd.blink();
            if (debug) {Serial.println(dbText+"Train number = "+trainNumber);}
          break;

          default:
            if (debug) {Serial.println(dbText+"Default");}
          break;
        }
      }
    break;
  }
}


// ------------------------------------------------------------------------------------------------------------------------------
// (Re)connects to MQTT broker and subscribes to one or more topics
// ------------------------------------------------------------------------------------------------------------------------------
bool mqttConnect() {

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
    lcd.setCursor(0, 0);
    lcd.print(startPhase[STARTING_UP]);
    lcd.setCursor(0, 1);
    lcd.print(subPhase[STARTING_MQTT]);

    if (debug) {Serial.print(dbText+"MQTT connection...");}

    // Attempt to connect
    // boolean connect (tmpID, pubDeviceStateTopic, willQoS, willRetain, willMessage)
    if (mqttClient.connect(tmpID, tmpTopic, 0, true, tmpMessage)) {

      lcd.setCursor(0, 0);
      lcd.print(startPhase[STARTING_UP]);
      lcd.setCursor(0, 1);
      lcd.print(subPhase[BROKER_CONNECTED]);
      if (debug) {Serial.println("connected");}
      if (debug) {Serial.print(dbText+"MQTT client id = ");}
      if (debug) {Serial.println(cfgClientID);}

      // Subscribe to all topics
      if (debug) {Serial.println(dbText+"Subscribing to:");}

      for (int i=0; i<nbrSubTopics; i++){
        // Convert String to char* for the mqttClient.subribe() function to work
        if (subscripeTopic[i].length() > 0) {
          subscripeTopic[i].toCharArray(tmpTopic, subscripeTopic[i].length()+1);

          // ... print topic
          if (debug) {Serial.println(dbText+" - "+tmpTopic);}

          //   ... and subscribe to topic
          mqttClient.subscribe(tmpTopic);
        }
      }

      // Publish to all topics
      if (debug) {Serial.println(dbText+"Publishing:");}

      for (int i=0; i<nbrPubTopics; i++){
        // Convert String to char* for the mqttClient.publish() function to work
        pubTopic[i].toCharArray(tmpTopic, pubTopic[i].length()+1);
        pubTopicContent[i].toCharArray(tmpContent, pubTopicContent[i].length()+1);

        // ... print topic
        if (debug) {Serial.print(dbText+" - "+tmpTopic);}
        if (debug) {Serial.print(" = ");}
        if (debug) {Serial.println(tmpContent);}

        // ... and subscribe to topic
        mqttClient.publish(tmpTopic, tmpContent, true);
      }
    }

    else {
      // Show why the connection failed
      lcd.setCursor(0, 0);
      lcd.print(startPhase[ERROR]);
      lcd.setCursor(0, 1);
      lcd.print(startError[BROKER_NOT_FOUND]);

      if (debug) {Serial.print(dbText+"failed, rc=");}
      if (debug) {Serial.print(mqttClient.state());}
      if (debug) {Serial.println(", try again in 5 seconds");}

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  // Set device status to "ready"
  setRunningString();

  for (int i=0; i<NUM_OF_DEST; i++) {
    if (tamBox[NODE][i][TOTTRACKS].toInt() == 1) {
//      mqttPublish(pubDirTopicFrwd[1], trainDir[traffDir[i]], RETAIN);                 // Current traffic direction
    }
    if (tamBox[NODE][i][TOTTRACKS].toInt() == 2) {
//      mqttPublish(pubDirTopicBckw[1], trainDir[traffDir[i]], RETAIN);                 // Current traffic direction
    }
  }

  mqttPublish(pubDeviceStateTopic, "ready", RETAIN);                                    // Node ready

  if (debug) {Serial.println("TamBox Redy!");}

  return true;
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Function to handle MQTT messages sent to this node
//
//  ex mqtt_n/tambox-4/b/traffic/up/direction/state  {up,down}
//     mqtt_n/tambox-2/a/traffic/up/train/request     233
//     mqtt_n/tambox-3/a/traffic/down/train/accept    1234
//     mqtt_n/tambox-2/a/traffic/up/train/reject      233
// ------------------------------------------------------------------------------------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  // Don't know why this have to be done :-(
  payload[length] = '\0';

  // Make strings
  String msg = String((char*)payload);
  String tpc = String((char*)topic);

  int MyP = 0;
  int MyI = 0;
  int trk = 0;                                                    // "up"
  int ext = 0;                                                    // "a"
  String subTopic[7];                                             // Topic array

  // Split topic string into an array of the seven parts
  for (int i=0; i<7; i++) {
    MyI = tpc.indexOf("/", MyP);
    String s = tpc.substring(MyP, MyI);
    MyP = MyI + 1;
    subTopic[i] = s;
  }

  if (subTopic[TOPIC_TOP] == tamBox[MQTT][SERVER][TOPIC]) {       // Our Root Topic
    // Print the topic and payload received
    if (debug) {Serial.println(dbText+"Recieved: "+tpc+" = "+msg);}

    // Convert received strings to a integer
    if      (subTopic[TOPIC_EXIT] == DEST_B_ID) {ext = 1;}
    else if (subTopic[TOPIC_EXIT] == DEST_C_ID) {ext = 2;}
    else if (subTopic[TOPIC_EXIT] == DEST_D_ID) {ext = 3;}
    if (subTopic[TOPIC_TRACK] == "down") {trk = 1;}

    // Check incoming topics so they belong to our subscribed topics
    if (subTopic[TOPIC_NODE] == tamBox[NODE][DEST_A][ID] && subTopic[TOPIC_EXIT] == nodeIDSuffix[DEST_A] ||
        subTopic[TOPIC_NODE] == tamBox[NODE][DEST_B][ID] && subTopic[TOPIC_EXIT] == nodeIDSuffix[DEST_B] ||
        subTopic[TOPIC_NODE] == tamBox[NODE][DEST_C][ID] && subTopic[TOPIC_EXIT] == nodeIDSuffix[DEST_C] ||
        subTopic[TOPIC_NODE] == tamBox[NODE][DEST_D][ID] && subTopic[TOPIC_EXIT] == nodeIDSuffix[DEST_D]) {

      // Check Traffic type
      if (subTopic[TOPIC_TYPE] == TOPIC_TRAFFIC) {

        if (subTopic[TOPIC_ID] == TOPIC_DIR) {                    // Direction order
          handleDirection(subTopic[TOPIC_NODE], ext, trk, subTopic[TOPIC_ORDER], msg);
        }

        else if (subTopic[TOPIC_ID] == TOPIC_TRAIN) {             // Train order
          handleTrain(subTopic[TOPIC_NODE], ext, trk, subTopic[TOPIC_ORDER], msg);
        }

        else {
          if (debug) {Serial.println(dbText+"Not supported id: "+subTopic[TOPIC_ID]);}
        }
      }

      // If $ starts the third topic part it is a status message
      else if (subTopic[TOPIC_EXIT].substring(0, 1) == "$") {
          if (debug) {Serial.println(dbText+subTopic[TOPIC_NODE]+"/"+subTopic[TOPIC_EXIT]+": "+msg);}
      }

      else {
        if (debug) {Serial.println(dbText+"Not supported type: "+subTopic[TOPIC_TYPE]);}
      }
    }

    else {
      if (debug) {Serial.println(dbText+"Not my subscribed node: "+subTopic[TOPIC_NODE]+"/"+subTopic[TOPIC_EXIT]);}
    }
  }

  else {
    if (debug) {Serial.println(dbText+"Not our root topic: "+subTopic[TOPIC_TOP]);}
  }
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Function that gets called when a direction message is received
//
//  ex tambox-4, 1, 0, state,  {up,down}
// ------------------------------------------------------------------------------------------------------------------------------
void handleDirection(String dest, int ext, int trk, String ord, String msg) {

  int direction = NOT_SELECTED;

  if (ord == TOPIC_STATE) {                                       // Incoming state
    for (int i=0; i<NUM_DIR_STATES; i++) {
      if (msg == trainDir[i]) {
        direction = i;
      }
    }

    switch (direction) {
      case DIR_RIGHT:
      case DIR_LEFT:
        traffDir[ext] = direction;
      break;

      default:
        if (debug) {Serial.println(dbText+"Message '"+msg+"' not supported");}
      break;
    }

    if (debug) {
      switch (direction) {
        case DIR_RIGHT:
          Serial.println(dbText+"Traffic on track "+trk+" ->");
        break;

        case DIR_LEFT:
          Serial.println(dbText+"Traffic on track "+trk+" <-");
        break;
      }
    }
  }

  else {
    if (debug) {Serial.println(dbText+"Not supported order: "+ord);}
  }
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Function that gets called when a train message is received
//
//  ex tambox-2, 0, 1, request, 233
// ------------------------------------------------------------------------------------------------------------------------------
void handleTrain(String dest, int ext, int trk, String ord, String msg) {

  if (ord == TOPIC_REQUEST) {                                     // Incoming request
    switch (tamBoxState[ext][trk]) {
      case STATE_IDLE:
        trainId[ext][trk] = msg.toInt();
        if (debug) {Serial.println(dbText+"Incoming request from: "+dest+" with train "+trainId[ext][trk]);}
        if (debug) {Serial.println(dbText+"State changed from: "+tamBoxStateTxt[tamBoxState[ext][trk]]+" to: "+tamBoxStateTxt[STATE_INREQUEST]);}
        tamBoxState[ext][trk] = STATE_INREQUEST;
        setLcdString(NOK_OK, ext);
      break;

      default:
        if (debug) {Serial.println(dbText+"Wrong state for incoming request");}
      break;
    }
  }

  else if (ord == TOPIC_ACCEPT) {                                 // Incoming accept
    switch (tamBoxState[ext][trk]) {
      case STATE_OUREQUEST:
        if (trainId[ext][trk] == msg.toInt()) {
          if (debug) {Serial.println(dbText+"Outgoing request to: "+dest+" with train "+trainId[ext][trk]+" accepted");}
          if (debug) {Serial.println(dbText+"State changed from: "+tamBoxStateTxt[tamBoxState[ext][trk]]+" to: "+tamBoxStateTxt[STATE_OUTTRAIN]);}
          tamBoxState[ext][trk] = STATE_OUTTRAIN;
        }

        else {
          if (debug) {Serial.println(dbText+"Wrong train for incoming accept: "+msg);}
        }
      break;

      default:
        if (debug) {Serial.println(dbText+"Wrong state for incoming accept");}
      break;
    }
  }

  else if (ord == TOPIC_REJECT) {                                 // Incoming reject
    switch (tamBoxState[ext][trk]) {
      case STATE_OUREQUEST:
        if (trainId[ext][trk] == msg.toInt()) {
          trainId[ext][trk] = 0;
          if (debug) {Serial.println(dbText+"Outgoing request to: "+dest+" with train "+msg+" rejected");}
          if (debug) {Serial.println(dbText+"State changed from: "+tamBoxStateTxt[tamBoxState[ext][trk]]+" to: "+tamBoxStateTxt[STATE_IDLE]);}
          tamBoxState[ext][trk] = STATE_IDLE;
        }

        else {
          if (debug) {Serial.println(dbText+"Wrong train for incoming reject: "+msg);}
        }
      break;

      default:
        if (debug) {Serial.println(dbText+"Wrong state for incoming reject");}
      break;
    }
  }

  else if (ord == TOPIC_ARRIVED) {                                // Incoming arrivel report
    switch (tamBoxState[ext][trk]) {
      case STATE_OUTTRAIN:
        if (trainId[ext][trk] == msg.toInt()) {
          trainId[ext][trk] = 0;
          if (debug) {Serial.println(dbText+"Outgoing train to: "+dest+" with train "+msg+" arrived");}
          if (debug) {Serial.println(dbText+"State changed from: "+tamBoxStateTxt[tamBoxState[ext][trk]]+" to: "+tamBoxStateTxt[STATE_IDLE]);}
          tamBoxState[ext][trk] = STATE_IDLE;
        }

        else {
          if (debug) {Serial.println(dbText+"Wrong train for incoming arrivel report: "+msg);}
        }
      break;

      default:
        if (debug) {Serial.println(dbText+"Wrong state for incoming arrivel report");}
      break;
    }
  }

  else {
    if (debug) {Serial.println(dbText+"Not supported order: "+ord);}
  }
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Publish message to MQTT broker
// ------------------------------------------------------------------------------------------------------------------------------
void mqttPublish(String pbTopic, String pbPayload, byte retain) {

  // Convert String to char* for the mqttClient.publish() function to work
  char msg[pbPayload.length()+1];
  pbPayload.toCharArray(msg, pbPayload.length()+1);
  char tpc[pbTopic.length()+1];
  pbTopic.toCharArray(tpc, pbTopic.length()+1);

  // Report back to pubTopic[]
  int check = mqttClient.publish(tpc, msg, retain);

  // TODO check "check" integer to see if all went ok

  // Print information
  if (debug) {Serial.println(dbText+"Publish: "+pbTopic+" = "+pbPayload);}
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Function to download config file
// ------------------------------------------------------------------------------------------------------------------------------
bool getConfigFile() {

// http://configserver.000webhostapp.com/?id=tambox-1

//  http.beginRequest();
  http.get(configPath);
//  http.sendHeader("Host",cHost);
//  http.endRequest();
  int statusCode = http.responseStatusCode();
  String response = http.responseBody();
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
        "type":"double",
        "double":{
          "id":"tambox-4",
          "tracks":2,
          "exit":"A",
          "signature":"SAL",
          "name":"Salsborg"
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
    "rootTopic":"mqtt_n"
  }
}
 */
  // Allocate the JSON document
  // Use arduinojson.org/v6/assistant to compute the capacity.
//  const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
  DynamicJsonDocument doc(2048);

  if (debug) {Serial.println("Status Code: "+statusCode);}
  if (debug) {Serial.println("Response: "+response);}
  // Parse JSON object
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    if (debug) {Serial.print(F("deserializeJson() failed: "));}
    if (debug) {Serial.println(error.c_str());}
    return false;
  }

  else {
    if (String(doc["id"].as<char*>()) == clientID) {
      tamBox[MQTT][SERVER][IP]        = String(doc["mqtt"]["server"].as<char*>());
      tamBox[MQTT][SERVER][PORT]      = String(doc["mqtt"]["port"].as<int>());
      tamBox[MQTT][SERVER][USER]      = String(doc["mqtt"]["user"].as<char*>());
      tamBox[MQTT][SERVER][PASS]      = String(doc["mqtt"]["pass"].as<char*>());
      tamBox[MQTT][SERVER][TOPIC]     = String(doc["mqtt"]["rootTopic"].as<char*>());
      tamBox[NODE][OWN][ID]           = String(doc["id"].as<char*>());                                    // Own station
      tamBox[NODE][OWN][SIGN]         = String(doc["config"]["signature"].as<char*>());
      tamBox[NODE][OWN][NAME]         = String(doc["config"]["name"].as<char*>());
      tamBox[NODE][OWN][NUMOFDEST]    = String(doc["config"]["destinations"].as<int>());
      tamBox[NODE][DEST_A][TOTTRACKS] = String(doc["config"]["destination"]["A"]["tracks"].as<int>());    // destination A
      tamBox[NODE][DEST_A][TYPE]      = String(doc["config"]["destination"]["A"]["type"].as<char*>());
      tamBox[NODE][DEST_A][ID]        = String(doc["config"]["destination"]["A"][String(doc["config"]["destination"]["A"]["type"].as<char*>())]["id"].as<char*>());
      tamBox[NODE][DEST_A][SIGN]      = String(doc["config"]["destination"]["A"][String(doc["config"]["destination"]["A"]["type"].as<char*>())]["signature"].as<char*>());
      tamBox[NODE][DEST_A][NAME]      = String(doc["config"]["destination"]["A"][String(doc["config"]["destination"]["A"]["type"].as<char*>())]["name"].as<char*>());
      tamBox[NODE][DEST_A][TRACKS]    = String(doc["config"]["destination"]["A"][String(doc["config"]["destination"]["A"]["type"].as<char*>())]["tracks"].as<int>());
      tamBox[NODE][DEST_A][EXIT]      = String(doc["config"]["destination"]["A"][String(doc["config"]["destination"]["A"]["type"].as<char*>())]["exit"].as<char*>());
      tamBox[NODE][DEST_B][TOTTRACKS] = String(doc["config"]["destination"]["B"]["tracks"].as<int>());    // destination B
      tamBox[NODE][DEST_B][TYPE]      = String(doc["config"]["destination"]["B"]["type"].as<char*>());
      tamBox[NODE][DEST_B][ID]        = String(doc["config"]["destination"]["B"][String(doc["config"]["destination"]["B"]["type"].as<char*>())]["id"].as<char*>());
      tamBox[NODE][DEST_B][SIGN]      = String(doc["config"]["destination"]["B"][String(doc["config"]["destination"]["B"]["type"].as<char*>())]["signature"].as<char*>());
      tamBox[NODE][DEST_B][NAME]      = String(doc["config"]["destination"]["B"][String(doc["config"]["destination"]["B"]["type"].as<char*>())]["name"].as<char*>());
      tamBox[NODE][DEST_B][TRACKS]    = String(doc["config"]["destination"]["B"][String(doc["config"]["destination"]["B"]["type"].as<char*>())]["tracks"].as<int>());
      tamBox[NODE][DEST_B][EXIT]      = String(doc["config"]["destination"]["B"][String(doc["config"]["destination"]["B"]["type"].as<char*>())]["exit"].as<char*>());
      tamBox[NODE][DEST_C][TOTTRACKS] = String(doc["config"]["destination"]["C"]["tracks"].as<int>());    // destination C
      tamBox[NODE][DEST_C][TYPE]      = String(doc["config"]["destination"]["C"]["type"].as<char*>());
      tamBox[NODE][DEST_C][ID]        = String(doc["config"]["destination"]["C"][String(doc["config"]["destination"]["C"]["type"].as<char*>())]["id"].as<char*>());
      tamBox[NODE][DEST_C][SIGN]      = String(doc["config"]["destination"]["C"][String(doc["config"]["destination"]["C"]["type"].as<char*>())]["signature"].as<char*>());
      tamBox[NODE][DEST_C][NAME]      = String(doc["config"]["destination"]["C"][String(doc["config"]["destination"]["C"]["type"].as<char*>())]["name"].as<char*>());
      tamBox[NODE][DEST_C][TRACKS]    = String(doc["config"]["destination"]["C"][String(doc["config"]["destination"]["C"]["type"].as<char*>())]["tracks"].as<int>());
      tamBox[NODE][DEST_C][EXIT]      = String(doc["config"]["destination"]["C"][String(doc["config"]["destination"]["C"]["type"].as<char*>())]["exit"].as<char*>());
      tamBox[NODE][DEST_D][TOTTRACKS] = String(doc["config"]["destination"]["D"]["tracks"].as<int>());    // destination D
      tamBox[NODE][DEST_D][TYPE]      = String(doc["config"]["destination"]["D"]["type"].as<char*>());
      tamBox[NODE][DEST_D][ID]        = String(doc["config"]["destination"]["D"][String(doc["config"]["destination"]["D"]["type"].as<char*>())]["id"].as<char*>());
      tamBox[NODE][DEST_D][SIGN]      = String(doc["config"]["destination"]["D"][String(doc["config"]["destination"]["D"]["type"].as<char*>())]["signature"].as<char*>());
      tamBox[NODE][DEST_D][NAME]      = String(doc["config"]["destination"]["D"][String(doc["config"]["destination"]["D"]["type"].as<char*>())]["name"].as<char*>());
      tamBox[NODE][DEST_D][TRACKS]    = String(doc["config"]["destination"]["D"][String(doc["config"]["destination"]["D"]["type"].as<char*>())]["tracks"].as<int>());
      tamBox[NODE][DEST_D][EXIT]      = String(doc["config"]["destination"]["D"][String(doc["config"]["destination"]["D"]["type"].as<char*>())]["exit"].as<char*>());
      if (debug) {Serial.println("Config received");}

      return true;
    }
  }

  return false;
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Function to set-up all topics
// ------------------------------------------------------------------------------------------------------------------------------
void setupTopics() {

  if (debug) {Serial.println(dbText+"Topics setup");}

  // Subscribe
  // Topic                körning/client/#
  for (int i=0; i<NUM_OF_DEST; i++) {
    if (tamBoxState[i][LEFT_TRACK] != STATE_NOTUSED) {
      subscripeTopic[i] = tamBox[MQTT][SERVER][TOPIC]+"/"+tamBox[NODE][i][ID]+"/#";
    }
  }

  // Publish - Client
  // Topic                körning/client/$
  pubTopic[0]           = tamBox[MQTT][SERVER][TOPIC]+"/"+clientID+"/$id";
  pubTopicContent[0]    = clientID;
  pubTopic[1]           = tamBox[MQTT][SERVER][TOPIC]+"/"+clientID+"/$name";
  pubTopicContent[1]    = clientName;
  pubTopic[2]           = tamBox[MQTT][SERVER][TOPIC]+"/"+clientID+"/$type";
  pubTopicContent[2]    = "TamBox";
  pubDeviceStateTopic   = tamBox[MQTT][SERVER][TOPIC]+"/"+clientID+"/$state";

  if (debug) {Serial.println(dbText+"Topics setup done!");}
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Prepare MQTT broker and define function to handle callbacks
// ------------------------------------------------------------------------------------------------------------------------------
void setupBroker() {

  tmpMqttServer = tamBox[MQTT][SERVER][IP].c_str();

  if (debug) {Serial.println(dbText+"MQTT setup");}

  mqttClient.setServer(tmpMqttServer, tamBox[MQTT][SERVER][PORT].toInt());
  mqttClient.setCallback(mqttCallback);

  if (debug) {Serial.println(dbText+"MQTT setup done!");}
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Function to show AP web start page
// ------------------------------------------------------------------------------------------------------------------------------
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

// ------------------------------------------------------------------------------------------------------------------------------
//  Function beeing called when wifi connection is up and running
// ------------------------------------------------------------------------------------------------------------------------------
void wifiConnected() {

  lcd.setCursor(0, 0);
  lcd.print(startPhase[STARTING_UP]);
  lcd.setCursor(0, 1);
  lcd.print(subPhase[LOADING_CONF]);

//  long rssi = wifiClient.RSSI();
//  if (debug) {Serial.println(dbText+"Signal strength (RSSI): "+rssi+" dBm");}

  String pathID = cPath+clientID;
  configPath = pathID.c_str();
  if (debug) {Serial.println(dbText+"configPath ="+configPath);}

  if (getConfigFile()) {
    if (debug) {Serial.println(dbText+"config received");}
    notReceivedConfig = false;

    for (int i=0; i<NUM_OF_DEST; i++) {
      if (tamBox[NODE][i][ID] == "-") {                 // Not used Destination
        tamBoxState[i][LEFT_TRACK] = STATE_NOTUSED;
        tamBoxState[i][RIGHT_TRACK] = STATE_NOTUSED;
      }

      else {
        if (tamBox[NODE][i][TYPE] == "single") {        // Single track Destination
          tamBoxState[i][LEFT_TRACK] = STATE_IDLE;      // Single track
          tamBoxState[i][RIGHT_TRACK] = STATE_NOTUSED;
        }

        else if (tamBox[NODE][i][TYPE] == "split") {    // Single track to two Destination (Not supported yet)
          tamBoxState[i][LEFT_TRACK] = STATE_IDLE;      // Single track Destination 1
          tamBoxState[i][RIGHT_TRACK] = STATE_IDLE;     // Single track Destination 2
        }

        else {                                          // Double track Destination
          tamBoxState[i][LEFT_TRACK] = STATE_IDLE;
          tamBoxState[i][RIGHT_TRACK] = STATE_IDLE;
        }
      }

      if (debug) {Serial.println(dbText+"TamBoxState destination "+nodeIDSuffix[i]+" track left set to "+tamBoxStateTxt[tamBoxState[i][0]]);}
      if (debug) {Serial.println(dbText+"TamBoxState destination "+nodeIDSuffix[i]+" track right set to "+tamBoxStateTxt[tamBoxState[i][0]]);}
    }
  }
  // We are ready to start the MQTT connection
  needMqttConnect = true;
}


// ------------------------------------------------------------------------------------------------------------------------------
//  Function that gets called when web page config has been saved
// ------------------------------------------------------------------------------------------------------------------------------
void configSaved() {

  if (debug) {Serial.println(dbText+"IotWebConf config saved");}
  clientID                          = String(cfgClientID);
  clientID.toLowerCase();
  clientName                        = String(cfgClientName);
  if (notReceivedConfig) {
    tamBox[MQTT][SERVER][IP]        = String(cfgMqttServer);
    tamBox[MQTT][SERVER][PORT]      = String(cfgMqttPort);
    tamBox[MQTT][SERVER][USER]      = String(cfgMqttUserName);
    tamBox[MQTT][SERVER][PASS]      = String(cfgMqttUserPass);
    tamBox[MQTT][SERVER][TOPIC]     = String(cfgMqttTopic);
    tamBox[NODE][OWN][SIGN]         = String(cfgClientSign);
    tamBox[NODE][DEST_A][ID]        = String(cfgDestA);
    tamBox[NODE][DEST_A][SIGN]      = String(cfgDestSignA);
    tamBox[NODE][DEST_A][NAME]      = "-";
    tamBox[NODE][DEST_A][EXIT]      = String(cfgDestExitA);
    tamBox[NODE][DEST_A][TOTTRACKS] = String(cfgTrackNumA);
    tamBox[NODE][DEST_B][ID]        = String(cfgDestB);
    tamBox[NODE][DEST_B][SIGN]      = String(cfgDestSignB);
    tamBox[NODE][DEST_B][NAME]      = "-";
    tamBox[NODE][DEST_B][EXIT]      = String(cfgDestExitB);
    tamBox[NODE][DEST_B][TOTTRACKS] = String(cfgTrackNumB);
    tamBox[NODE][DEST_C][ID]        = String(cfgDestC);
    tamBox[NODE][DEST_C][SIGN]      = String(cfgDestSignC);
    tamBox[NODE][DEST_C][NAME]      = "-";
    tamBox[NODE][DEST_C][EXIT]      = String(cfgDestExitC);
    tamBox[NODE][DEST_C][TOTTRACKS] = String(cfgTrackNumC);
    tamBox[NODE][DEST_D][ID]        = String(cfgDestD);
    tamBox[NODE][DEST_D][SIGN]      = String(cfgDestSignD);
    tamBox[NODE][DEST_D][NAME]      = "-";
    tamBox[NODE][DEST_D][EXIT]      = String(cfgDestExitD);
    tamBox[NODE][DEST_D][TOTTRACKS] = String(cfgTrackNumD);
  }
  ledBrightness                     = atoi(cfgLedBrightness);
  lcdBackLight                      = atoi(cfgBackLight);

  FastLED.setBrightness(ledBrightness);
  lcd.setBacklight(lcdBackLight);

}

// ------------------------------------------------------------------------------------------------------------------------------
//  Set runningPhase String
// ------------------------------------------------------------------------------------------------------------------------------
void setRunningString() {

  String dest[4];
  String dir[4];
  String space[5] = {"     ", "    ", "   ", "  ", " "};

  for (int i=0; i<NUM_OF_DEST; i++) {
    if (tamBox[NODE][i][TOTTRACKS].toInt() == 1) {
      if (traffDir[i] == DIR_RIGHT) {
        dir[i] = DIR_RIGHT_S;
      }

      else {
        dir[i] = DIR_LEFT_S;
      }
    }

    else {
      dir[i] = DIR_DOUBLE_L_S;      
    }
  }

  for (int i=0; i<NUM_OF_DEST; i++) {
    if (nodeIDSuffix[i] == "A" || nodeIDSuffix[i] == "C") {
      if (tamBox[NODE][i][TOTTRACKS].toInt() == 1) {
        dest[i] = nodeIDSuffix[i]+dir[i]+" "+tamBox[NODE][i][SIGN]+space[tamBox[NODE][i][SIGN].length()];
      }

      else {
        dest[i] = nodeIDSuffix[i]+dir[i]+tamBox[NODE][i][SIGN]+space[tamBox[NODE][i][SIGN].length()];
      }
    }

    else {
      if (tamBox[NODE][i][TOTTRACKS].toInt() == 1) {
        dest[i] = space[tamBox[NODE][i][SIGN].length()]+tamBox[NODE][i][SIGN]+" "+dir[i]+nodeIDSuffix[i];
      }

      else {
        dest[i] = space[tamBox[NODE][i][SIGN].length()]+tamBox[NODE][i][SIGN]+dir[i]+nodeIDSuffix[i];
      }
    }
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(dest[DEST_A]+dest[DEST_B]);
  lcd.setCursor(0, 1);
  lcd.print(dest[DEST_C]+dest[DEST_D]);
}

// ------------------------------------------------------------------------------------------------------------------------------
//  Function to validate the input data form
// ------------------------------------------------------------------------------------------------------------------------------
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper) {

  if (debug) {Serial.println(dbText+"IotWebConf validating form");}
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
      if (i == nbrSigTypes -1){msg += ",";}
    }
  }

  if (!valid) {
    webSignal1Type.errorMessage = "Endast dessa är tillåtna signaltyper: Not used, Hsi2, Hsi3, Hsi4, Hsi5";
  }
*/
  return valid;
}
