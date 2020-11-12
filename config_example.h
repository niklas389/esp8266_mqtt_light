// Network
const char* wifi_ssid               =   "<SSID>";
const char* wifi_password           =   "<Password>";
const char* espName                 =   "<device-name>";

// MQTT Config
const char* mqtt_server             =   "";
const char* mqtt_user               =   "";
const char* mqtt_password           =   "";
const int mqtt_port                 =   1883;

// MQTT v2
const char* topic_available         =   "";

const int pin_led1    	            =   5;
const char* topic_led1_state        =   "";
const char* topic_led1_switch       =   "";
const char* topic_led1_level        =   "";
const char* topic_led1_levelCMD     =   "";

//DEFAULT FOR 'PIN 2' is GPIO 4
const int pin_led2    	            =   0;
const char* topic_led2_state        =   "";
const char* topic_led2_switch       =   "";
const char* topic_led2_level        =   "";
const char* topic_led2_levelCMD     =   "";

// Misc
bool restoreAfterReboot             =   false;
bool DEBUGGING                      =   true;
const int pwm_range                 =   255