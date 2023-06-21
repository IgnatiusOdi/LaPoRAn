// LIBRARY
#include <PubSubClient.h>
#include <Preferences.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>

// CREDENTIAL
#include "env/MQTTCredential.h"
#include "env/WiFiCredential.h"

// VARIABLE
unsigned long prev = 0;
int counter = 0;
const int RXpin = 13, TXpin = 14, GPSBaud = 9600;
// const double latitude = -7.291378;
// const double longitude = 112.758697;

// MQTT BROKER
const char *mqtt_broker = MQTT_BROKER;
const char *mqtt_topic = MQTT_TOPIC;
const char *mqtt_username = MQTT_USERNAME;
const char *mqtt_password = MQTT_PASSWORD;
const int mqtt_port = MQTT_PORT;

// PREFERENCES
Preferences preferences;

// SERIAL CONNECTION TO GPS
SoftwareSerial ss(RXpin, TXpin);

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
  Serial.println("Connecting to WiFi...");
  delay(500);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not Connected to WiFi!");
  } else {
    Serial.println("Connected to WiFi!");
  }
}

// MQTT CALLBACK FUNCTION
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println("---------------------------------");
}

// Connect to MQTT broker
void connectMQTT() {
  // CLIENT ID
  String client_id = "esp32-client-";
  client_id += String(WiFi.macAddress());

  Serial.print("Attempting MQTT connection...");
  // ATTEMPT TO CONNECT MQTT
  if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
    // SUBSCRIBE MQTT TOPIC
    mqtt_client.subscribe(mqtt_topic);
    Serial.println("MQTT connected");
  } else {
    Serial.print("failed with state ");
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
  const char *key = counter + "";
  preferences.putDouble(key, value);
}

void setup() {
  // SET SOFTWARE SERIAL BAUD TO 115200
  Serial.begin(115200);

  // SET GPS BAUD
  ss.begin(GPSBaud);

  // CONNECT WIFI
  connectWiFi();

  // SET MQTT CALLBACK
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);

  // SET COUNTER
  preferences.begin("GPS", false);
  counter = preferences.getInt("counter");
}

void loop() {
  while (ss.available() > 0) {
    unsigned long now = millis();

    // ATTEMPT RECONNECT TO WIFI EVERY 5 SECONDS
    if (now - prev % 5000 == 0) {
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
      }
    }

    // ATTEMPT GET GPS LOCATION EVERY 30 SECONDS
    if (now - prev >= 30000) {
      // GET GPS LOCATION
      gps.encode(ss.read());

      if (gps.location.isUpdated()) {
        double latitude = gps.location.lat();
        double longitude = gps.location.lng();

        // SAVE TO EEPROM
        preferences.begin("GPS", false);
        if (counter == 8) {
          shiftPreferencesLocation();
          counter -= 2;
          updateCounter();
        }
        savePreferencesLocation(latitude);
        counter++;
        updateCounter();
        savePreferencesLocation(longitude);
        counter++;
        updateCounter();

        // IF CONNECTED TO WIFI, DUMP ALL EEPROM TO MQTT
        if (WiFi.status() == WL_CONNECTED) {
          // IF MQTT DISCONNECTED, RECONNECT
          if (!mqtt_client.connected()) {
            connectMQTT();
          }
          // PUBLISH GPS LOCATION TO MQTT TOPIC
          for (int i = 0; i < counter / 2; i++) {
            const char *lat = i * 2 + "";
            String ltt = String(preferences.getDouble(lat), 7);
            // char ltt[11] = dtostrf(preferences.getDouble(lat), 9, 7, ltt);
            const char *lng = i * 2 + 1 + "";
            String lnt = String(preferences.getDouble(lng), 7);
            // char lnt[11] = dtostrf(preferences.getDouble(lng), 9, 7, lnt);

            String result = ltt + ", " + lnt;
            mqtt_client.publish(mqtt_topic, result.c_str());
          }
          counter = 0;
          updateCounter();
        }

        // CLOSE PREFERENCES
        preferences.end();
      }

      // RESET PREV
      prev = now;
    }
  }

  // MQTT CLIENT LOOP
  mqtt_client.loop();
}