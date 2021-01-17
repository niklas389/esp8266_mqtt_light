#include <ESP_EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

// Import settings
// #include "config.h"
#include "config_kitchen.h"

const String sw_version = "2021.2.3";

ESP8266WiFiMulti WiFiMulti;
WiFiClient wifiClient;
PubSubClient client(wifiClient);

bool single_light = true;

String led1_state = "OFF";
int led1_level_set = 0;
int led1_level = 0;
String led2_state = "OFF";
int led2_level_set = 0;
int led2_level = 0;

int fade_interval = 3;

// SETUP
void setup() {
    //Start serial if debugging is enabled in config
    if (DEBUGGING == true) {
        Serial.begin(115200);
        splashScreen();
    }

    //Set PWM to 0 to avoid MAX brightness after power loss.
    analogWrite(pin_led1, 0);

    //Single lightMode if pin2 not set
    if (pin_led2 != 0) {
        single_light = false;
        analogWrite(pin_led2, 0);
    }

    // Set PWM Freq
    analogWriteFreq(1000);
    // Set PWM range to XXX (DEF: 1023) (set in config)
    analogWriteRange(pwm_range);

    EEPROM.begin(16);
    if (restoreAfterReboot) {
        int tmp_val1;
        int tmp_val2;

        EEPROM.get(0, tmp_val1);
        if (tmp_val1 == 0) {
            led1_state = "OFF";
        } else if (tmp_val1 == 1) {
            led1_state = "ON";
        } else {
            EEPROM.put(0, 0);
        }

        EEPROM.get(4, tmp_val2);
        if (tmp_val2 > 0 && tmp_val2 < 101) {
            led1_level_set = tmp_val2;
        } else {
            EEPROM.put(4, 0);
        }

        pwm_ctrl_led1();

        tmp_val1 = 0;
        tmp_val2 = 0;

        if (single_light == false) {
            EEPROM.get(8, tmp_val1);
            if (tmp_val1 == 0) {
                led2_state = "OFF";
            } else if (tmp_val1 == 1) {
                led2_state = "ON";
            } else {
                EEPROM.put(8, 0);
            }

            EEPROM.get(12, tmp_val2);
            if (tmp_val2 > 0 && tmp_val2 < 101) {
                led2_level_set = tmp_val2;
            } else {
                EEPROM.put(12, 0);
            }
            pwm_ctrl_led2();
        }

        EEPROM.commit();
    }

    startWiFi();
    runMQTT();
}

bool fade_led1 = false;
bool fade_led2 = false;
unsigned long current_millis = 0;
long prev_millis = 0;

void loop() {
    current_millis = millis();

    if (!client.connected()) {
        runMQTT();
    }
    
    client.loop();

    if (current_millis - prev_millis > fade_interval) {
        prev_millis = current_millis;
        if (fade_led1 == true) {vFade_LED1();}
        if (fade_led2 == true) {vFade_LED2();}
    }
}

void vFade_LED1() {
    if (led1_level != led1_level_set) {
        if (led1_level_set < led1_level) {
            led1_level--;
        } else {
            led1_level++;
        }
    }
    analogWrite(pin_led1, led1_level);

    if (led1_level == led1_level_set) {
        fade_led1 = false;
        Serial.println("fade_led1 set to false");

        Serial.println("---------------------------------------");
        Serial.print("IST: ");
        Serial.print(led1_level);
        Serial.print(" | SOLL: ");
        Serial.println(led1_state);
        Serial.println("---------------------------------------");

        // if (led1_state == "ON" && led1_level > 0) {
        //     // SAVE STATE & BRIGHTNESS
        //     EEPROM.put(0, 1);
        //     EEPROM.put(4, led1_level);
        //     EEPROM.commit();
        //     // client.publish(topic_led1_state, "ON");
        //     // client.publish(topic_led1_level, String(led1_level).c_str());
        //     Serial.println("    Saved and published LEVEL AND STATE!");
        // } else {
        //     // SAVE STATE ONLY
        //     EEPROM.put(0, 0);
        //     EEPROM.commit();
        //     // client.publish(topic_led1_state, "OFF");
        //     led1_level_set = -1;
        //     Serial.println("    Saved and published STATE!");
        // }
        Serial.println("#######################################");
    }
}

void vFade_LED2() {
    if (led2_level_set < led2_level) {
        led2_level--;
    } else {
        led2_level++;
    }
    analogWrite(pin_led2, led2_level);

    if (led2_level == led2_level_set) {
        fade_led2 = false;
        Serial.println("fade_led2 set to false");

        Serial.println("---------------------------------------");
        Serial.print("IST: ");
        Serial.print(led2_level);
        Serial.print(" | SOLL: ");
        Serial.println(led2_state);
        Serial.println("---------------------------------------");

        // if (led2_state == "ON" && led2_level > 0) {
        //     // SAVE STATE & BRIGHTNESS
        //     EEPROM.put(8, 1);
        //     EEPROM.put(12, led2_level);
        //     EEPROM.commit();
        //     // client.publish(topic_led1_state, "ON");
        //     // client.publish(topic_led1_level, String(led1_level).c_str());
        //     Serial.println("    Saved and published LEVEL AND STATE!");
        // } else {
        //     // SAVE STATE ONLY
        //     EEPROM.put(8, 0);
        //     EEPROM.commit();
        //     // client.publish(topic_led1_state, "OFF");
        //     led2_level_set = -1;
        //     Serial.println("    Saved and published STATE!");
        // }
        Serial.println("#######################################");
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
    Serial.print("Device Name: ");
    Serial.println(espName);

    WiFi.hostname(espName);
    WiFiMulti.addAP(wifi_ssid, wifi_password);

    int tryCnt = 0;
    Serial.print("Connecting...");
    while (WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
        tryCnt++;

        if (tryCnt > 25) {
            Serial.println("");
            Serial.println("Couldn't connect to WiFi.");
            delay(3000);
            ESP.restart();
        }
    }

    Serial.println("WiFi connected");
    Serial.print("IP adress: ");
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
        Serial.print("Attempting connection...");

        if (client.connect(espName, mqtt_user, mqtt_password)) {
            Serial.println(" >> Success!");
            Serial.println(client.state());
        } else {
            Serial.println(" >> Failed! (Couldn't connect to MQTT-Server)");
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
    // Serial.println();
    // Serial.print("Message arrived Topic: [");
    String newTopic = topic;
    // Serial.print(topic);
    // Serial.print("] | Payload: [");
    payload[length] = '\0';
    String newPayload = String((char *)payload);
    // Serial.print(newPayload);
    // Serial.println("] ");

    if (newTopic == topic_led1_switch) {
        Serial.print("LED1 - Switching State: ");
        led1_state = newPayload;
        Serial.println(led1_state);
        pwm_ctrl_led1();
    }

    if (newTopic == topic_led1_levelCMD) {
        Serial.print("LED1 - Setting Brightness to: ");
        led1_level_set = newPayload.toInt();
        Serial.println(led1_level_set);
    }

    if (newTopic == topic_led2_switch) {
        Serial.print("LED2 - Switching State: ");
        led2_state = newPayload;
        Serial.println(led2_state);
        pwm_ctrl_led2();
    }

    if (newTopic == topic_led2_levelCMD) {
        Serial.print("LED2 - Setting Brightness to: ");
        led2_level_set = newPayload.toInt();
        Serial.println(led2_level_set);
    }
}

void pwm_ctrl_led1() {
    Serial.println("#######################################");
    Serial.print("Current value: ");
    Serial.println(led1_level);

    if (led1_state == "ON") {
        //get eeprom value for toggling
        if (led1_level == -1) {
            EEPROM.get(4, led1_level_set);
            Serial.print("    EEPROM value: ");
            Serial.println(led1_level_set);
        }
    } else {
        led1_level_set = 0;
    }

    Serial.print("Fade to value: ");
    Serial.println(led1_level_set);

    if (led1_state == "ON" && led1_level_set > 0) {
        // SAVE STATE & BRIGHTNESS
        EEPROM.put(0, 1);
        EEPROM.put(4, led1_level);
        EEPROM.commit();

        client.publish(topic_led1_state, "ON");
        client.publish(topic_led1_level, String(led1_level_set).c_str());
    } else {
        // SAVE STATE ONLY
        EEPROM.put(0, 0);
        EEPROM.commit();
        // client.publish(topic_led1_state, "OFF");
        led1_level_set = -1;
        client.publish(topic_led1_state, "OFF");
        Serial.println("    Saved and published STATE!");
    }
    //Start fading
    fade_led1 = true;
}

void pwm_ctrl_led2() {
    Serial.println("#######################################");
    Serial.print("Old value: ");
    Serial.println(led2_level);

    if (led2_state == "ON") {
        //get eeprom value for toggling
        if (led2_level_set == -1) {
            EEPROM.get(12, led2_level_set);
            Serial.print("    EEPROM value: ");
            Serial.println(led2_level_set);
        }
    } else {
        led2_level_set = 0;
    }

    Serial.print("New value: ");
    Serial.println(led2_level_set);

    if (led2_state == "ON" && led2_level_set > 0) {
        // SAVE STATE & BRIGHTNESS
        EEPROM.put(8, 1);
        EEPROM.put(12, led2_level);
        EEPROM.commit();

        client.publish(topic_led2_state, "ON");
        client.publish(topic_led2_level, String(led2_level_set).c_str());
    } else {
        // SAVE STATE ONLY
        EEPROM.put(8, 0);
        EEPROM.commit();
        // client.publish(topic_led1_state, "OFF");
        led2_level_set = -1;

        client.publish(topic_led2_state, "OFF");
        Serial.println("    Saved and published STATE!");
    }
    //Start fading
    fade_led2 = true;
}

// Splash Screen with basic info.
void splashScreen() {
    for (int i = 0; i <= 5; i++)
        Serial.println();
    Serial.println("#######################################");
    Serial.println("ESP8266 Light Controller");
    Serial.println("Device:");
    Serial.print("Device Name: ");
    Serial.println(espName);
    Serial.print("Client Version: ");
    Serial.println(sw_version);
    Serial.println("Config:");
    Serial.print("Debugging enabled: ");
    Serial.println(DEBUGGING);
    Serial.print("Single-Light mode: ");
    Serial.println(single_light);
    Serial.println("#######################################");
    for (int i = 0; i < 2; i++)
        Serial.println();
}