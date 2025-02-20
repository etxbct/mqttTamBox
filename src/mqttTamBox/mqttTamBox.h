/**
  * Settings for this specific MQTT client that needs to be set
  */
// Used pins for I2C, Buzzer and LEDs
//SCL                                                D1       // Always used for i2c communication pin 5
//SDA                                                D2       // Always used for i2c communication pin 4
#define BUZZER_PIN                                   D5       // Buzzer signal pin

// i2C addresses
#define KEY_I2C_ADDR                               0x20       // Keypad address
#define LCD_I2C_ADDR                               0x27       // Default LCD address

// Non EU ASCII character set used by the LCD display
#define NON_EU_CHAR_SET                                       // comment if the LCD uses European ASCII character set

// Debug settings
// Set Arduino IDE Debug port set to Serial to activate debug, default set to Disabled
#ifdef DEBUG_ESP_PORT
  #define DEBUG                                               // Normal debug mode
//#define DEBUG_ALL                                           // Extended debug mode
#endif

// When CONFIG_PIN is pulled to ground on startup, the client will use the initial
// password to build an AP. (E.g. in case of lost password)
#define CONFIG_PIN                                   D0       // Configuration pin

// Set Wifi timeout in ms
#define WIFI_TIMEOUT                              15000       // Default 30000 ms

//-----------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * Settings that normally don't need to be changed
 */
// SW type and version
#define SW_TYPE                             "mqttTamBox"      // Name of the software
#define SW_VERSION                          "ver 2.0.12"      // Version of the software

// -- Configuration specific key. The value should be modified if config structure is changed.
#define CONFIG_VERSION                         "ver 1.4"

// First it will light up (kept LOW), on Wifi connection it will blink
// and when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN                          LED_BUILTIN       // Status indicator pin

// Timers
#define TIME_BEEP_DURATION                         1000       // Default time for a beep, 1 second
#define TIME_BEEP_PAUS                             2000       // Default time for a beep paus, 2 seconds
#define TIME_PING_INTERVAL                        10000       // 10 seconds ping interval
#define TIME_TOGGLE_TRACK                          2000       // Toggle track between left and right on duoble track every 2 sec

// FastLED settings
#define NUM_LED_DRIVERS                               4       // Max number of signal RGB LED drivers
//#define LED_DRIVE_TYPE                           WS2811
//#define COLOR_ORDER                                 RGB
#define ON                                          255       // Led on
#define OFF                                           0       // Led off
#define LED_BRIGHTNESS                              125       // Default LED brightness

// LCD settings
enum {LCD_FIRST_ROW, LCD_SECOND_ROW, LCD_THIRD_ROW, LCD_FOURTH_ROW};
#define LCD_PARTS                                     3
enum {LCD_DEST, LCD_DIR, LCD_NODE};
#define LCD_FIRST_COL                                 0       // Start position
#define LCD_DIR_LEN                                   1       // Direction symbol length
#define LCD_DEST_LEN                                  1       // Destination character length
#define LCD_BACKLIGHT                               128       // Default LCD backlight

// Buzzer settings
#define BEEP_KEY_CLK                                500       // 500 Hz
#define BEEP_OK                                     800       // 800 Hz
#define BEEP_NOK                                    300       // 300 Hz

// Configuration host
#define DB_CONFIG_HOST        "http://mqtt-broker.local"      // Host name to the configuration server
#define DB_CONFIG_FILE                           "/?id="      // Query string 
#define DB_CONFIGFILE_LEN                            10
#define DB_CONFIGPATH_LEN                            40       // DB_HOST_LEN+DB_CONFIGFILE_LEN
#define DB_HOST_LEN                                  30
#define DB_CLIENTID_LEN                              30       // Same length as in config server database
#define DB_NUMBER_LEN                                 8
#define DB_DEST_LEN                                   1       // Same length as in mySQL database
#define DB_TOPIC_LEN                                 10       // Same length as in mySQL database
#define DB_USER_NAME                                 10       // Same length as in mySQL database
#define DB_USER_PASS                                 10       // Same length as in mySQL database
#define DB_CLIENT_LEN                                50       // Same length as in mySQL database
#define DB_STNNAME_LEN                               50       // Same length as in mySQL database
#define DB_SIGN_LEN                                   5       // Same length as in mySQL database

// MQTT node configuration (tamBoxMqtt)
// String tamBoxMqtt[MQTT_PARAM]
#define MQTT_PARAM                                    5       // Size of var tamBoxMqtt
enum {SERVER, PORT, USER, PASS, SCALE};

// TamBox node configuration (tamBoxConfig)
// String tamBoxConfig[CONFIG_DEST][CONFIG_PARAM]
#define CONFIG_DEST                                   9       // Size of var tamBoxConfig
#define CONFIG_PARAM                                  8       // for all dest:    ID, SIGN, NAME, TRACKS, EXIT, TOTTRACKS, TYPE
enum {
  ID,                                                         // max size 20 characters
  SIGN,                                                       // max size 4 characters
  NAME,                                                       // for own station, max size 30 characters
  NUMOFDEST,                                                  // for own station
  TRACKS,                                                     // integer
  EXIT,                                                       // max size 1 character
  TOTTRACKS,                                                  // integer
  TYPE                                                        // max size 6 characters
};

// Destinations
// const char* destIDTxt[NUM_OF_DEST]
#define DEST_BUTTONS                                  4       // Number of Destination buttons A-D
#define NUM_OF_DEST_STRINGS                          12       // Number of Destination strings
enum {
  DEST_A,                                                     // Destination on left side outgoing track
  DEST_B,                                                     // Destination on right side outgoing track
  DEST_C,                                                     // Destination on left side outgoing track
  DEST_D,                                                     // Destination on right side outgoing track
  OWN,                                                        // Own Module
  DEST_A_RIGHT,                                               // Used when type is split on left side outgoing track
  DEST_B_RIGHT,                                               // Used when type is split on right side outgoing track
  DEST_C_RIGHT,                                               // Used when type is split on left side outgoing track
  DEST_D_RIGHT,                                               // Used when type is split on right side outgoing track
  DEST_CONFIG,                                                // Configuration mode selected
  DEST_ALL_DEST,                                              // All destinations
  DEST_NOT_SELECTED                                           // Destination not selected
};
#define DEST_A_T                                     "A"      // 0
#define DEST_B_T                                     "B"      // 1
#define DEST_C_T                                     "C"      // 2
#define DEST_D_T                                     "D"      // 3
#define DEST_OWN_STATION_T            "Show own station"      // 4
#define DEST_A_RIGHT_T                              "Ar"      // 5
#define DEST_B_RIGHT_T                              "Br"      // 6
#define DEST_C_RIGHT_T                              "Cr"      // 7
#define DEST_D_RIGHT_T                              "Dr"      // 8
#define DEST_CONFIG_T                  "Config selected"      // 9
#define DEST_ALL_DEST_T               "All destinations"      // 10
#define DEST_NOT_SELECTED_T               "Not selected"      // 11

// Recevied config in JSON file
#define MQTT_T                                    "mqtt"      // Used in received config JSON file
#define SERVER_T                                "server"      // Used in received config JSON file
#define PORT_T                                    "port"      // Used in received config JSON file
#define USER_T                                     "usr"      // Used in received config JSON file
#define PASS_T                                     "pwd"      // Used in received config JSON file
#define SCALE_T                                  "scale"      // Used in received config JSON file
#define EPOCH_T                                  "epoch"      // Used in received config JSON file
#define CONFIG_T                                "config"      // Used in received config JSON file
#define ID_T                                        "id"      // Used in received config JSON file
#define DESTS_T                           "destinations"      // Used in received config JSON file
#define DEST_T                             "destination"      // Used in received config JSON file
#define SIGN_T                               "signature"      // Used in received config JSON file
#define NAME_T                                    "name"      // Used in received config JSON file
#define TRACK_T                                 "tracks"      // Used in received config JSON file
#define EXIT_T                                    "exit"      // Used in received config JSON file
#define TYPE_T                                    "type"      // Used in received config JSON file
#define NOT_USED_T                                   "-"      // Used in received config JSON file
#define TYPE_NONE_T                               "none"      // Used in received config JSON file
#define TYPE_SINGLE_T                           "single"      // Used in received config JSON file
#define TYPE_SPLIT_T                             "split"      // Used in received config JSON file
#define TYPE_LEFT_T                               "left"      // Used in received config JSON file
#define TYPE_RIGHT_T                             "right"      // Used in received config JSON file
#define TYPE_DOUBLE_T                           "double"      // Used in received config JSON file

// Tambox states
// const char* trackStateTxt[NUM_OF_STATES]
#define NUM_OF_STATES                                10       // Size of var trackStateTxt
enum {_NOTUSED, _IDLE, _TRAFDIR, _INREQUEST, _INACCEPT, _INTRAIN, _OUTREQUEST, _OUTACCEPT, _OUTTRAIN, _LOST};
// State strings for debug
#define _NOTUSED_T                            "_NOTUSED"
#define _IDLE_T                                  "_IDLE"
#define _TRAFDIR_T                            "_TRAFDIR"
#define _INREQUEST_T                        "_INREQUEST"
#define _INACCEPT_T                          "_INACCEPT"
#define _INTRAIN_T                            "_INTRAIN"
#define _OUTREQUEST_T                      "_OUTREQUEST"
#define _OUTACCEPT_T                        "_OUTACCEPT"
#define _OUTTRAIN_T                          "_OUTTRAIN"
#define _LOST_T                                  "_LOST"

enum {LEFT_TRACK, RIGHT_TRACK};
#define DEST_TRAIN_0                                  0

// Incoming MQTT topics                                       // cmd/h0/node/tambox-1/inventory/req
                                                              // cmd/h0/tam/tambox-1/a/req
                                                              // cmd/h0/tam/tambox-1/a/res
                                                              // dt/h0/ping/tambox-1
#define NUM_OF_TOPICS                                 6
enum {
  TOPIC_MSGTYPE,                                              // cmd,dt
  TOPIC_SCALE,                                                // h0
  TOPIC_TYPE,                                                 // tam,node,tower,ping
  TOPIC_NODE_ID,                                              // node id
  TOPIC_PORT_ID,                                              // port id
  TOPIC_ORDER                                                 // req,res
};

// Codes used when handling incoming MQTT messages
enum {CODE_LOST, CODE_READY, CODE_TRAFDIR_REQ_IN, CODE_TRAFDIR_RES_IN, CODE_TRAFDIR_RES_OUT, CODE_TRAIN_IN, CODE_TRAIN_OUT, CODE_ACCEPT, CODE_ACCEPTED, CODE_REJECTED, CODE_CANCEL, CODE_CANCELED};

// Incoming dt queue
// uint16_t dtInQueue[DEST_BUTTONS][Q_DATA]
enum {Q_INACTIVE, Q_ACTIVE};
#define Q_DATA                                        4       // Number of queue data
enum {Q_STATE, Q_TRACK, Q_ORDERCODE, Q_TRAIN};

// MQTT Topics strings
enum {_REQUEST, _RESPONSE, _DATA};
#define COMMAND                                    "cmd"      // Message type
#define DATA                                        "dt"      // Message type
#define TAM                                        "tam"      // Body type
#define NODE                                      "node"      // Body type
#define TOWER                                    "tower"      // Body type
#define PING                                      "ping"      // Body type
#define INVENTORY                            "inventory"      // Port id for inventory requests
#define REQUEST                                    "req"      // Message subtype used in cmd
#define RESPONSE                                   "res"      // Message subtype used in cmd

// MQTT Body strings
// const char* useTrackTxt[DIR_STATES]
// const char* trainDir[DIR_STATES]
#define VERSION                                "version"
#define TIMESTAMP                            "timestamp"
#define SESSION_ID                          "session-id"
#define RESPOND_TO                          "respond-to"
#define TRACK                                    "track"
#define NODE_ID                                "node-id"
#define PORT_ID                                "port-id"
#define TRAIN_ID                              "identity"
#define STATE                                    "state"
#define DESIRED                                "desired"
#define ACCEPT                                  "accept"
#define CANCEL                                  "cancel"
#define REJECT                                  "reject"
#define REPORTED                              "reported"
#define ACCEPTED                              "accepted"
#define CANCELED                              "canceled"
#define REJECTED                              "rejected"
#define LOST                                      "lost"
#define READY                                    "ready"
#define IN                                          "in"
#define OUT                                        "out"
#define LEFT                                      "left"
#define RIGHT                                    "right"
#define METADATA                              "metadata"
#define M_ID                                        "id"      // Used in metadata
#define M_TYPE                                    "type"      // Used in metadata
#define M_VER                                      "ver"      // Used in metadata
#define M_NAME                                    "name"      // Used in metadata
#define M_SIGN                                    "sign"      // Used in metadata
#define M_RSSI                                    "rssi"      // Used in metadata

// mqtt-lcp support
#define LCP_BODY_VER                               "1.0"
#define LCP_BODY_REBOOT                         "reboot"
#define LCP_BODY_SHUTDOWN                     "shutdown"
#define NODE_SUPERVISOR                     "supervisor"
enum {LCP_SESSION_ID, LCP_RESPOND_TO, LCP_DESIRED_STATE, LCP_NODE_ID, LCP_PORT_ID, LCP_TRACK};

// Directions
// uint8_t traffDir[DEST_BUTTONS][MAX_NUM_OF_TRACKS]
// uint8_t lastTraffDir[DEST_BUTTONS][MAX_NUM_OF_TRACKS]
#define DIR_STATES                                    2       // Number of Direction states, OUT/IN
enum {DIR_OUT, DIR_IN};
#define DIR_LOST                                     15       // Connection lost
#define SINGLE_TRACK                                  1
#define DOUBLE_TRACK                                  2
#define MAX_NUM_OF_TRACKS                             2       // Max number of tracks
enum {TRAFFIC_LEFT, TRAFFIC_RIGHT};

// Direction symbols                                             Max one character
#define DIR_IDLE_T                                   " "      // Idle symbol
#define DIR_RIGHT_T                                  ">"      // Traffic direction symbol,                          >
#define DIR_LEFT_T                                   "<"      // Traffic direction symbol,                          <
#define DIR_RIGHT_TRAIN_T                         "\xab"      // Train direction symbol when moving left to right,  >>
#define DIR_LEFT_TRAIN_T                          "\xbb"      // Train direction symbol when moving right to left,  <<
#define DIR_QUERY_T                                  "?"      // TAM ongoing symbol,                                ?
#define DIR_LOST_T                                   "-"      // Lost connection symbol,                            -

// Special characters max 8 for LCD HD44780
enum {
  TRAIN_MOVING_RIGHT,                                         // Train direction symbol when moving left to right
  TRAIN_MOVING_LEFT,                                          // Train direction symbol when moving right to left
  SWE_CAP_Å,                                                  // uppercase Å
  SWE_CAP_Ä,                                                  // uppercase Ä
  SWE_CAP_Ö,                                                  // uppercase Ö
  SWE_LOW_Å,                                                  // lowercase å
  SWE_LOW_Ä,                                                  // lowercase ä
  SWE_LOW_Ö                                                   // lowercase ö
};

// LCD display strings
// String positions
// const char* stringTxt[languages][LCD_STRINGS]
enum {LCD_TRAIN, LCD_TRAINDIR_NOK, LCD_DEPATURE, LCD_TAM_CANCEL, LCD_TAM_ACCEPT, LCD_ARRIVAL, LCD_TAM_NOK, LCD_TAM_CANCELED, LCD_TAM_OK, LCD_ARRIVAL_OK, LCD_DEPATURE_OK, LCD_TRAIN_ID};
// Strings
#define LCD_STRINGS                                  11       // Number of strings
// English
#define LCD_TRAIN_TE                       "Dep. train#"      // Max length 11 characters
#define LCD_TRAINDIR_NOK_TE           "Other stn. busy!"      // Max length 16 characters
#define LCD_DEPATURE_TE                     "Departure?"      // Max length 16 characters
#define LCD_TAM_CANCEL_TE                    "Undo TAM?"      // Max length 16 characters
#define LCD_TAM_ACCEPT_TE                  "Accept TAM?"      // Max length 16 characters
#define LCD_ARRIVAL_TE                        "Arrival?"      // Max length 16 characters
#define LCD_TAM_NOK_TE                     "TAM denied!"      // Max length 16 characters
#define LCD_TAM_CANCELED_TE                "TAM undone!"      // Max length 16 characters
#define LCD_TAM_OK_TE                    "TAM accepted!"      // Max length 16 characters
#define LCD_ARRIVAL_OK_TE                    "Train In!"      // Max length 16 characters
#define LCD_DEPATURE_OK_TE                  "Train Out!"      // Max length 16 characters
// Swedish
#define LCD_TRAIN_T                        "Avgång tåg#"      // Max length 16 characters
#define LCD_TRAINDIR_NOK_T            "Tågriktn. nekad!"      // Max length 16 characters
#define LCD_DEPATURE_T                         "Avgång?"      // Max length 16 characters
#define LCD_TAM_CANCEL_T                    "Ångra TAM?"      // Max length 16 characters
#define LCD_TAM_ACCEPT_T                "Acceptera TAM?"      // Max length 16 characters
#define LCD_ARRIVAL_T                         "Ankomst?"      // Max length 16 characters
#define LCD_TAM_NOK_T                       "TAM nekad!"      // Max length 16 characters
#define LCD_TAM_CANCELED_T                 "TAM ångrad!"      // Max length 16 characters
#define LCD_TAM_OK_T                   "TAM accepterad!"      // Max length 16 characters
#define LCD_ARRIVAL_OK_T                       "Tåg In!"      // Max length 16 characters
#define LCD_DEPATURE_OK_T                      "Tåg Ut!"      // Max length 16 characters

// Start phase strings
// English only
#define LCD_AP_MODE                            "AP mode"      // Max length 16 characters
#define LCD_STARTING_UP                 "Starting up..."      // Max length 16 characters
#define LCD_START_ERROR                          "Error"      // Max length 16 characters
#define LCD_REBOOTING                     "Rebooting..."      // Max length 16 characters
#define LCD_SHUTTINGDOWN                 "Shutting down"      // Max length 16 characters
#define LCD_WIFI_CONNECTING            "Connecting WiFi"      // Max length 16 characters
#define LCD_WIFI_CONNECTED              "WiFi connected"      // Max length 16 characters
#define LCD_SIGNAL                             "Signal "      // Max length 16 characters
#define LCD_LOADING_CONF               "Loading conf..."      // Max length 16 characters
#define LCD_LOADING_CONF_OK              "Config loaded"      // Max length 16 characters
#define LCD_STARTING_MQTT             "Starting MQTT..."      // Max length 16 characters
#define LCD_BROKER_CONNECTED          "Broker connected"      // Max length 16 characters
#define LCD_BROKER_NOT_FOUND          "Broker not found"      // Max length 16 characters
#define LCD_WIFI_NOT_FOUND              "WiFi not found"      // Max length 16 characters
#define LCD_LOADING_CONF_NOK          "Config not found"      // Max length 16 characters

// Structs
//struct tamBoxConfiguration {                                    // Values from received JSON configuration
//  char id[50];                                                  // ID
//  char sign[5];                                                 // SIGN
//  char name[50];                                                // NAME
//  char type[6];                                                 // TYPE
//  char exit[1];                                                 // EXIT
//  int numOfDest;                                                // NUMOFDEST
//  int totTracks;                                                // TOTTRACKS
//  int tracks;                                                   // TRACKS
//};
