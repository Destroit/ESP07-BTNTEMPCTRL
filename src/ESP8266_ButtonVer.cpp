/*
1.Wi-Fi for ESP8266
2.UDP protocol realisation
3.Time synchroniastion using NTP
4.MQTT
5.I2C for OLED SSD1306
6.OneWire for D18B20
7.for D18B20 
8.for buttons
9.for SSD1306 OLED 
*/

#include <Ticker.h>
#include <ESP8266WiFi.h>       
#include <PubSubClient.h>       
#include <Wire.h>               
#include <OneWire.h>            
#include <DallasTemperature.h>  
#include <OneButton.h>         
#include "SSD1306Wire.h"
//#include <WiFiUdp.h>            
//#include <NTPClient.h>   

const char* SSID = "Smart_Home";
const char* PASSWORD = "shirokod"; 
const char* MQTT_SERVER = "192.168.120.92";
const char* MQTT_LOGIN = "mosquitto";
const char* MQTT_PASSWORD = "mosquitto";
const char* MQTT_SENDTPC =  "home/CurrentTemp";
const char* MQTT_RECIEVETPC = "home/SetTemp";
const char* MQTT_BACKSENDTPC = "home/BackSetTemp";
const int MQTT_PORT = 1883;
const int RELAY_PIN = 12;
const int BTN_PIN = 14;
const int ONEWIRE_BUS = 13;

/*
const long UTC_OFFSET_SECONDS = 10800;
const float NIGHT_L_TEMP_BORDER = 16;
const float NIGHT_H_TEMP_BORDER = 20;
const int PM_NIGHT_TIME = 19;
const int AM_NIGHT_TIME = 5;
*/

String modeString;
String dispWifiStatus;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastTemp = 0;
unsigned long lastReconnectAttempt = 0;
float sensorTemp = 0;
float sumTemp = 0;
int modecount = 1;
int reqTemp = 18;
int deviceCount = 0;
char msgT[7] = "";
char backreqT[7] = "";

/*
int hours = 0;
int minutes = 0;
int seconds = 0;
*/


OneWire DallasOneWire(ONEWIRE_BUS);
DallasTemperature sensors(&DallasOneWire);
OneButton button(BTN_PIN, true, true);
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);
void offlineActionList();
void displayPrepare();
Ticker btnTimer;
/*
WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp, "pool.ntp.org", UTC_OFFSET_SECONDS);
*/

void btntick()
{
  button.tick();
}

void wifiSetup()
{
  Serial.println("entering wifiSetup");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(".");
    offlineActionList();
  }
}

void modeclick()
{
  modecount++;
}

void switchclick()
{
  switch (modecount)
  {
   case 1:
   reqTemp++;
   break;

   case 2:
   reqTemp--;
   break;
  }
}

void recievedTemp(char* topic, byte* payload, unsigned int length)
{
  Serial.println("entering recievedTemp");
  char arrayToInt[length] = ""; 
  Serial.print("Message arrived from ");
  Serial.println(topic);
  for (unsigned int i = 0; i < length; i++) 
  {
    Serial.print((char)payload[i]);
    arrayToInt[i] = (char)payload [i];
  }
  reqTemp = atoi(arrayToInt);
}

bool reconnect()
{
  Serial.println("reconnecting");
  offlineActionList();
  if (client.connect("ESP8266Client")) 
  {
    Serial.println("connected");
    client.publish(MQTT_SENDTPC, msgT); //Публикация температуры на сервер
    client.subscribe(MQTT_RECIEVETPC);     //Подписка на топик
  } 
  return client.connected();
}

void setup()
{
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);  

  btnTimer.attach(0.6, btntick);

  randomSeed(micros());

  sensors.begin();
  sensors.requestTemperatures();

  button.setDebounceTicks(50);
  button.attachLongPressStop(modeclick);
  button.attachClick(switchclick);

  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  wifiSetup();
  
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(recievedTemp);
}

void displayPrepare()
{
  //Prepares data for being displayed 
  String reqstr = String(reqTemp);
  String realstr = String(sumTemp);
  //String StrTime = String(hours)+":"+String(minutes)+":"+String(seconds);
  display.drawString(0, 0, "Set T:"+reqstr);
  display.drawString(0, 10, "Real T:"+realstr);
  display.drawString(48, 0, modeString);
  //display.drawString(0, 20, StrTime);
  display.drawString(60, 0, dispWifiStatus);

}

void modeCurrent()
{
   switch (modecount)
    {
    case 1:
    modeString = "+";
    break;

    case 2:
    modeString = "-";
    break;

    default:
    modecount = 1;
    break;
    }
}


void wifiCurrent()
{
  //Prepares current Wifi status for display
  switch(WiFi.status())
  {
    case WL_CONNECTED:
    dispWifiStatus = "WL +";
    break;

    case WL_CONNECTION_LOST:
    dispWifiStatus = "WL lst";
    break;

    case WL_NO_SSID_AVAIL:
    dispWifiStatus = "WL id";
    break;

    case WL_CONNECT_FAILED:
    dispWifiStatus = "WL fld";
    break;

    case WL_IDLE_STATUS:
    dispWifiStatus = "WL idl";
    break;

    case WL_DISCONNECTED:
    dispWifiStatus = "WL dis";
    break;
  }
}

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
    /*
    Sensors' values to port
    Serial.print("Sensor ");                   
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(Sensor_temp);
    */
    sumTemp += sensorTemp;
  }
  sumTemp /= deviceCount;
  //Serial.println(sumTemp);
  dtostrf(sumTemp, 6, 2, msgT);
} 

void relayHandler()
{
  if (sumTemp < (reqTemp-1)){
    digitalWrite(RELAY_PIN, HIGH);
  }
  if (sumTemp > (reqTemp+1)){
    digitalWrite(RELAY_PIN, LOW);
  }
  /*
  Night mode (Needs RTC, in progress)
  if ((hours>PM_NIGHT_TIME) or (hours<AM_NIGHT_TIME))
  {
    if (sumTemp < NIGHT_L_TEMP_BORDER)
    {
      digitalWrite(RELAY_PIN, HIGH);
    }
    if (sumTemp > NIGHT_H_TEMP_BORDER)
    {
      digitalWrite(RELAY_PIN, LOW);
    }
  }
  else{
    if(sumTemp < reqTemp-2)
    {
    digitalWrite(RELAY_PIN, HIGH);
    }
    if(sumTemp > reqTemp+2)
    {
    digitalWrite(RELAY_PIN, LOW);
    }
  }*/
}

/*
void getNtpTime()
{
  timeClient.update();
  hours = timeClient.getHours();
  minutes = timeClient.getMinutes();
  seconds = timeClient.getSeconds();
}
*/

void offlineActionList()
{
  //Tasks that could be done without WiFi
  button.tick();
  wifiCurrent();
  modeCurrent();
  displayPrepare();
  if(millis()-lastTemp > 5000)
  {
    getTemp();
    lastTemp = millis();
  }
  relayHandler();
  display.display();
  display.clear();
}


void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    wifiSetup();
  }
  else 
  {
    if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
     }
    }
    else
    {
    offlineActionList();
    client.loop();
    dtostrf(reqTemp, 6, 2, backreqT);
    client.publish(MQTT_SENDTPC, msgT);
    client.publish(MQTT_BACKSENDTPC, backreqT);
    }
  }
} 