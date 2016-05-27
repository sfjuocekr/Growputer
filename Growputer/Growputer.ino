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
#include <SD.h>
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
 OneWire = 9d
*/

#define DHT0_PIN      2
#define DHT1_PIN      3
#define SD_PIN        4 // DO NOT CHANGE
#define PUMP_PIN      5
#define FINT_PIN      6
#define FEXT_PIN      7
#define LIGHT_PIN     8
#define ONEWIRE_PIN   9
#define ETH_PIN      10 // DO NOT CHANGE

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
JsonObject& json_data = jsonBuffer.createObject();

void setup()
{
  Serial.begin(9600);
  Serial.flush();
  
  while (!Serial) {}
  
  Serial.print("Growputer 2016: ");

  if (initHardware())
  {
    read_DS18x20();
    read_DHT();

    Alarm.timerRepeat(1, read_DS18x20);        // 0
    Alarm.timerRepeat(2, read_DHT);            // 1
    
    Alarm.alarmRepeat(6, 0, 0, setState);      // 2
    Alarm.alarmRepeat(0, 0, 0, setState);      // 3

    if (loadSettings())
      Alarm.timerRepeat(60, logStats);         // 4
    
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

  setState(PUMP_PIN, false);
  setState(FINT_PIN, false);
  setState(FEXT_PIN, false);
  setState(LIGHT_PIN, false);

  setSyncProvider(RTC.get);

  if (init_W5100())
  {
    json_data["IP"] = IPToString(Ethernet.localIP());
    
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
  if (!SD.begin(SD_PIN)) return false;
  
  if (Ethernet.begin(mac) == 0) return false;
  else return true;
}

bool loadSettings()
{
  DynamicJsonBuffer _json;
  JsonObject& _data = _json.parseObject(readSettings());
  
  if (_data.success())
  {
    json_data["startTime"] = (unsigned long)_data["startTime"];
    json_data["endTime"] = (unsigned long)_data["endTime"];
    
    setAlarm(true, (unsigned long)_data["startTime"]);
    setAlarm(false, (unsigned long)_data["endTime"]);

    setState(PUMP_PIN, (bool)_data["PUMP"]);    
    setState(FINT_PIN, (bool)_data["FINT"]);    
    setState(FEXT_PIN, (bool)_data["FEXT"]);    
    
    if (seconds() >= json_data["startTime"]) setState(LIGHT_PIN, true);
      else setState(LIGHT_PIN, false);

    return true;
  }
  
  return false;
}

String readSettings()
{
  if (!SD.exists("SETTINGS.420"))
    saveSettings();
  
  File settingsFile = SD.open("SETTINGS.420", FILE_READ);
  
  if (settingsFile)
  {
    String _json;
    
    while (settingsFile.available())
    {
      _json += (String)(char)settingsFile.read();
    }
    
    settingsFile.close();
    
    return _json;
  }
  
  settingsFile.close();
}

bool saveSettings()
{
  if (SD.exists("SETTINGS.420"))
    SD.remove("SETTINGS.420");
    
  File settingsFile = SD.open("SETTINGS.420", FILE_WRITE);
  
  if (settingsFile)
  {
    json_data.printTo(settingsFile);
    
    settingsFile.close();
    
    return true;
  }

  settingsFile.close();
  
  return false;
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
  json_data["dht0_h"] = dht0.readHumidity();
  json_data["dht0_t"] = dht0.readTemperature(false);
  json_data["dht1_h"] = dht1.readHumidity();
  json_data["dht1_t"] = dht1.readTemperature(false);
}

void read_DS18x20()
{
  sensors.requestTemperatures();
  
  json_data["water_t"] = sensors.getTempCByIndex(0);
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

  json_data["NTP"] = -1;

  sendNTPpacket(timeServer);

  unsigned int beginWait = millis();

  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();

    if (size >= NTP_PACKET_SIZE)
    {
      json_data["NTP"] = 1;
      
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

  json_data["NTP"] = 0;
  
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
          json_data["time"] = RTC.get();
          json_data.printTo(client);
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
  Serial.print("HUMID0:\t");
  json_data["dht0_h"].printTo(Serial);
  Serial.print("\tTEMP0:\t");
  json_data["dht0_t"].printTo(Serial);
  Serial.print("\nHUMID1:\t");
  json_data["dht1_h"].printTo(Serial);
  Serial.print("\tTEMP1:\t");
  json_data["dht1_t"].printTo(Serial);
  Serial.print("\nWATER:\t");
  json_data["water_t"].printTo(Serial);
  Serial.println();
}

void logStats()
{
  File logFile = SD.open("LOG.420", FILE_WRITE);
  
  if (logFile)
  {
    DynamicJsonBuffer _json;
    JsonObject& _data = _json.createObject();
  
    _data["time"] = (unsigned long)RTC.get();
    _data["dht0_h"] = (float)json_data["dht0_h"];
    _data["dht0_t"] = (float)json_data["dht0_t"];
    _data["dht1_h"] = (float)json_data["dht1_h"];
    _data["dht1_t"] = (float)json_data["dht1_t"];
    _data["water_t"] = (float)json_data["water_t"];
    _data["PUMP"] = json_data["PUMP"];
    _data["FINT"] = json_data["FINT"];
    _data["FEXT"] = json_data["FEXT"];
    _data["LIGHT"] = json_data["LIGHT"];
    _data.printTo(logFile);
    _data.printTo(Serial);
    
    logFile.println();
    logFile.close();
  }
}

void printStates()
{
  Serial.print("PUMP: ");
  json_data["PUMP"].printTo(Serial);
  Serial.print("\tFINT: ");
  json_data["FINT"].printTo(Serial);
  Serial.print("\tFEXT: ");
  json_data["FEXT"].printTo(Serial);
  Serial.print("\tLIGHT: ");
  json_data["LIGHT"].printTo(Serial);
  Serial.println();
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

void setState(int _name, bool _state)
{
  switch (_name)
  {
    case PUMP_PIN:
      json_data["PUMP"] = _state;
      digitalWrite(PUMP_PIN, !_state);
      saveSettings();
      break;
    
    case FINT_PIN:
      json_data["FINT"] = _state;
      digitalWrite(FINT_PIN, !_state);
      saveSettings();
      break;

    case FEXT_PIN:
      json_data["FEXT"] = _state;
      digitalWrite(FEXT_PIN, !_state);
      saveSettings();
      break;

    case LIGHT_PIN:
      json_data["LIGHT"] = _state;
      digitalWrite(LIGHT_PIN, !_state);
      saveSettings();
      break;
  }
}

void printAlarms()
{
  Serial.print("ON: ");
  json_data["startTime"].printTo(Serial);
  Serial.print("\tOFF: ");
  json_data["endTime"].printTo(Serial);
  Serial.println();
}

void setAlarms(String _data)
{
  int _start = _data.indexOf("s");
  int _end = _data.indexOf("e");
    
  if ((_start > -1) && (_end == -1))
  {
    setAlarm(true, (unsigned long)_data.substring(1, _start).toInt());
    return;
  }
  
  if ((_end > -1) && (_start == -1))
  {
    setAlarm(false, (unsigned long)_data.substring(1, _end).toInt());
    return;
  }
  
  if ((_start > -1) && (_end > -1) && (_end < _start))
  {
    setAlarms((unsigned long)_data.substring(_end + 1, _start).toInt(), (unsigned long)_data.substring(1, _end).toInt());
    return;
  }
  
  if ((_start > -1) && (_end > -1) && (_end > _start))
  {
    setAlarms((unsigned long)_data.substring(1, _start).toInt(), (unsigned long)_data.substring(_start + 1, _end).toInt());
    return;
  }
}

void setAlarms(unsigned long _start, unsigned long _end)
{
  json_data["startTime"] = _start;
  json_data["endTime"] = _end;
  
  Alarm.write(2, json_data["startTime"]);
  Alarm.write(3, json_data["endTime"]);
  
  saveSettings();
}

void setAlarm(bool _type, unsigned long _time)
{
  if (_type)
  {
    json_data["startTime"] = _time;
    Alarm.write(2, json_data["startTime"]);
  }
  else
  {
    json_data["endTime"] = _time;
    Alarm.write(3, json_data["endTime"]);
  }
  
  saveSettings();
}

void printTime()
{   
  Serial.println("SECONDS: " + (String)seconds());
  
  Serial.print("TIME: ");
  Serial.print(return2digits(hour()));
  Serial.print(":");
  Serial.print(return2digits(minute()));
  Serial.print(":");
  Serial.print(return2digits(second()));

  Serial.print("\tDATE: ");
  Serial.print(return2digits(day()));
  Serial.print("-");
  Serial.print(return2digits(month()));
  Serial.print("-");
  Serial.println(return2digits(year()));
}

unsigned long seconds()
{
  unsigned long _time = hour();
                _time *= 3600;
                _time += minute() * 60;
                _time += second();

  return _time;
}

void serialEvent()
{
  if (Serial.available())
  {
    String _serialString = Serial.readString();
           _serialString.trim();
    
    switch (_serialString.charAt(0))
    {
      case 48: printStats();                             // 0
               printStates();
               printAlarms();
               printTime();
               break;
      case 49: printStats(); break;                            // 1
      case 50: printStates(); break;                           // 2
      case 51: printAlarms(); break;                           // 3
      case 52: printTime(); break;                             // 4
      case 53: setState(_serialString); break;                 // 5
      case 54: setAlarms(_serialString); break;                // 6
      case 55: break;                                          // 7
      case 56: loadSettings(); break;                          // 8
      case 57: saveSettings(); break;                          // 9
      default: Serial.println("NOT IMPLEMENTED"); break;
    }
  }
}

void loop()
{
  Alarm.delay(0);

  listenServer();
}

