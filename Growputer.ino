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
EthernetServer server(80);

float airHumids[10];
float airTemps[10];
float waterTemps[10];

byte mac[6] = { 0xBE, 0xEF, 0xED, 0xC0, 0xFF, 0xEE };
byte packetBuffer[NTP_PACKET_SIZE];

unsigned char index = 0;

bool inited = false;
EthernetUDP udp;

void setup()
{
  Serial.begin(9600);

  Serial.println("Growputer v0.1 initializing");
  Serial.println();

  if (initHardware())
  {
    Serial.println("Init: Done!");
    Serial.println();

    Alarm.timerRepeat(2, read_sensors);
    Alarm.timerRepeat(60, printStats);

    Alarm.alarmRepeat(6, 0, 0, lampOn);
    Alarm.alarmRepeat(0, 0, 0, lampOff);

    if (hour() >= 6) lampOn();
    else lampOff();

    inited = true;
  }
  else
  {
    Serial.println("Init: Failed!");

    while (1);
  }
}

bool initHardware()
{
  if (!init_DHT22()) return false;
  if (!init_DS18B20()) return false;

  init_DS1307();

  if (init_W5100())
  {
    init_NTP();

    server.begin();
  }

  return true;
}

bool init_DHT22()
{
  Serial.println("Init: DHT22");

  dht.begin();

  if (isnan(dht.readTemperature(false))) return false;
  else return true;
}

bool init_DS18B20()
{
  Serial.println("Init: DS18B20");

  sensors.begin();

  if (sensors.getDeviceCount() == 0) return false;
  else return true;
}

void init_DS1307()
{
  Serial.println("Init: DS1307");

  setSyncProvider(RTC.get);
}

bool init_W5100()
{
  Serial.println("Init: W5100");

  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  if (Ethernet.begin(mac) == 0) return false;
  else
  {
    Serial.print("      IP address from DHCP: ");
    Serial.println(Ethernet.localIP());

    return true;
  }
}

void read_DHT22()
{
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature(false);

  while (isnan(humidity) || isnan(temperature))
  {
    Serial.println("NAN");

    humidity = dht.readHumidity();
    temperature = dht.readTemperature(false);
  }

  airHumids[index] = humidity;
  airTemps[index] = temperature;
}

void read_DS18B20()
{
  sensors.requestTemperatures();

  waterTemps[index] = sensors.getTempCByIndex(0);
}

String readTime()
{
  return return2digits(hour());
  /*Serial.print(return2digits(hour()));
  Serial.print(":");
  Serial.print(return2digits(minute()));
  Serial.print(":");
  Serial.print(return2digits(second()));*/
}

String readDate()
{
  return return2digits(day());
  /*Serial.print(return2digits(day()));
  Serial.print("-");
  Serial.print(return2digits(month()));
  Serial.print("-");
  Serial.print(year());*/
}

void init_NTP()
{
  Serial.println("Init: NTP");

  RTC.set(getNTP());
}

time_t getNTP()
{
  udp.begin(LOCALPORT);

  while (udp.parsePacket() > 0);

  Serial.println("NTP:  Transmitting NTP request");

  sendNTPpacket(timeServer);

  unsigned int beginWait = millis();

  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();

    if (size >= NTP_PACKET_SIZE)
    {
      Serial.println("NTP:  Received NTP response!");

      udp.read(packetBuffer, NTP_PACKET_SIZE);

      unsigned long secsSince1900;

      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      udp.stop();

      return secsSince1900 - 2208988800UL;
    }
  }

  Serial.println("NTP:  No response, using RTC value instead!");

  udp.stop();

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

  index++;

  if (index == 10)
  {
    index = 1;

    airHumids[0]  = (airHumids[0]  + airHumids[1]  + airHumids[2]  + airHumids[3]  + airHumids[4]  + airHumids[5]  + airHumids[6]  + airHumids[7]  + airHumids[8]  + airHumids[9])  / 10;
    airTemps[0]   = (airTemps[0]   + airTemps[1]   + airTemps[2]   + airTemps[3]   + airTemps[4]   + airTemps[5]   + airTemps[6]   + airTemps[7]   + airTemps[8]   + airTemps[9])   / 10;
    waterTemps[0] = (waterTemps[0] + waterTemps[1] + waterTemps[2] + waterTemps[3] + waterTemps[4] + waterTemps[5] + waterTemps[6] + waterTemps[7] + waterTemps[8] + waterTemps[9]) / 10;
  }
}

void listen_server()
{
  EthernetClient client = server.available();

  if (client)
  {
    boolean currentLineIsBlank = true;

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();

        if (c == '\n' && currentLineIsBlank)
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println("Refresh: 5");
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.print("Air humidity: ");
          client.println(airHumids[0]);
          client.print("<br>Air temperature: ");
          client.println(airTemps[0]);
          client.print("<br>Water temperature: ");
          client.println(waterTemps[0]);
          client.print("<br><br>Timestamp: ");
          client.print(return2digits(day()));
          client.print("-");
          client.print(return2digits(month()));
          client.print("-");
          client.print(return2digits(year()));
          client.print(" ");
          client.print(return2digits(hour()));
          client.print(":");
          client.print(return2digits(minute()));
          client.print(":");
          client.println(return2digits(second()));
          client.println("</html>");

          break;
        }

        if (c == '\n') currentLineIsBlank = true;
        else if (c != '\r') currentLineIsBlank = false;
      }
    }

    client.stop();
  }
}

String return2digits(int number)
{
  if (number >= 0 && number < 10) return "0" + String(number);

  return String(number);
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
  Serial.print(readDate());
  Serial.print(" ");
  Serial.print(readTime());
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
  Alarm.delay(0);

  listen_server();
}

