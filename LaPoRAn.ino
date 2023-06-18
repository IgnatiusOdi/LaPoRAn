// Library
#include <EEPROM.h>
#include <PubSubClient.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>

// Credential
#include "env/WiFiCredential.h"
#include "env/MQTTCredential.h"

// WiFi
const char* ssid = SSID;
const char* password = PASSWORD;

// MQTT Broker
const char* mqtt_broker = MQTT_BROKER;
const char* topic = MQTT_TOPIC;
const char* mqtt_username = MQTT_USERNAME;
const char* mqtt_password = MQTT_PASSWORD;
const int mqtt_port = MQTT_PORT;

// EEPROM
#define EEPROM_SIZE 512

// EEPROM address to store latitude and longitude
const int eepromAddr = 0;

// GPS Serial Connection
HardwareSerial SerialGPS(1);  // Use UART 1 for ESP32 (GPIO16: RX, GPIO17: TX)
TinyGPSPlus gps;              // GPS object

// WiFi and MQTT clients
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

// Connect to WiFi
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi!");
}

// MQTT callback function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println("-----------------------");
}

// Connect to MQTT broker
void connectMQTT() {
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);

  while (!mqtt_client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (mqtt_client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT!");
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" Retrying in 1 seconds...");
      delay(1000);
    }
  }
}

// Publish GPS coordinates to MQTT
void publishGPS(float latitude, float longitude) {
  char latStr[10];
  dtostrf(latitude, 8, 6, latStr);

  char lonStr[10];
  dtostrf(longitude, 9, 6, lonStr);

  mqtt_client.publish("gps/latitude", latStr);
  mqtt_client.publish("gps/longitude", lonStr);
}

// Store GPS coordinates in EEPROM
void storeGPS(float latitude, float longitude) {
  EEPROM.put(eepromAddr, latitude);
  EEPROM.put(eepromAddr + sizeof(float), longitude);
  EEPROM.commit();
  Serial.println("GPS coordinates stored in EEPROM!");
}

void setup() {
  // Set software serial baud to 115200
  Serial.begin(115200);
  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);  // GPS module connected to UART1 (GPIO16: RX, GPIO17: TX)

  connectWiFi();
  connectMQTT();
}

void loop() {
  while (SerialGPS.available() > 0) {
    if (gps.encode(SerialGPS.read())) {
      if (gps.location.isValid()) {
        float latitude = gps.location.lat();
        float longitude = gps.location.lng();

        // Publish GPS coordinates to MQTT
        publishGPS(latitude, longitude);

        // STORE GPS coordinates to EEPROM
        storeGPS(latitude, longitude);

        Serial.print("Latitude: ");
        Serial.println(latitude, 6);
        Serial.print(", Longitude: ");
        Serial.println(longitude, 6);
      }
    }
  }

  // Reconnect to MQTT if disconnected
  if (!mqtt_client.connected()) {
    connectMQTT();
  }

  // MQTT client loop
  mqtt_client.loop();
}