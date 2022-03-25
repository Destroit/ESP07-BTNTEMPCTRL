
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <Wire.h>
#include <OneWire.h>
#include "EncButton.h"
#include "DallasTemperature.h"
#include "AsyncMqttClient.h"
#include "SSD1306Wire.h"

#define WIFI_SSID "ssid"
#define WIFI_PASSWORD "pwd"
#define MQTT_HOST IPAddress(192, 168, 100, 116)
#define MQTT_PORT 1883
#define MQTT_LOGIN "mosquitto"
#define MQTT_PASSWORD "mosquitto"
#define MQTT_SENDTPC "home/CurrentTemp"
#define MQTT_RECIEVETPC "home/SetTemp"
#define MQTT_BACKSENDTPC "home/BackSetTemp"
#define RELAY_PIN 2
#define ONEWIRE_BUS 13
#define ENC_MODE 1
#define ENC_A 12
#define ENC_B 14

const uint8_t relayInterval = 2; // temperature gap in Celsius to reduce relay switching 
char msgT[7] = "";               // to send 
char backreqT[7] = "";
int8_t reqTemp = 18;
uint8_t deviceCount = 0;
uint64_t lastTempCall = 0; 
uint64_t lastBackReq = 0;
float sensorTemp = 0;
float sumTemp = 0;

EncButton<ENC_MODE, ENC_A, ENC_B> enc;
String dispWifiStatus;
AsyncMqttClient espClient;
Ticker mqttReconnectTimer;
Ticker wifiReconnectTimer;
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
OneWire DallasOneWire(ONEWIRE_BUS);
DallasTemperature sensors(&DallasOneWire);
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_64);

void connectToWifi();
void connectToMqtt();
void onWifiConnect(const WiFiEventStationModeGotIP& event);
void onWifiDisconnect(const WiFiEventStationModeDisconnected& event);
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttSubscribe(uint16_t packetId, uint8_t qos);
void onMqttUnsubscribe(uint16_t packetId);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void onMqttPublish(uint16_t packetId);
void displayDraw();
void wifiCurrent();
void getTemp();

// Encoder via interrupt
void IRAM_ATTR isr()
{
  enc.tickISR();
}

void rightInc()
{
  ++reqTemp;
}

void leftDec()
{
  --reqTemp;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Serial OK");
    pinMode(RELAY_PIN, OUTPUT);
    display.init();
    enc.attach(LEFT_HANDLER, leftDec);
    enc.attach(RIGHT_HANDLER, rightInc);
    attachInterrupt(0, isr, CHANGE);
    attachInterrupt(1, isr, CHANGE);
    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
    espClient.onConnect(onMqttConnect);
    espClient.onDisconnect(onMqttDisconnect);
    espClient.onSubscribe(onMqttSubscribe);
    espClient.onUnsubscribe(onMqttUnsubscribe);
    espClient.onMessage(onMqttMessage);
    espClient.onPublish(onMqttPublish);
    espClient.setServer(MQTT_HOST, MQTT_PORT);
    connectToWifi();
}

void connectToWifi()
{
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt()
{
    Serial.println("Connecting to MQTT...");
    espClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP& event)
{
    Serial.println("Connected to Wi-Fi.");
    connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event)
{
    Serial.println("Disconnected from Wi-Fi.");
    mqttReconnectTimer.detach();
    wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool sessionPresent)
{
    Serial.println("Connected to MQTT.");
    Serial.print("Session present: ");
    Serial.println(sessionPresent);
    uint16_t packetIdSub = espClient.subscribe(MQTT_RECIEVETPC, 1);
    Serial.print("Subscribing MQTT, packetId: ");
    Serial.println(packetIdSub);
    uint16_t packetIdPub1 = espClient.publish(MQTT_SENDTPC, 1, true, msgT);
    Serial.print("Publishing Temperature, packetId: ");
    Serial.println(packetIdPub1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{ 
    Serial.println("Disconnected from MQTT.");
    if (WiFi.isConnected()) 
    {
        mqttReconnectTimer.once(2, connectToMqtt);
    }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
    Serial.println("Subscribe acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
    Serial.print("  qos: ");
    Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId)
{
    Serial.println("Unsubscribe acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    Serial.println("Mesasage received.");
    Serial.print("  topic: ");
    Serial.println(topic);
    Serial.print("  qos: ");
    Serial.println(properties.qos);
    Serial.print("  dup: ");
    Serial.println(properties.dup);
    Serial.print("  retain: ");
    Serial.println(properties.retain);
    Serial.print("  len: ");
    Serial.println(len);
    Serial.print("  index: ");
    Serial.println(index);
    Serial.print("  total: ");
    Serial.println(total);
    char arrayToInt[len] = ""; 
    for (unsigned int i = 0; i < len; i++) 
    {
        Serial.print(payload[i]);
        arrayToInt[i] = payload [i];
    }
    reqTemp = atoi(arrayToInt);
}

void onMqttPublish(uint16_t packetId)
{
    Serial.println("Publish acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
}

void wifiCurrent()
{
    switch(WiFi.status())
    {
        case WL_CONNECTED:
        dispWifiStatus = "WiFi OK";
        break;
        
        case WL_CONNECTION_LOST:
        dispWifiStatus = "WiFi lost";
        break;
        
        case WL_NO_SSID_AVAIL:
        dispWifiStatus = "WiFi ssid";
        break;
        
        case WL_CONNECT_FAILED:
        dispWifiStatus = "WiFi failed";
        break;
        
        case WL_IDLE_STATUS:
        dispWifiStatus = "WiFi idle";
        break;
        
        case WL_DISCONNECTED:
        dispWifiStatus = "WiFi disconnected";
        break;
    }
}

//Prepares display
void displayDraw()
{
  String reqstr = String(reqTemp);
  String realstr = String(sumTemp);
  display.drawString(0, 0, "Set T:"+reqstr);
  display.drawString(0, 10, "Real T:"+realstr);
  display.drawString(0, 20, dispWifiStatus);
  display.display();
  display.clear();

}

//Gets temperature from sensors
void getTemp()
{
    Serial.println("entering getTemp");
    sensors.requestTemperatures();
    sensorTemp = 0;
    sumTemp = 0;
    deviceCount = sensors.getDeviceCount();
    Serial.print("Devices found: ");
    Serial.println(deviceCount);
    DeviceAddress Address18b20;
    sensors.getAddress(Address18b20, 1);
    for (int i = 0; i<= deviceCount-1; i++)
    {
        DeviceAddress Address18b20;
        sensors.getAddress(Address18b20, i);
        sensorTemp = sensors.getTempC(Address18b20);
        Serial.print("Sensor ");                   
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.println(sensorTemp);
        sumTemp += sensorTemp;
    }
    sumTemp /= deviceCount;
    Serial.println(sumTemp);
    dtostrf(sumTemp, 6, 2, msgT);
}

//Relay reaction on temperature changes
void relay()
{
  if (reqTemp > (sumTemp + relayInterval))
  {
    digitalWrite(RELAY_PIN, HIGH);
  }
  else if (reqTemp < (sumTemp - relayInterval))
  {
    digitalWrite(RELAY_PIN, LOW);
  }
}

void loop()
{
    if (millis() - lastTempCall > 5000)
    {
        lastTempCall = millis();
        getTemp();
        espClient.publish(MQTT_SENDTPC, 1, true, msgT);
    }

    if (millis() - lastBackReq > 2000)
    {
        lastBackReq = millis();
        dtostrf(reqTemp, 6, 2, backreqT);
        espClient.publish(MQTT_BACKSENDTPC, 1 , true, backreqT);
    }
    relay();
    enc.tick();
    wifiCurrent();
    displayDraw();
}