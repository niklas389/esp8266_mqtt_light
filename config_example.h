// Network
const char* wifi_ssid           =   "<SSID>";
const char* wifi_password       =   "<Password>";
const char* espName             =   "<device-name>";

// MQTT Config
const char* mqtt_server         =   "";
const char* mqtt_user           =   "";
const char* mqtt_password       =   "";
const int mqtt_port             =   1883;

// MQTT Topics
const int pin_led1    	        =   5;
const char* topic_led1          =   "home/bedroom/ceilingLight";
const char* topic_led1_toggle   =   "home/bedroom/ceilingLight/toggle";
const char* topic_led1_state    =   "home/bedroom/ceilingLight/state";
    // >>If PIN2 is set to 0 then client will run in Single LED mode<<
const int pin_led2              =   0;
const char* topic_led2          =   "";
const char* topic_led2_toggle   =   "";
const char* topic_led2_state    =   "";

// Misc
bool restoreAfterReboot         =   true;
String clientVer                =   "1.4";
bool DEBUGGING                  =   true;
const int pwm_range             =   512;

// OTA
bool OTAenabled                 =   true;
const char* ota_password        =   "<Password>";