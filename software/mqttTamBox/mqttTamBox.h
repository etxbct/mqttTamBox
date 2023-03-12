/**
  * Settings for this specific MQTT client
  */
// Access point
#define APNAME                  "TAMBOX-1"    // uniqe client id used as AP name
#define APTYPE                  "TamBox"      // client type for tambox
#define APPASSWORD              "tambox1234"  // initial password for connecting in AP mode, needs to be changed first time logging in
#define ROOTTOPIC               "mqtt_h0"     // typically used on H0 track layout
#define CLIENTSIGN              "CDA"         // station id for the tambox
#define SW_VERSION              "ver 1.1.0"

// -- Configuration specific key. The value should be modified if config structure is changed.
#define CONFIG_VERSION          "ver 0.4"

// Default string and number length
#define STRING_LEN             32
#define HOST_LEN               16
#define NUMBER_LEN              8

// Debug
//#define DEBUG                         // uncomment to use debug mode

// Used pins
//SCL                          D1       // For i2c communication pin 5
//SDA                          D2       // For i2c communication pin 4
#define BUZZER_PIN             D5       // Buzzer signal pin
#define FASTLED_PIN            D6       // Led signal pin

// i2C addresses
#define KEYI2CADDR           0x20       // Keypad address
#define LCDI2CADDR           0x27       // LCD address

// FastLED settings
#define NUM_LED_DRIVERS         4       // Max number of signal RGB LED drivers
#define LED_DRIVE_TYPE     WS2811
#define COLOR_ORDER           RGB
#define ON                    255       // Led on
#define OFF                     0       // Led off
#define LED_BRIGHTNESS        125       // 0-255

// LCD settings
#define LCD_BACKLIGHT         128       // 0-255
#define LCD_MAX_LENGTH         16       // 16 character
#define LCD_MAX_ROWS            2       // 2 rows

//Buzzer settings
#define KEY_CLK               500      // 500 Hz
#define BEEP_OK               800      // 800 Hz
#define BEEP_NOK              300      // 300 Hz
#define BEEP_DURATION        1000      // 1 sekund

// Configuration host
#define CONFIG_HOST             "192.168.1.7"
#define CONFIG_HOST_PORT       80
#define CONFIG_HOST_FILE        "/?id=" 

// MQTT node configuration
#define MQTT_PARAM              5
#define SERVER                  0
#define PORT                    1
#define USER                    2
#define PASS                    3
#define TOPIC                   4

// TamBox node configuration
#define CONFIG_DEST             9
#define DEST_A                  0       // Destination on left side outgoing track
#define DEST_B                  1       // Destination on right side outgoing track
#define DEST_C                  2       // Destination on left side outgoing track
#define DEST_D                  3       // Destination on right side outgoing track 
#define OWN                     4       // Own Module 
#define DEST_A_LEFT             0       // Used when type is split
#define DEST_A_RIGHT            5       // Used when type is split
#define DEST_B_LEFT             1       // Used when type is split
#define DEST_B_RIGHT            6       // Used when type is split
#define DEST_C_LEFT             2       // Used when type is split
#define DEST_C_RIGHT            7       // Used when type is split
#define DEST_D_LEFT             3       // Used when type is split
#define DEST_D_RIGHT            8       // Used when type is split

#define CONFIG_PARAM            8       // for all dest:    ID, SIGN, NAME, TRACKS, EXIT, TOTTRACKS, TYPE
#define ID                      0
#define SIGN                    1
#define NAME                    2
#define NUMOFDEST               3       // for own station
#define TRACKS                  4
#define EXIT                    5
#define TOTTRACKS               6
#define TYPE                    7

#define MQTT_TXT                "mqtt"
#define SERVER_TXT              "server"
#define PORT_TXT                "port"
#define USER_TXT                "user"
#define PASS_TXT                "password"
#define ROOT_TXT                "rootTopic"
#define CONFIG_TXT              "config"
#define ID_TXT                  "id"
#define DESTS_TXT               "destinations"
#define DEST_TXT                "destination"
#define SIGN_TXT                "signature"
#define NAME_TXT                "name"
#define TRACK_TXT               "tracks"
#define TYPE_TXT                "type"
#define EXIT_TXT                "exit"
#define NOT_USED_TXT            "-"
#define TYPE_NONE_TXT           "none"
#define TYPE_SINGLE_TXT         "single"
#define TYPE_SPLIT_TXT          "split"
#define TYPE_LEFT_TXT           "left"
#define TYPE_RIGHT_TXT          "right"
#define TYPE_DOUBLE_TXT         "double"

// Tambox states
#define NUM_OF_STATES           8
#define STATE_NOTUSED           0       // Not used
#define STATE_IDLE              1       // Idle
#define STATE_TRAFDIR           2       // Traffic direction sent
#define STATE_INREQUEST         3       // Incoming request
#define STATE_INTRAIN           4       // Incoming Train
#define STATE_OUTREQUEST        5       // Outgoing request
#define STATE_OUTTRAIN          6       // Outgoing Train
#define STATE_LOST              7       // Lost connection

// Destinations
#define NUM_OF_DEST             4       // Number of Destinations A-D
#define DEST_A_TXT              "A"
#define DEST_B_TXT              "B"
#define DEST_C_TXT              "C"
#define DEST_D_TXT              "D"
#define DEST_CONFIG             4       // Configuration mode selected
#define DEST_CONFIG_TXT         "Config selected"
#define DEST_ALL_DEST           5       // All destinations
#define DEST_ALL_DEST_TXT       "All destinations"
#define DEST_OWN_STATION        6
#define DEST_OWN_STATION_TXT    "Show own station"
#define DEST_NOT_SELECTED       7       // Destination not selected
#define DEST_NOT_SELECTED_TXT   "Not selected"
#define DEST_LEFT               0       // Left track or single track
#define DEST_RIGHT              1       // Right track
#define DEST_ZERO_TRAIN         0

// Incoming MQTT topics
#define NUM_OF_TOPICS           7
#define TOPIC_ROOT              0       // mqtt_h0
#define TOPIC_CLIENT            1       // tambox-1
#define TOPIC_DEST              2       // cda
#define TOPIC_TYPE              3       // traffic
#define TOPIC_TRACK             4       // left/right
#define TOPIC_ITEM              5       // direction/train
#define TOPIC_ORDER             6       // set/reject/accept/request/cancel

// MQTT topics strings
#define NUM_TOPICS_TXT         19
#define TOPIC_NO_TOPIC          0
#define TOPIC_DIVIDER_TXT       "/"
#define TOPIC_HASH_TXT          "#"
#define TOPIC_DID               0
#define TOPIC_DID_TXT           "$id"
#define TOPIC_DSW               1
#define TOPIC_DSW_TXT           "$sw"
#define TOPIC_DNAME             2
#define TOPIC_DNAME_TXT         "$name"
#define TOPIC_DTYPE             3
#define TOPIC_DTYPE_TXT         "$type"
#define TOPIC_DSTATE            4
#define TOPIC_DSTATE_TXT        "$state"
#define TOPIC_LOST              5
#define TOPIC_LOST_TXT          "lost"
#define TOPIC_READY             6
#define TOPIC_READY_TXT         "ready"
#define TOPIC_SET               7
#define TOPIC_SET_TXT           "set"
#define TOPIC_REQUEST           8
#define TOPIC_REQUEST_TXT       "request"
#define TOPIC_ACCEPT            9
#define TOPIC_ACCEPT_TXT        "accept"
#define TOPIC_CANCELED         10
#define TOPIC_CANCELED_TXT      "cancel"
#define TOPIC_REJECT           11
#define TOPIC_REJECT_TXT        "reject"
#define TOPIC_IN               12
#define TOPIC_IN_TXT            "in"
#define TOPIC_OUT              13
#define TOPIC_OUT_TXT           "out"
#define TOPIC_TRAIN            14
#define TOPIC_TRAIN_TXT         "train"
#define TOPIC_TRAFFIC          15
#define TOPIC_TRAFFIC_TXT       "traffic"
#define TOPIC_DIR              16
#define TOPIC_DIR_TXT           "direction"
#define TOPIC_LEFT             17
#define TOPIC_LEFT_TXT          "left"
#define TOPIC_RIGHT            18
#define TOPIC_RIGHT_TXT         "right"

// Directions
#define NUM_DIR_STATES          2       // Number of Direction states
#define DIR_OUT                 0       // Single track direction out
#define DIR_IN                  1       // Single track direction in
#define DIR_LOST               15       // Connection lost
#define SINGLE_TRACK            1
#define DOUBLE_TRACK            2
#define MAX_NUM_OF_TRACKS       2       // Max number of tracks
#define TRAFFIC_LEFT            0
#define TRAFFIC_RIGHT           1
#define DIR_IDLE_TXT            " "
#define DIR_RIGHT_TXT           ">"
#define DIR_LEFT_TXT            "<"
#define DIR_QUERY_TXT           "?"
#define DIR_LOST_TXT            "-"

// Text in LCD display
#define LCD_TOOGLE_TRACK     2000       // Toogle track between left and right on duoble track every 2 sec
#define LCD_SHOW_TEXT        3000       // Show status text for 3 sec
#define LCD_TRAIN               0
#define LCD_TRAIN_TXT           "Avgång tåg#"
#define LCD_TRAINDIR_NOK        1
#define LCD_TRAINDIR_NOK_TXT    "Tågriktn. nekad!"
#define LCD_DEPATURE            2
#define LCD_DEPATURE_TXT        "Avgång?"
#define LCD_TAM_CANCEL          3
#define LCD_TAM_CANCEL_TXT      "Ångra TAM?"
#define LCD_TAM_ACCEPT          4
#define LCD_TAM_ACCEPT_TXT      "Acceptera TAM?"
#define LCD_ARRIVAL             5
#define LCD_ARRIVAL_TXT         "Ankomst?"
#define LCD_TAM_NOK             6
#define LCD_TAM_NOK_TXT         "TAM nekad!"
#define LCD_TAM_CANCELED        7
#define LCD_TAM_CANCELED_TXT    "TAM ångrad!"
#define LCD_TAM_OK              8
#define LCD_TAM_OK_TXT          "TAM accepterad!"
#define LCD_ARRIVAL_OK          9
#define LCD_ARRIVAL_OK_TXT      "Tåg Ankommit!"
#define LCD_TRAIN_ID           10
#define LCD_AP_MODE             0
#define LCD_STARTING_UP         1
#define LCD_START_ERROR         2
#define LCD_REBOOTING           3
#define LCD_WIFI_CONNECTING     1
#define LCD_WIFI_CONNECTED      2
#define LCD_SIGNAL              3
#define LCD_LOADING_CONF        4
#define LCD_LOADING_CONF_OK     5
#define LCD_STARTING_MQTT       6
#define LCD_BROKER_CONNECTED    7
#define LCD_BROKER_NOT_FOUND    0
#define LCD_WIFI_NOT_FOUND      1
#define LCD_LOADING_CONF_NOK    2
#define LCD_FIRST_COL           0
#define LCD_FIRST_ROW           0
#define LCD_SECOND_ROW          1
#define LCD_THIRD_ROW           2
#define LCD_FOURTH_ROW          3
#define LCD_DEST_LEN            1
#define LCD_DIR_LEN             1
#define LCD_NODE_LEN            6
#define LCD_DEST_TXT            0
#define LCD_DIR_TXT             1
#define LCD_NODE_TXT            2

// Defince which pin to use for LED output
//#define LED_PIN LED_BUILTIN

// Configuration pin
// When CONFIG_PIN is pulled to ground on startup, the client will use the initial
// password to build an AP. (E.g. in case of lost password)
#define CONFIG_PIN D0

// Status indicator pin
// First it will light up (kept LOW), on Wifi connection it will blink
// and when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN
