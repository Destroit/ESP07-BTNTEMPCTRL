#include <ESP8266WiFi.h>        //Библиотека Wi-Fi для модуля ESP8266
#include <WiFiUdp.h>            //Библиотека реализации протокола UDP через Wi-Fi
#include <NTPClient.h>          //Библиотека синхронизации часов
#include <Wire.h>               //Библеотека реализации протокола I2C для OLED SSD1306
#include <OneWire.h>            //Библиотека реализации однопроводная линии передачи данных OneWire
#include <DallasTemperature.h>  // Для работы с датчиками Dallas     
#include <OneButton.h>         
#include "SSD1306Wire.h"

const char* SSID = "ssid";
const char* PASSWORD = "pwd"; 
const int RELAY_PIN = 12;
const int BTN_PIN = 14;
const int ONEWIRE_BUS = 13;
const long UTC_OFFSET_SECONDS = 10800;
const float NIGHT_L_TEMP_BORDER = 16;
const float NIGHT_H_TEMP_BORDER = 20;
const int PM_NIGHT_TIME = 19;
const int AM_NIGHT_TIME = 5;

String modeString;
String DispWifiStatus;
unsigned long lastTemp = 0;
float Sensor_temp = 0;
float SumTemp = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
int modecount = 1;
int reqTemp = 18;
int deviceCount = 0;

WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp, "pool.ntp.org", UTC_OFFSET_SECONDS);
OneWire DallasOneWire(ONEWIRE_BUS);
DallasTemperature sensors(&DallasOneWire);
OneButton button(BTN_PIN, true);
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);

void setup_wifi()
{
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
}

void wifi_current()
{
  switch(WiFi.status())
  {
    case WL_CONNECTED:
    DispWifiStatus = "WiFi +";
    break;

    case WL_CONNECTION_LOST:
    DispWifiStatus = "no WiFi";
    break;

    case WL_NO_SSID_AVAIL:
    DispWifiStatus = "WiFi -";
    break;

    case WL_CONNECT_FAILED:
    DispWifiStatus = "Wifi x";
    break;
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

void setup()
{
  //Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);  

  sensors.begin();
  sensors.requestTemperatures();

  button.setDebounceTicks(50);
  button.attachLongPressStop(modeclick);
  button.attachClick(switchclick);

  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  setup_wifi();
}

void disp()
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
  String reqstr = String(reqTemp);
  String realstr = String(SumTemp);
  String StrTime = String(hours)+":"+String(minutes)+":"+String(seconds);
  display.drawString(0, 0, "Set T:"+reqstr);
  display.drawString(0, 10, "Real T:"+realstr);
  display.drawString(48, 0, modeString);
  display.drawString(0, 20, StrTime);
  display.drawString(60, 0, DispWifiStatus);
}

void getTemp()
{
  sensors.requestTemperatures();
  Sensor_temp = 0;
  SumTemp = 0;
  deviceCount = sensors.getDeviceCount();
  Serial.print("Devices found: ");
  Serial.println(deviceCount);
  DeviceAddress Address18b20;
  sensors.getAddress(Address18b20, 1);
  for (int i = 0; i<= deviceCount-1; i++)
  {
    DeviceAddress Address18b20;
    sensors.getAddress(Address18b20, i);
    Sensor_temp = sensors.getTempC(Address18b20);
    /*Serial.print("Sensor ");                   //Значение датчиков по отдельности
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(Sensor_temp);*/
    SumTemp += Sensor_temp;
  }
  SumTemp /= deviceCount;
  Serial.println(SumTemp);
} 

void relay()
{
  if (SumTemp < reqTemp){
    digitalWrite(RELAY_PIN, HIGH);
  }
  if (SumTemp > reqTemp){
    digitalWrite(RELAY_PIN, LOW);
  }
  /*if ((hours>PM_NIGHT_TIME) or (hours<AM_NIGHT_TIME))
  {
    if (SumTemp < NIGHT_L_TEMP_BORDER)
    {
      digitalWrite(RELAY_PIN, HIGH);
    }
    if (SumTemp > NIGHT_H_TEMP_BORDER)
    {
      digitalWrite(RELAY_PIN, LOW);
    }
  }
  else{
    if(SumTemp < reqTemp-2)
    {
    digitalWrite(RELAY_PIN, HIGH);
    }
    if(SumTemp > reqTemp+2)
    {
    digitalWrite(RELAY_PIN, LOW);
    }
  }*/
}

void time()
{
  timeClient.update();
  hours = timeClient.getHours();
  minutes = timeClient.getMinutes();
  seconds = timeClient.getSeconds();
}


void loop()
{
  button.tick();
  wifi_current();
  disp();
  if (WiFi.status() == WL_CONNECTED)
  {
    time();
  }
  if(millis()-lastTemp > 5000)
  {
    getTemp();
    lastTemp = millis();
  }
  relay();
  display.display();
  display.clear();
}