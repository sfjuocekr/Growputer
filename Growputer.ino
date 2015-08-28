#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DS1307RTC.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <Wire.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>

#define DHTPIN 3
#define DHTTYPE DHT22
#define ONE_WIRE_BUS 5
#define TEMPERATURE_PRECISION 12
#define NTP_PACKET_SIZE 48
#define LOCALPORT 123

DHT dht(DHTPIN, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
IPAddress timeServer(132, 163, 4, 101);

float airHumids[10];
float airTemps[10];
float waterTemps[10];

byte mac[6] = { 0xBE, 0xEF, 0xED, 0xC0, 0xFF, 0xEE };
byte packetBuffer[NTP_PACKET_SIZE];

float deltaTime = 0;

unsigned char index = 0;

bool inited = false;
EthernetUDP udp;

void setup()
{
  Serial.begin(9600);

  while (!Serial);

  Serial.flush();
  Serial.println("Growputer v0.1 initializing. . .");
  Serial.println();

  initHardware();

  Serial.println("Init: Done!");
  Serial.println();
  
  Alarm.timerRepeat(60, printStats);
  
  Alarm.alarmRepeat(6, 0, 0, lampOn);
  Alarm.alarmRepeat(0, 0, 0, lampOff);

  if (hour() >= 6) lampOn();
  else lampOff();
  
  inited = true;
}

void initHardware()
{
  init_DHT22();
  init_DS18B20();

  if (init_W5100()) init_NTP();
  else setSyncProvider(RTC.get);
}

void init_DHT22()
{
  Serial.println("Init: DHT22");

  dht.begin();
}

void init_DS18B20()
{
  Serial.println("Init: DS18B20");

  sensors.begin();
}

bool init_W5100()
{
  Serial.println("Init: W5100");

  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("      Failed to configure ethernet using DHCP!");

    return false;
  }

  Serial.print("      IP address from DHCP: ");
  Serial.println(Ethernet.localIP());

  return true;
}

void read_DHT22()
{
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  while (isnan(humidity) || isnan(temperature))
  {
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
  }

  airHumids[index] = humidity;
  airTemps[index] = temperature;
}

void read_DS18B20()
{
  sensors.requestTemperatures();

  waterTemps[index] = sensors.getTempCByIndex(0);
}

void readTime()
{
  Serial.print("");
  print2digits(hour());
  Serial.print(":");
  print2digits(minute());
  Serial.print(":");
  print2digits(second());
}

void readDate()
{
  Serial.print("");
  print2digits(day());
  Serial.print("-");
  print2digits(month());
  Serial.print("-");
  print2digits(year());
}

void print2digits(int number)
{
  if (number >= 0 && number < 10) Serial.print("0");

  Serial.print(number);
}

void init_NTP()
{
  Serial.println("Init: NTP");

  udp.begin(LOCALPORT);

  setSyncProvider(getNTP);
}

time_t getNTP()
{
  while (udp.parsePacket() > 0);

  Serial.println("      Transmitting NTP request. . .");

  sendNTPpacket(timeServer);

  unsigned int beginWait = millis();

  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();

    if (size >= NTP_PACKET_SIZE)
    {
      Serial.println("      Received NTP response!");

      udp.read(packetBuffer, NTP_PACKET_SIZE);

      unsigned long secsSince1900;

      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      RTC.set(secsSince1900 - 2208988800UL);

      return secsSince1900 - 2208988800UL;
    }
  }

  Serial.println("      No NTP response, using RTC value instead!");

  return RTC.get();
}

void sendNTPpacket(IPAddress &address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void read_sensors()
{
  unsigned long currentTime = millis();

  read_DHT22();
  read_DS18B20();

  deltaTime += (millis() - currentTime);

  index++;

  if (index == 10)
  {
    index = 1;

    airHumids[0]  = (airHumids[0]  + airHumids[1]  + airHumids[2]  + airHumids[3]  + airHumids[4]  + airHumids[5]  + airHumids[6]  + airHumids[7]  + airHumids[8]  + airHumids[9])  / 10;
    airTemps[0]   = (airTemps[0]   + airTemps[1]   + airTemps[2]   + airTemps[3]   + airTemps[4]   + airTemps[5]   + airTemps[6]   + airTemps[7]   + airTemps[8]   + airTemps[9])   / 10;
    waterTemps[0] = (waterTemps[0] + waterTemps[1] + waterTemps[2] + waterTemps[3] + waterTemps[4] + waterTemps[5] + waterTemps[6] + waterTemps[7] + waterTemps[8] + waterTemps[9]) / 10;

    deltaTime = 0;
  }
}

void printStats()
{
  Serial.print("Air humidity: ");
  Serial.println(airHumids[0]);
  Serial.print("Air temperature: ");
  Serial.println(airTemps[0]);
  Serial.print("Water temperature: ");
  Serial.println(waterTemps[0]);
  Serial.print("Timestamp: ");
  readDate();
  Serial.print(" ");
  readTime();
  Serial.println();
  Serial.println();
}

void lampOn()
{
  Serial.println("Lamp: ON");
  Serial.println();
}

void lampOff()
{
  Serial.println("Lamp: OFF");
  Serial.println();
}

void loop()
{
  if (inited)
  {
    read_sensors();
    
    Alarm.delay(1);
  }
}
