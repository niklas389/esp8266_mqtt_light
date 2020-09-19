#include <ESP_EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

// Import settings
#include "config.h"
#define SERIAL_BAUD 115200

ESP8266WiFiMulti WiFiMulti;
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// int eeprom_led1 = 0;
int led1_value = 0;
int led2_value = 0;
double range_multiplier = (pwm_range / 100);
bool single_light = true;

// SETUP
void setup() {
    if (pin_led2 != 0) { single_light = false; }

    if (DEBUGGING == true) {
        Serial.begin(SERIAL_BAUD);
        splashScreen();
        delay(100);      
    }

    // Set PWM Freq to 19.2 kHz (Same as RPi)
    analogWriteFreq(19200);
    // Set PWM range to XXX (from 1023)
    analogWriteRange(pwm_range);  

    EEPROM.begin(16);

    if (restoreAfterReboot) {
        int mem1 = 0;
        EEPROM.get(0, mem1);
        if (mem1 != 0) {
            Serial.print("EEPROM first value was: ");
            Serial.println(mem1);
            if (mem1 > 100) {
                EEPROM.put(0, 0); 
                } else {
                fadePWM(pin_led1, mem1 * range_multiplier);
            }
        }

        int mem2 = 0;
        EEPROM.get(4, mem2);
        if (mem2 =! 0) {
            Serial.print("EEPROM second value was: ");
            Serial.println(mem2);
            if (mem2 > 100) {
                EEPROM.put(4, 0); 
                } else {
                fadePWM(pin_led2, mem2 * range_multiplier);
            }
        }
    }
    
    startWiFi();
    runMQTT();
}

void loop() {
    if (!client.connected()) { runMQTT(); }
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
            delay(30000);
            tryCnt = 0;
        }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP adress: ");
    Serial.println(WiFi.localIP());
    delay(100);
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
        }

        client.subscribe(topic_led1);
        client.subscribe(topic_led1_toggle);
        //If PIN2 is set to 0 then client will run in Single LED mode
        if (single_light == false) {
        client.subscribe(topic_led2);
        client.subscribe(topic_led2_toggle);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) 
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
    int intPayload = newPayload.toInt();

    // Fading Lights to received value
    if (newTopic == topic_led1) {
        Serial.println("FADE - LED 1");
        fadePWM(pin_led1, int(intPayload * range_multiplier));
        // Save to EEPROM if restoreAfterReboot is set true
        EEPROM.put(0, intPayload); EEPROM.commit();
    }

    if (newTopic == topic_led2) {
        Serial.println("FADE - LED 2");
        fadePWM(pin_led2, int(intPayload * range_multiplier));
        // Save to EEPROM if restoreAfterReboot is set true
        EEPROM.put(4, intPayload); EEPROM.commit();
    }

    if (newTopic == topic_led1_toggle) {
        Serial.println("TOGGLE - LED 1");
        Serial.println(led1_value);
        if (intPayload == 0) {
            if (led1_value != 0) { fadePWM(pin_led1, 0); } else { fadePWM(pin_led1, 100 * range_multiplier); }
        } else {
            int stored_value = 0;
            EEPROM.get(0, stored_value);
            //fadePWM(pin_led1, stored_value * range_multiplier);
            if (led1_value != 0) { fadePWM(pin_led1, 0); } else { fadePWM(pin_led1, stored_value * range_multiplier); }
        }
    }

    if (newTopic == topic_led2_toggle) {
        Serial.println("TOGGLE - LED 2");
        Serial.println(led2_value);
        if (intPayload == 0) {
            if (led2_value != 0) { fadePWM(pin_led2, 0); } else { fadePWM(pin_led2, 100 * range_multiplier); }
        } else {
            int stored_value = 0;
            EEPROM.get(4, stored_value);
            //fadePWM(pin_led1, stored_value * range_multiplier);
            if (led2_value != 0) { fadePWM(pin_led2, 0); } else { fadePWM(pin_led2, stored_value * range_multiplier); }
        }
    }
}

// Fading PWM values to new (received) values.
void fadePWM(int PIN, int newValue) {
    int i;

    switch (PIN) {
    case pin_led1:
        i = led1_value;
        while (i != newValue) {
            if (newValue < i) {i --;} else {i ++;}
            if ((i * range_multiplier) < 15) {delay(16);} else {delay(2);}
            // delay(2);
            analogWrite(PIN, i); 
        }
        led1_value = i;
        publish_lightStatus(pin_led1, led1_value);
        break;

    case pin_led2:
        i = led2_value;
        while (i != newValue) {
            if (newValue < i) {i --;} else {i ++;}
            if ((i * range_multiplier) < 15) {delay(16);} else {delay(2);}
            // delay(2)
            analogWrite(PIN, i); 
        }
        led2_value = i;
        publish_lightStatus(pin_led2, led2_value);
        break;
   
    default:
        break;
    }
}

// Publishing new PWM-values via MQTT
void publish_lightStatus (int ledpin, int value) {
    Serial.print("PIN ");
    Serial.print(ledpin);
    Serial.print(" new PWM value: ");
    Serial.print(value);
    Serial.print(" (");
    Serial.print(round(value / range_multiplier));
    Serial.println(")");

    switch (ledpin) {
    case pin_led1:
        client.publish(topic_led1_state, String(round(value / range_multiplier)).c_str(), true);
        Serial.println("Published Light 1 value");
        break;

    case pin_led2:
        client.publish(topic_led2_state, String(round(value / range_multiplier)).c_str(), true);
        Serial.println("Published Light 2 value");
        break;

    default:
        Serial.println("publish_lightStatus : DEFAULT CASE!");
        break;
    }
}

// Splash Screen with basic info.
void splashScreen() {
    for (int i=0; i<=5; i++) Serial.println();
    Serial.println("#######################################");
    Serial.println("ESP8266 Light Controller");
    Serial.print("Device Name: ");
    Serial.println(espName);
    Serial.print("Client Version: ");
    Serial.println(clientVer);
    Serial.print("Debugging enabled: ");
    Serial.println(DEBUGGING);
    Serial.print("Single Light-mode: ");
    Serial.println(single_light);
    Serial.println("#######################################");
    for (int i=0; i<2; i++) Serial.println();
}