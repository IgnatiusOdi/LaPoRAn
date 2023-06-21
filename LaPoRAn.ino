// LIBRARY
#include <PubSubClient.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>

// CREDENTIAL
#include "env/MQTTCredential.h"
#include "env/WiFiCredential.h"

// VARIABLE
#define GPSBaud 9600
unsigned long prev = 0;
int counter = 0;

// MQTT BROKER
const char *mqtt_broker = MQTT_BROKER;
const char *mqtt_topic = MQTT_TOPIC;
const char *mqtt_username = MQTT_USERNAME;
const char *mqtt_password = MQTT_PASSWORD;
const int mqtt_port = MQTT_PORT;

// PREFERENCES
Preferences preferences;

// TINYGPSPLUS
TinyGPSPlus gps;

// WIFI
const char *ssid = SSID;
const char *password = PASSWORD;

// WIFI & MQTT CLIENTS
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

// CONNECT TO WIFI
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.println(F("Connecting to WiFi..."));
  delay(2000);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Not Connected to WiFi!"));
  } else {
    Serial.println(F("Connected to WiFi!"));
  }
}

// MQTT CALLBACK FUNCTION
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print(F("Message: "));
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println(F(""));
  Serial.println(F("---------------------------------"));
}

// Connect to MQTT broker
void connectMQTT() {
  // CLIENT ID
  String client_id = "esp32-client-";
  client_id += String(WiFi.macAddress());

  Serial.println(F("Attempting MQTT connection..."));
  // ATTEMPT TO CONNECT MQTT
  if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
    // SUBSCRIBE MQTT TOPIC
    mqtt_client.subscribe(mqtt_topic);
    Serial.println(F("MQTT connected"));
  } else {
    Serial.print(F("failed with state "));
    Serial.print(mqtt_client.state());
    delay(3000);
  }
}

void updateCounter() {
  preferences.putInt("counter", counter);
}

void shiftPreferencesLocation() {
  for (int i = 0; i < 6; i++) {
    const char *key = i + "";
    const char *key2 = i + 2 + "";
    preferences.putDouble(key, preferences.getDouble(key2));
  }
}

void savePreferencesLocation(double value) {
  String key = String(counter);
  preferences.putDouble(key.c_str(), value);
}

void setup() {
  // SET SOFTWARE SERIAL BAUD TO 115200
  Serial.begin(115200);

  // SET GPS BAUD
  Serial2.begin(GPSBaud);

  // CONNECT WIFI
  connectWiFi();

  // SET MQTT CALLBACK
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);

  preferences.begin("GPS", false);
  preferences.clear();
}

void loop() {
  while (Serial2.available() > 0) {
    unsigned long now = millis();

    // ATTEMPT RECONNECT TO WIFI EVERY 5 SECONDS
    if (now - prev % 5000 == 0) {
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
      }
    }

    // ATTEMPT GET GPS LOCATION EVERY 30 SECONDS
    if (now - prev >= 10000) {
      // GET GPS LOCATION
      if (gps.encode(Serial2.read())) {
        Serial.println(F("Attempting GPS Location..."));

        // SAVE TO EEPROM
        preferences.begin("GPS", false);
        counter = preferences.getInt("counter", 0);
        if (counter == 8) {
          shiftPreferencesLocation();
          counter -= 2;
          updateCounter();
        }
        savePreferencesLocation(gps.location.lat());
        counter++;
        updateCounter();
        savePreferencesLocation(gps.location.lng());
        counter++;
        updateCounter();
        Serial.println(F("GPS Location saved!"));

        // IF CONNECTED TO WIFI, DUMP ALL EEPROM TO MQTT
        if (WiFi.status() == WL_CONNECTED) {
          // IF MQTT DISCONNECTED, RECONNECT
          if (!mqtt_client.connected()) {
            connectMQTT();
          }

          // PUBLISH GPS LOCATION TO MQTT TOPIC
          for (int i = 0; i < counter / 2; i++) {
            String lat = String(i * 2);
            String lng = String(i * 2 + 1);
            String latitude = String(preferences.getDouble(lat.c_str()), 6);
            String longitude = String(preferences.getDouble(lng.c_str()), 6);
            String result = latitude + ", " + longitude;
            mqtt_client.publish(mqtt_topic, result.c_str());
          }
          counter = 0;
          updateCounter();
          Serial.println(F("GPS Location uploaded!"));
        }

        // CLOSE PREFERENCES
        preferences.end();

        // RESET PREV
        prev = now;
      }
    }

    // MQTT CLIENT LOOP
    mqtt_client.loop();
  }
}