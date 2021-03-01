#include <NTPClient.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <MQTTClient.h>
#include <WiFiUdp.h>
#include <Adafruit_Sensor.h>
#include "DHT.h" // подключаем библиотеку для датчика
#include <SPI.h>
#include <ArduinoOTA.h>
#include "timerMinim.h"
#include "Credentials.h"

DHT dht(0, DHT11); // сообщаем на каком порту будет датчик
Servo myservo;

String timeValue = "";
String firstFeedTime = "";
String secondFeedTime = "";
int h = 0;
int t = 0;
int portion = 900;        // порция в миллисекундах
int indicateDelay = 1000; //делей индикации работы, моргания светодиодом в миллисекундах
int fRotateDelay = 500;   // делей работы моторчика вперед в миллисекундах
int bRotateDelay = 100;   // делей работы моторчика назад в миллисекундах
int tempDelay = 60000;    // делей замера температуры в миллисекундах
int feedTime1 = 7;        // время первого кормления
int feedTime2 = 17;       // время второго кормления
const int buttonPin = 5;  // button input//выход D1
int buttonState = 0;      // переменная для хранения состояния кнопки
int ledPin = 14;          //выход D5

timerMinim portionTimer(portion);
timerMinim fRotateTimer(fRotateDelay);
timerMinim bRotateTimer(bRotateDelay);
timerMinim tempTimer(tempDelay);
timerMinim indicateTimer(indicateDelay);
// timerMinim gameTimer(DEMO_GAME_SPEED);
// effectTimer.setInterval(effects_speed);
// effectTimer.isReady();
WiFiUDP ntpUDP;
WiFiClient WiFiclient;
MQTTClient client;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200);

unsigned long lastMillis = 0;
unsigned long feedMillis = 0;
// String wifiMode;
void setup()
{
  dht.begin(); // запускаем датчик влажности DHT11
  Serial.begin(9600);
  delay(10);
  Serial.println();
  Serial.println();

  // rtcObject.Begin();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  IPAddress ip(192, 168, 31, 210);
  IPAddress gateway(192, 168, 31, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(192,168,31,1);
  WiFi.config(ip, gateway, subnet, dns);
  delay(5000);
  Serial.println("begin NTP");
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("connecting to MQTT broker...");
  client.begin("192.168.31.200", WiFiclient);
  client.onMessage(messageReceived);
  connect();

  // инициализируем пин, подключенный к кнопке, как вход
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT); // установка 2-го контакта в режим вывода
  // wifiMode = WiFi.getMode();
  // start the WiFi OTA library with internal (flash) based storage
  ArduinoOTA.begin();
  
  
  timeClient.begin();
    // NTP.setTimeZone (TZ_Europe_Kiev);
    // NTP.begin ();
}

void connect()
{
  client.connect("feeder_test", mqttName, mqttPassword);
  Serial.println("\nconnected!");
  client.subscribe("/feeder/feed");
  client.subscribe("/feeder/portion");
  client.subscribe("/feeder/firstFeedTime");
  client.subscribe("/feeder/secondFeedTime");
}

void setPortion(String newPortion)
{
  Serial.println("setPortion to" + newPortion);
  portion = newPortion.toInt();
  Serial.println("Portion setted to : " + (String)portion);
}
void setFeedTime(int count, String rawTime)
{
  Serial.println("setFeedTime");
  if (count == 1)
  {
    feedTime1 = rawTime.toInt();
    Serial.println("Firt feed time setted to : " + (String)feedTime1 + " hours ");
  }
  if (count == 2)
  {
    feedTime2 = rawTime.toInt();
    Serial.println("Second feed time setted to : " + (String)feedTime2 + " hours ");
  }
}
void messageReceived(String &topic, String &payload)
{
  Serial.println("incoming: " + topic + " - " + payload);
  if (topic == "/feeder/feed" && payload == "ON")
  {
    feedStart();
  }
  if (topic == "/feeder/portion")
  {
    setPortion(payload);
  }
  if (topic == "/feeder/firstFeedTime")
  {
    setFeedTime(1, payload);
  }
  if (topic == "/feeder/secondFeedTime")
  {
    setFeedTime(2, payload);
  }
}
void loop()
{
  // check for WiFi OTA updates
  ArduinoOTA.handle();
  //  Serial.println("loop");
  // считываем значения с входа кнопки
  buttonState = digitalRead(buttonPin);
  // если нажата, то buttonState будет HIGH:
  if (buttonState == HIGH)
  {
    //кормим
    feedStart();
  }
  // int val = analogRead(A0);
  
  client.loop();
  if (!client.connected())
  {
    connect();
  }
  if (tempTimer.isReady())
  {
    // считываем температуру (t) и влажность (h) каждые 250 мс
    h = dht.readHumidity();
    t = dht.readTemperature();
    client.publish("/kitchen/humidity/in", (String)h);
    client.publish("/kitchen/temp/in", (String)t);
  }
  if (indicateTimer.isReady())
  { // если прошла 1 секунда
  timeClient.update();
    lastMillis = millis();
    workIndicate(); //моргаем диодом показывая работу
    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    // client.publish("/feeder/test/wifi", wifiMode);
    String NTPtime = timeClient.getFormattedTime();
    
    client.publish("/feeder/test", (String)hours);
    if (hours == feedTime1 && minutes == 1 || hours == feedTime2 && minutes == 1)
    {
      if (millis() - feedMillis > 65000)
      {
        feedMillis = millis();
        Serial.println("It's feed time!");
        feedStart();
      }
    }
  }
}
void workIndicate()
{
  digitalWrite(ledPin, HIGH); //моргаем диодом показывая работу
  client.publish("/feeder/status/in", "ON");
  delay(100);
  digitalWrite(ledPin, LOW);
}
void feedStart()
{
  myservo.attach(13); //атачим серво на 13 пин
  digitalWrite(ledPin, HIGH);
  client.publish("/feeder/feed/in", "ON");
  myservo.write(150);
  delay(portion);
  digitalWrite(ledPin, LOW);
  myservo.write(20);
  client.publish("/feeder/feed/in", "OFF");
  Serial.println("feed stoped");
  delay(2000);      //пауза что бы серво успел занять позицию
  myservo.detach(); //детачим что бы не пинговал его зря
}
