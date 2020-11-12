#include <ESP_EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

// Import settings
#include "config.h"

const String sw_version = "2020.46.2";

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

// SETUP
void setup() {
    if (pin_led2 != 0) { single_light = false; }

    if (DEBUGGING == true) {
        Serial.begin(115200);
        splashScreen();
        delay(100);      
    }

    // Set PWM Freq
    analogWriteFreq(1000);
    // Set PWM range to XXX (from 1023)
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

void loop() {
    if (!client.connected()) { 
        runMQTT(); 
    }
    client.loop();
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
    Serial.println("Starting MQTT-Client with following credentials:");
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
            Serial.println(" >>Success!");
            Serial.println(client.state());
        } else {
            Serial.println(" >> Failed!");
            Serial.println("Couldn't connect to MQTT-Server.");
            delay(30000)
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

void callback(char* topic, byte* payload, unsigned int length) {
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
        Serial.println("LED1 - Switching State");
        led1_state = newPayload;
        pwm_ctrl_led1();
    }

    if (newTopic == topic_led1_levelCMD) {
        Serial.println("LED1 - Setting Brightness");
        led1_level_set = newPayload.toInt();
    }

    if (newTopic == topic_led2_switch) {
        Serial.println("LED2 - Switching State");
        led2_state = newPayload;
        pwm_ctrl_led2();
    }

    if (newTopic == topic_led2_levelCMD) {
        Serial.println("LED2 - Setting Brightness");
        led2_level_set = newPayload.toInt();
    }
}

void pwm_ctrl_led1() {
    Serial.println("#######################################");
    Serial.print("Old value: ");
    Serial.println(led1_level);

    if (led1_state == "ON") {    
        //get eeprom value for toggling
        if (led1_level_set == -1) {
            EEPROM.get(4, led1_level_set);
            Serial.print("    EEPROM value: ");
            Serial.println(led1_level_set);
        }     
    } else {
        led1_level_set = 0;
    }

    Serial.print("New value: ");
    Serial.println(led1_level_set);

    while (led1_level != led1_level_set) {
        if (led1_level_set < led1_level) {led1_level --;} else {led1_level ++;}
        if (led1_level < 15) {delay(10);} else {delay(2);}
        // delay(2);
        analogWrite(pin_led1, led1_level); 
    }

    Serial.println("---------------------------------------");
    Serial.print("Faded to: ");
    Serial.println(led1_level);
    Serial.print("New State: ");
    Serial.println(led1_state);
    Serial.println("---------------------------------------");

    if (led1_state == "ON" && led1_level > 0) {
        // SAVE STATE & BRIGHTNESS
        EEPROM.put(0, 1);
        EEPROM.put(4, led1_level);
        EEPROM.commit();
        client.publish(topic_led1_state, "ON");
        client.publish(topic_led1_level, String(led1_level).c_str());
        Serial.println("    Saved and published LEVEL AND STATE!");
    } else {
        // SAVE STATE ONLY
        EEPROM.put(0, 0);
        EEPROM.commit();
        client.publish(topic_led1_state, "OFF");
        led1_level_set = -1;
        Serial.println("    Saved and published STATE!");
    }    
    Serial.println("#######################################");   
}

void pwm_ctrl_led2() {
    Serial.println("#######################################");
    Serial.print("Old value: ");
    Serial.println(led2_level);
    Serial.print("New value: ");
    Serial.println(led2_level_set);

    if (led2_state == "ON") {    
        //get eeprom value for toggling
        if (led2_level_set == -1) { EEPROM.get(12, led2_level_set); }
    } else {
        led2_level_set = 0;
    }

    while (led2_level != led2_level_set) {
        if (led2_level_set < led2_level) {led2_level --;} else {led2_level ++;}
        if (led2_level < 15) {delay(10);} else {delay(2);}
        // delay(2);
        analogWrite(pin_led2, led2_level); 
    }


    Serial.print("Faded to: ");
    Serial.println(led2_level);
    Serial.print("New State: ");
    Serial.println(led2_state);

    if (led1_state == "ON") {
        // SAVE STATE & BRIGHTNESS
        EEPROM.put(8, 1);
        EEPROM.put(12, led2_level);
        EEPROM.commit();
        client.publish(topic_led2_state, "ON");
        client.publish(topic_led2_level, String(led2_level).c_str());
    } else {
        // SAVE STATE ONLY
        EEPROM.put(8, 0);
        EEPROM.commit();
        client.publish(topic_led2_state, "OFF");
        led2_level_set = -1;
    }    
    Serial.println("#######################################");
}

// Splash Screen with basic info.
void splashScreen() {
    for (int i=0; i<=5; i++) Serial.println();
    Serial.println("#######################################");
    Serial.println("ESP8266 Light Controller");
    Serial.print("Device Name: ");
    Serial.println(espName);
    Serial.print("Client Version: ");
    Serial.println(sw_version);
    Serial.print("Debugging enabled: ");
    Serial.println(DEBUGGING);
    Serial.print("Single Light-mode: ");
    Serial.println(single_light);
    Serial.println("#######################################");
    for (int i=0; i<2; i++) Serial.println();
}