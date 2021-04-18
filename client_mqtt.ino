#include <ESP_EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

// Import settings
#include "config.h"
// #include "config_tv.h"
// #include "config_kitchen.h"

const String sw_version = "2021.16.5";

ESP8266WiFiMulti WiFiMulti;
WiFiClient wifiClient;
PubSubClient client(wifiClient);

bool single_light = true;

bool light_P1_on = true;
int light_P1_target = 0;
int light_P1_state = 0;
int light_P1_interval = 2;

bool light_P2_on = true;
int light_P2_target = 0;
int light_P2_state = 0;
int light_P2_interval = 2;

bool fade_led1 = false;
bool fade_led2 = false;

bool initial_fadingDone = false;

// SETUP
void setup() {
    //Start serial if debugging is enabled in config
    if (DEBUGGING == true) {Serial.begin(115200);}

    //Set PWM to 0 to avoid MAX brightness after power loss.
    analogWrite(pin_led1, 0);
    //Single lightMode if pin2 not set
    if (pin_led2 != 0) {
        single_light = false;
        analogWrite(pin_led2, 0);
    }

    // Set PWM Freq
    analogWriteFreq(1000);
    // Set PWM range to XXX (DEF: 1023)
    analogWriteRange(255);
    
    splashScreen();

    EEPROM.begin(16);
    if (restore_after_PL) {
        Serial.println(">> reboot - restoring last state...");
        int tmp_val1;
        int tmp_val2;

        // Get last state (ON/OFF)
        EEPROM.get(0, tmp_val1);
        if (tmp_val1 == 0 || tmp_val1 == 1) {
            light_P1_on = tmp_val1;
        } else {
            EEPROM.put(0, 0);
        }
        // Get last set brightness value and reset if out of range.
        EEPROM.get(4, tmp_val2);
        if (tmp_val2 > 0 && tmp_val2 < 101) {
            light_P1_target = tmp_val2;
        } else {
            EEPROM.put(4, 0);
        }
        fade_led1 = true;

        tmp_val1 = 0;
        tmp_val2 = 0;

        if (single_light == false) {
            // Get last state (ON/OFF)
            EEPROM.get(8, tmp_val1);
            if (tmp_val1 == 0 || tmp_val1 == 1) {
                light_P2_on = tmp_val1;
            } else {
                EEPROM.put(8, 0);
            }
            // Get last set brightness value and reset if out of range
            EEPROM.get(12, tmp_val2);
            if (tmp_val2 > 0 && tmp_val2 < 101) {
                light_P2_target = tmp_val2;
            } else {
                EEPROM.put(12, 0);
            }
            fade_led2 = true;
        }

        EEPROM.commit();
    } else {
        initial_fadingDone = true;
    }
}

unsigned long current_millis = 0;
long light1_prevMillis = 0;
long light2_prevMillis = 0;

void loop() {
    // Start WiFi & MQTT after restoring last state finished
    if (initial_fadingDone) {
        if (!WiFi.isConnected()) {
            startWiFi();
            runMQTT();
        } else if (!client.connected()) {
            runMQTT();
        }
    }

    client.loop();

    //Fade without blocking (delay)
    current_millis = millis();
    if (fade_led1 == true && (current_millis - light1_prevMillis > light_P1_interval)) {vFade_LED1(); light1_prevMillis = current_millis;}
    if (fade_led2 == true && (current_millis - light2_prevMillis > light_P2_interval)) {vFade_LED2(); light2_prevMillis = current_millis;}
}

bool fadeSpeed1 = false;
void vFade_LED1() {
    if (fadeSpeed1 == false) {
        int diff = light_P1_state > light_P1_target ? light_P1_state - light_P1_target : light_P1_target - light_P1_state;
        Serial.println(diff);
        if (diff < 35)  {light_P1_interval = 8; Serial.println("L1 - Slow");} else {light_P1_interval = 2; Serial.println("L1 - Fast");}
        fadeSpeed1 = true;
    }

    if (light_P1_target < light_P1_state) {
        light_P1_state--;
    } else {
        light_P1_state++;
    }    
    analogWrite(pin_led1, light_P1_state);

    if (light_P1_state == light_P1_target) {
        fade_led1 = false;
        Serial.println("Light 1 fading done!");
        Serial.print("Target: ");
        Serial.print(light_P1_target);
        Serial.print(" | New State: ");
        Serial.println(light_P1_state);

        if (light_P1_on == true && light_P1_state > 0) {
            // SAVE STATE & BRIGHTNESS
            EEPROM.put(0, 1);
            EEPROM.put(4, light_P1_state);
            //Publish new brightness and state
            client.publish(topic_led1_state, "ON");
            client.publish(topic_led1_level, String(light_P1_state).c_str());
        } else {
            // SAVE STATE ONLY
            EEPROM.put(0, 0);
            client.publish(topic_led1_state, "OFF");
        }
            EEPROM.commit();

        initial_fadingDone = true;
        light_P1_target = -1;
        fadeSpeed1 = false;
    }
}

bool fadeSpeed2 = false;
void vFade_LED2() {
    if (fadeSpeed2 == false) {
        int diff = light_P2_state > light_P2_target ? light_P2_state - light_P2_target : light_P2_target - light_P2_state;
        Serial.println(diff);
        if (diff < 35)  {light_P2_interval = 8; Serial.println("L2 - Slow");} else {light_P2_interval = 2; Serial.println("L2 - Fast");}
        fadeSpeed2 = true;
    }

    if (light_P2_target < light_P2_state) {
        light_P2_state--;
    } else {
        light_P2_state++;
    }
    analogWrite(pin_led2, light_P2_state);

    if (light_P2_state == light_P2_target) {
        fade_led2 = false;
        Serial.println("Light 2 fading done!");
        Serial.print("Target: ");
        Serial.print(light_P2_target);
        Serial.print(" | New State: ");
        Serial.println(light_P2_state);

        if (light_P2_on == true && light_P2_state > 0) {
            // SAVE STATE & BRIGHTNESS
            EEPROM.put(8, 1);
            EEPROM.put(12, light_P2_state);
            //Publish new brightness and state
            client.publish(topic_led2_state, "ON");
            client.publish(topic_led2_level, String(light_P2_state).c_str());
        } else {
            // SAVE STATE ONLY
            EEPROM.put(8, 0);
            client.publish(topic_led2_state, "OFF");
        }
            EEPROM.commit();

        light_P2_target = -1;
        fadeSpeed2 = false;
    }
}

// Establish WiFi-Connection
void startWiFi() {
    Serial.println("");
    Serial.println("-------");
    WiFi.mode(WIFI_STA);
    Serial.println("(re)conneting wifi with following credentials:");
    Serial.print("SSID: ");
    Serial.println(wifi_ssid);
    Serial.print("Key: ");
    Serial.println(wifi_password);

    WiFi.hostname(espName);
    WiFiMulti.addAP(wifi_ssid, wifi_password);

    int tryCnt = 0;
    Serial.print("Connecting...");
    while (WiFiMulti.run() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
        tryCnt++;

        if (tryCnt > 25) {
            Serial.println("");
            Serial.println("(!) Couldn't connect to WiFi");
            delay(3000);
            ESP.restart();
        }
    }

    Serial.print("Connected! - IP adress: ");
    Serial.println(WiFi.localIP());
}

// Establish MQTT Connection
void runMQTT() {
    Serial.println("");
    Serial.println("-------");
    Serial.println("Starting MQTT-Client with following credentials.");
    Serial.print("Host: ");
    Serial.println(mqtt_server);
    Serial.print("Port: ");
    Serial.println(mqtt_port);
    Serial.print("User: ");
    Serial.println(mqtt_user);
    Serial.print("Password: ");
    Serial.println(mqtt_password);
    Serial.print("ClientID: ");
    Serial.println(espName);

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    while (!client.connected()) {
        Serial.println("Attempting connection...");

        if (client.connect(espName, mqtt_user, mqtt_password)) {
            Serial.println(">> Success - we're connected!");
            // Serial.println((client.state());
        } else {
            Serial.println("(!) Failed - (Couldn't connect to MQTT-Server)");
            Serial.println(" (Retrying in 30 seconds)");
            delay(30000);
        }

        client.subscribe(topic_led1_switch);
        client.subscribe(topic_led1_levelCMD);

        //If PIN2 is set to 0 then client will run in Single LED mode
        if (single_light == false) {
            client.subscribe(topic_led2_switch);
            client.subscribe(topic_led2_levelCMD);
        }
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.println();
    Serial.print("Message arrived Topic: [");
    String newTopic = topic;
    Serial.print(topic);
    Serial.print("] | Payload: [");
    payload[length] = '\0';
    String newPayload = String((char *)payload);
    Serial.print(newPayload);
    Serial.println("] ");

    if (newTopic == topic_led1_switch) {
        Serial.println("Light 1 - Switching state");
        if (newPayload == "ON") {
            light_P1_on = true;

            if (light_P1_target == -1) {
                EEPROM.get(4, light_P1_target); //get eeprom value for toggling
                Serial.print("Toggling - Last state was: ");
                Serial.println(light_P1_target);
            }
        } else {
                light_P1_on = false;
                light_P1_target = 0;
        }

        fade_led1 = true;
    }

    if (newTopic == topic_led1_levelCMD) {
        light_P1_target = newPayload.toInt();
        Serial.print("Light 1 - New target brightness: ");
        Serial.println(light_P1_target);
    }

    if (newTopic == topic_led2_switch) {
        Serial.println("Light 2 - Switching state");
        if (newPayload == "ON") {
            light_P2_on = true;

            if (light_P2_target == -1) {
                EEPROM.get(12, light_P2_target); //get eeprom value for toggling
                Serial.print("Toggling - Last state was: ");
                Serial.println(light_P2_target);
            }
        } else {
                light_P2_on = false;
                light_P2_target = 0;
        }

        fade_led2 = true;
    }

    if (newTopic == topic_led2_levelCMD) {
        light_P2_target = newPayload.toInt();
        Serial.print("Light 2 - New target brightness: ");
        Serial.println(light_P2_target);
    }
}

// Splash Screen with basic info.
void splashScreen() {
    for (int i = 0; i <= 5; i++)
        Serial.println();
    Serial.println("#######################################");
    Serial.println("ESP8266 Light Controller");
    Serial.println("    ### Device ###");
    Serial.print("Device Name: ");
    Serial.println(espName);
    Serial.print("Client Version: ");
    Serial.println(sw_version);
    Serial.println("    ### Config ###");
    Serial.print("Debugging enabled: ");
    Serial.println(DEBUGGING);
    Serial.print("Single-Light mode: ");
    Serial.println(single_light);
    Serial.println("#######################################");
    for (int i = 0; i < 2; i++)
        Serial.println();
}