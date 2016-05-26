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
#include <ArduinoJson.h>

/*
 RTC = SDA 4a
 SCL 5a
 
 DTH0 = 2d
 DHT1 = 3d
 SD = 4d
 Relay0 = 5d
 Relay1 = 6d
 Relay2 = 7d
 Relay3 = 8d
 W5100 = 10d, keep high is also for DS18x20 power!
 OneWire = 12d
*/

#define DHT0_PIN     2
#define DHT1_PIN     3
#define SD_PIN       4 // DO NOT CHANGE
#define PUMP_PIN     5
#define FINT_PIN     6
#define FEXT_PIN     7
#define LIGHT_PIN    8
#define ETH_PIN     10 // DO NOT CHANGE
#define ONEWIRE_PIN 12 // DO NOT CHANGE

#define DHT0_TYPE DHT22
#define DHT1_TYPE DHT22
#define TEMPERATURE_PRECISION 12
#define NTP_PACKET_SIZE 48
#define NTP_PORT 123
#define HTTP_PORT 80

DHT dht0(DHT0_PIN, DHT0_TYPE);
DHT dht1(DHT1_PIN, DHT1_TYPE);

OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

IPAddress timeServer(5, 200, 6, 34);
EthernetServer server(HTTP_PORT);
byte mac[6] = { 0xBE, 0xEF, 0xED, 0xC0, 0xFF, 0xEE };
byte packetBuffer[NTP_PACKET_SIZE];
EthernetUDP udp;

DynamicJsonBuffer jsonBuffer;
JsonObject& sensor_data = jsonBuffer.createObject();

void setup()
{
  Serial.begin(9600);
  Serial.flush();
  Serial.print("Growputer 2016: ");

  if (initHardware())
  {
    read_DS18x20();
    read_DHT();

    Alarm.timerRepeat(1, read_DS18x20);        // 0
    Alarm.timerRepeat(2, read_DHT);            // 1
    
    Alarm.alarmRepeat(6, 0, 0, setState);      // 2
    sensor_data["startTime"] = Alarm.read(2);
    
    Alarm.alarmRepeat(0, 0, 0, setState);      // 3
    sensor_data["endTime"] = Alarm.read(3);

    if (hour() >= 6) setState(LIGHT_PIN, true);
    else setState(LIGHT_PIN, false);
    
    Serial.println("0");
  }
  else
  {
    Serial.println("1");

    // enable some timer to check if stuff automagicly worked again?

    while (1);
  }
}

bool initHardware()
{
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FINT_PIN, OUTPUT);
  pinMode(FEXT_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);

  setState(PUMP_PIN, true);
  setState(FINT_PIN, true);
  setState(FEXT_PIN, false);
  setState(LIGHT_PIN, false);
    
  setSyncProvider(RTC.get);

  if (init_W5100())
  {
    sensor_data["IP"] = IPToString(Ethernet.localIP());
    
    RTC.set(getNTP());

    server.begin();
  }

  int _dht = init_DHT();
  if (_dht != -1)
  {
    Serial.println("2" + _dht);

    return false;
  }

  if (!init_DS18x20())
  {
    Serial.println("3");

    return false;
  }

  return true;
}

int init_DHT()
{
  dht0.begin();
  dht1.begin();

  if (isnan(dht0.readTemperature(false)))
    return 0;

  if (isnan(dht0.readTemperature(false)))
    return 1;
  
  return -1;
}

bool init_DS18x20()
{
  sensors.begin();

  if (sensors.getDeviceCount() == 0) return false;
  else return true;
}

bool init_W5100()
{
  pinMode(SD_PIN, OUTPUT);
  pinMode(ETH_PIN, OUTPUT);

  digitalWrite(SD_PIN, LOW); // SD off
  digitalWrite(ETH_PIN, HIGH); // ETH on

  if (Ethernet.begin(mac) == 0) return false;
  else return true;
}

String IPToString(IPAddress _ip)
{
  String _s = "";
  
  for (int _i = 0; _i < 4; _i++)
    _s += _i ? "." + String(_ip[_i]) : String(_ip[_i]);
    
  return _s;
}

void read_DHT()
{  
  sensor_data["dht0_h"] = dht0.readHumidity();
  sensor_data["dht0_t"] = dht0.readTemperature(false);
  sensor_data["dht1_h"] = dht1.readHumidity();
  sensor_data["dht1_t"] = dht1.readTemperature(false);
}

void read_DS18x20()
{
  sensors.requestTemperatures();
  
  sensor_data["water_t"] = sensors.getTempCByIndex(0);
}

String readTime()
{
  return return2digits(hour());
}

String readDate()
{
  return return2digits(day());
}

time_t getNTP()
{
  udp.begin(NTP_PORT);

  while (udp.parsePacket() > 0);

  sensor_data["NTP"] = -1;

  sendNTPpacket(timeServer);

  unsigned int beginWait = millis();

  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();

    if (size >= NTP_PACKET_SIZE)
    {
      sensor_data["NTP"] = 1;
      
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

  sensor_data["NTP"] = 0;
  
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

void listenServer()
{
  EthernetClient client = server.available();

  if (client)
  {
    bool currentLineIsBlank = true;

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();

        if (c == '\n' && currentLineIsBlank)
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Access-Control-Allow-Origin: *");
          client.println("Content-Type: application/json");
          client.println("Connection: close");
          client.println("Refresh: 1");
          client.println();
          sensor_data["time"] = RTC.get();
          sensor_data.printTo(client);
          client.println();

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
  Serial.print("\ndht0_h: ");
  sensor_data["dht0_h"].printTo(Serial);
  Serial.print(" dht0_t: ");
  sensor_data["dht0_t"].printTo(Serial);
  Serial.print("\ndht1_h: ");
  sensor_data["dht1_h"].printTo(Serial);
  Serial.print(" dht1_t: ");
  sensor_data["dht1_t"].printTo(Serial);
  Serial.print("\nwater_t: ");
  sensor_data["water_t"].printTo(Serial);
  Serial.print("\ntime: ");
  Serial.print(return2digits(day()));
  Serial.print("-");
  Serial.print(return2digits(month()));
  Serial.print("-");
  Serial.print(return2digits(year()));
  Serial.print(" ");
  Serial.print(return2digits(hour()));
  Serial.print(":");
  Serial.print(return2digits(minute()));
  Serial.print(":");
  Serial.println(return2digits(second()));
}

void setState()
{
  int alarmID = Alarm.getTriggeredAlarmId();
  
  switch (alarmID)
  {
    case 2:
      setState(LIGHT_PIN, true);
      break;

    case 3:
      setState(LIGHT_PIN, false);
      break;

    default:
      Serial.println("TIMER: " + Alarm.getTriggeredAlarmId());
      break;
  }
}

void setState(int _name, bool _state)
{
  switch (_name)
  {
    case PUMP_PIN:
      sensor_data["PUMP"] = _state;
      digitalWrite(PUMP_PIN, !_state);
      break;
    
    case FINT_PIN:
      sensor_data["FINT"] = _state;
      digitalWrite(FINT_PIN, !_state);
      break;

    case FEXT_PIN:
      sensor_data["FEXT"] = _state;
      digitalWrite(FEXT_PIN, !_state);
      break;

    case LIGHT_PIN:
      sensor_data["LIGHT"] = _state;
      digitalWrite(LIGHT_PIN, !_state);
      break;
  }
}

void setState(String _data)
{
  _data = _data.substring(1, _data.length());
  
  if (_data.substring(0, _data.length() - 1) == "PUMP")
    setState(PUMP_PIN, (_data.charAt(_data.length() - 1) == '1'));
  if (_data.substring(0, _data.length() - 1) == "FINT")
    setState(FINT_PIN, (_data.charAt(_data.length() - 1) == '1'));
  if (_data.substring(0, _data.length() - 1) == "FEXT")
    setState(FEXT_PIN, (_data.charAt(_data.length() - 1) == '1'));
  if (_data.substring(0, _data.length() - 1) == "LIGHT")
    setState(LIGHT_PIN, (_data.charAt(_data.length() - 1) == '1'));
}

void setAlarms(int _start, int _end)
{
  sensor_data["startTime"] = _start;
  sensor_data["endTime"] = _end;
  
  Alarm.write(2, sensor_data["startTime"]);
  Alarm.write(3, sensor_data["endTime"]);
}

void setAlarms(bool _type, int _time)
{
  if (_type)
  {
    sensor_data["startTime"] = _time;
    Alarm.write(2, sensor_data["startTime"]);
  }
  else
  {
    sensor_data["endTime"] = _time;
    Alarm.write(3, sensor_data["endTime"]);
  }
}

void setAlarms(String _data)
{
  int _start = _data.indexOf("s");
  int _end = _data.indexOf("e");
  
  Serial.println((String)_start + " " + (String)_end);
  
  if ((_start > -1) && (_end == -1))
  {
    setAlarms((bool)true, (int)_data.substring(1, _start).toInt());
    Serial.println("start: " + _data.substring(1, _start));
    return;
  }
  
  if ((_end > -1) && (_start == -1))
  {
    setAlarms((bool)false, (int)_data.substring(1, _end).toInt());
    Serial.println("end: " + _data.substring(1, _end));
    return;
  }
  
  if ((_start > -1) && (_end > -1) && (_end < _start))
  {
    setAlarms((int)_data.substring(_end + 1, _start).toInt(), (int)_data.substring(1, _end).toInt());
    Serial.println("start: " + _data.substring(_end + 1, _start));
    Serial.println("end: " + _data.substring(1, _end));
    return;
  }
  
  if ((_start > -1) && (_end > -1) && (_end > _start))
  {
    setAlarms((int)_data.substring(1, _start).toInt(), (int)_data.substring(_start + 1, _end).toInt());
    Serial.println("start: " + _data.substring(1, _start));
    Serial.println("end: " + _data.substring(_start + 1, _end));
    return;
  }
  
  //setAlarms((int)_data.substring(1, _data.indexOf("s")).toInt(), (int)_data.substring(_data.indexOf("s") + 1, _data.indexOf("e")).toInt());
}

void serialEvent()
{
  if (Serial.available())
  {
    String _serialString = Serial.readString();
           _serialString.trim();
    
    switch (_serialString.charAt(0))
    {
      case 48: printStats(); break;
      case 49: setAlarms(_serialString); break;
      case 50: setState(_serialString); break;
    }
  }
}

void loop()
{
  Alarm.delay(0);

  listenServer();
}

