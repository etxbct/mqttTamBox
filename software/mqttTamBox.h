/**
  * Settings for this specific MQTT client
  */
// Access point
#define APNAME                  "TAMBOX-1"
#define APPASSWORD              "tambox1234"
#define ROOTTOPIC               "mqtt_n"
#define CLIENTSIGN              "CDA"
// -- Configuration specific key. The value should be modified if config structure is changed.
#define CONFIG_VERSION          "ver 0.4"
// Default string and number length
#define STRING_LEN             32
#define NUMBER_LEN              8


// Debug
#define SET_DEBUG               true

// Configuration host
#define CONFIG_NODES            2       // Two nodes: MQTT, NODE
#define CONFIG_HOST             "192.168.1.7"
#define CONFIG_HOST_PORT        80
#define CONFIG_HOST_FILE        "/?id=" 

// MQTT node
#define MQTT                    0
#define SERVER                  0
#define IP                      0
#define PORT                    1
#define USER                    2
#define PASS                    3
#define TOPIC                   4

// TamBox node
#define NODE                    1
#define CONFIG_PARAM            7       // for all dest:    ID, SIGN, NAME, TRACKS, EXIT, TOTTRACKS, TYPE
#define NUMOFDEST               3       // for own station: ID, SIGN, NAME, NUMOFDEST
#define ID                      0
#define SIGN                    1
#define NAME                    2
#define TRACKS                  3
#define EXIT                    4
#define TOTTRACKS               5
#define TYPE                    6

// FastLED settings
#define NUM_LED_DRIVERS         4       // Max number of signal LED drivers
#define SIG_DATA_PIN           D6       // Led signal pin 12
#define ON                    255       // Led on
#define OFF                     0       // Led off
#define LED_BRIGHTNESS        125

//i2C addresses
#define KEYI2CADDR              0x20    // Keypad address
#define LCDI2CADDR              0x27    // LCD address

// Destinations
#define NUM_OF_DEST             4       // Number of Destinations A-D
#define DEST_A                  0       // Destination on left side outgoing track
#define DEST_A_ID              "a"
#define DEST_B                  1       // Destination on right side outgoing track
#define DEST_B_ID              "b"
#define DEST_C                  2       // Destination on left side outgoing track
#define DEST_C_ID              "c"
#define DEST_D                  3       // Destination on right side outgoing track 
#define DEST_D_ID              "d"
#define OWN                     4       // Own Module 
#define NOT_SELECTED          255       // Destination not selected
#define LEFT_TRACK              0
#define RIGHT_TRACK             1

// Tambox states
#define NUM_OF_STATES           6
#define STATE_NOTUSED           0       // Not used
#define STATE_IDLE              1       // Idle
#define STATE_INREQUEST         2       // Incoming request
#define STATE_INTRAIN           3       // Incoming request approved
#define STATE_OUREQUEST         4       // Outgoing request
#define STATE_OUTTRAIN          5       // Outgoing request approved

// MQTT topics
#define TOPIC_STATE             "state"
#define TOPIC_REQUEST           "request"
#define TOPIC_ACCEPT            "accept"
#define TOPIC_REJECT            "reject"
#define TOPIC_ARRIVED           "arrived"
#define TOPIC_TRAIN             "train"
#define TOPIC_TRAFFIC           "traffic"
#define TOPIC_DIR               "direction"
#define TOPIC_DIR_RIGHT         "up"    // Direction A-B (to the right)
#define TOPIC_DIR_LEFT          "down"  // Direction B-A (to the left)

// Incoming MQTT split topics
#define TOPIC_TOP               0
#define TOPIC_NODE              1
#define TOPIC_EXIT              2
#define TOPIC_TYPE              3
#define TOPIC_TRACK             4
#define TOPIC_ID                5
#define TOPIC_ORDER             6

// Directions
#define NUM_DIR_STATES          2       // Number of Direction states
#define DIR_RIGHT               0       // Single track direction right
#define DIR_RIGHT_S             ">"
#define DIR_LEFT                1       // Single track direction left
#define DIR_LEFT_S              "<"
#define DIR_DOUBLE_R            2       // Double track direction other track
#define DIR_DOUBLE_R_S          "<>"
#define DIR_DOUBLE_L            3       // Double track direction normal track
#define DIR_DOUBLE_L_S          "><"

// Text in display
#define LCD_BACKLIGHT         255
#define TRAIN                   0
#define CANCEL_OK               1
#define NOK_OK                  2
#define AP_MODE                 0
#define STARTING_UP             1
#define ERROR                   2
#define REBOOTING               3
#define LOADING_CONF            1
#define STARTING_MQTT           2
#define BROKER_CONNECTED        3
#define BROKER_NOT_FOUND        4

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
