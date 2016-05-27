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
#include <Timezone.h>

#define DHT0_PIN      2
#define DHT1_PIN      3
#define SD_PIN        4 // FIXED, DO NOT CHANGE
#define PUMP_PIN      5
#define FINT_PIN      6
#define FEXT_PIN      7
#define LIGHT_PIN     8
#define ONEWIRE_PIN   9
#define ETH_PIN      10 // FIXED, DO NOT CHANGE

// PIN's 11, 12 and 13 are used by the SD card!

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
byte mac[6] = { 
  0xBE, 0xEF, 0xED, 0xC0, 0xFF, 0xEE };
byte packetBuffer[NTP_PACKET_SIZE];
EthernetUDP udp;

DynamicJsonBuffer jsonBuffer;
JsonObject& jsonData = jsonBuffer.createObject();

TimeChangeRule DST = {
  "DST", Second, Sun, Mar, 3, 120};
TimeChangeRule STD = {
  "STD", Second, Sun, Nov, 3, 180};
Timezone TZ(DST, STD);

void setup()
{
  Serial.begin(9600);
  Serial.flush();

  while (!Serial) {
  }

  Serial.print("Growputer 2016: ");

  if (initHardware())
  {
    read_DS18x20();
    read_DHT();

    Alarm.timerRepeat(1, read_DS18x20);        // 0
    Alarm.timerRepeat(2, read_DHT);            // 1

    Alarm.alarmRepeat(6, 0, 0, setState);      // 2
    Alarm.alarmRepeat(0, 0, 0, setState);      // 3
    Alarm.timerRepeat(0, 0, 0, syncTime);      // 4

    if (loadSettings())
      Alarm.timerRepeat(60, logStats);         // 5

    Alarm.timerRepeat(1, watchdog);            // 6

    Serial.println("0");
  }
  else
  {
    Serial.println("1");

    // enable some timer to check if stuff automagicly worked again?

    while (1);
  }
}

boolean initHardware()
{
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FINT_PIN, OUTPUT);
  pinMode(FEXT_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);

  setState(PUMP_PIN, true);
  setState(FINT_PIN, true);
  setState(FEXT_PIN, false);
  setState(LIGHT_PIN, false);

  byte _tmp = 0;

  _tmp = init_W5100();
  if (_tmp <= 1)
  {
    jsonData["IP"] = (String)IPToString(Ethernet.localIP());

    RTC.set(getNTP());

    server.begin();
  }
  else Serial.print("2" + (String)_tmp);

  if (RTC.chipPresent())
    setSyncProvider(RTC.get);
  else if (_tmp <= 1)
    setSyncProvider(getNTP);
  else return false;

  _tmp = init_DHT();
  if (_tmp != 0)
  {
    Serial.print("3" + (String)_tmp);

    return false;
  }

  if (!init_DS18x20())
  {
    Serial.print("4");

    return false;
  }

  return true;
}

String IPToString(IPAddress _ip)
{
  String _IP = "";

  for (int i = 0; i < 4; i++)
    _IP += i ? "." + String(_ip[i]) : String(_ip[i]);

  return _IP;
}

byte init_W5100()
{
  byte _state = 0;

  if (!SD.begin(SD_PIN)) _state += 1;  
  if (Ethernet.begin(mac) == 0) _state += 2;

  return _state;
}

byte init_DHT()
{
  dht0.begin();
  dht1.begin();

  if (isnan(dht0.readTemperature(false)))
    return 1;

  if (isnan(dht0.readTemperature(false)))
    return 2;

  return 0;
}

boolean init_DS18x20()
{
  sensors.begin();

  if (sensors.getDeviceCount() == 0) return false;
  else return true;
}

boolean loadSettings()
{
  DynamicJsonBuffer _json;
  JsonObject& _data = _json.parseObject(readSettings());

  if (_data.success())
  {
    setAlarm(true, (unsigned long)_data["startTime"]);
    setAlarm(false, (unsigned long)_data["endTime"]);

    setState(PUMP_PIN, (boolean)_data["PUMP"]);    
    setState(FINT_PIN, (boolean)_data["FINT"]);    
    setState(FEXT_PIN, (boolean)_data["FEXT"]);
  }
  else
  {
    setAlarm(true, (unsigned long)21600);
    setAlarm(false, (unsigned long)0);

    setState(PUMP_PIN, (boolean)true);    
    setState(FINT_PIN, (boolean)true);    
    setState(FEXT_PIN, (boolean)false);
  }

  if (seconds() >= jsonData["startTime"]) setState(LIGHT_PIN, true);
  else setState(LIGHT_PIN, false);

  return _data.success();
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

boolean saveSettings()
{
  if (SD.exists("SETTINGS.420"))
    SD.remove("SETTINGS.420");

  File settingsFile = SD.open("SETTINGS.420", FILE_WRITE);

  if (settingsFile)
  {
    jsonData.printTo(settingsFile);

    settingsFile.close();

    return true;
  }

  settingsFile.close();

  return false;
}

void read_DHT()
{  
  jsonData["dht0_h"] = (float)dht0.readHumidity();
  jsonData["dht0_t"] = (float)dht0.readTemperature(false);
  jsonData["dht1_h"] = (float)dht1.readHumidity();
  jsonData["dht1_t"] = (float)dht1.readTemperature(false);
}

void read_DS18x20()
{
  sensors.requestTemperatures();

  jsonData["water_t"] = (float)sensors.getTempCByIndex(0);
}

void syncTime()
{
  RTC.set(getNTP());

  if (RTC.chipPresent())
    setSyncProvider(RTC.get);
  else
    setSyncProvider(getNTP);
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

  jsonData["NTP"] = (int)-1;

  sendNTPpacket(timeServer);

  unsigned int beginWait = millis();

  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();

    if (size >= NTP_PACKET_SIZE)
    {
      jsonData["NTP"] = (int)1;

      udp.read(packetBuffer, NTP_PACKET_SIZE);

      unsigned long secsSince1900;

      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      udp.stop();

      return TZ.toLocal((unsigned long)(secsSince1900 - 2208988800UL));

      //return secsSince1900 - 2208988800UL;
    }
  }

  jsonData["NTP"] = (int)0;

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
    boolean currentLineIsBlank = true;

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
          jsonData["time"] = (unsigned long)RTC.get();
          jsonData.printTo(client);
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
  Serial.print("HUMID0:\t ");
  jsonData["dht0_h"].printTo(Serial);
  Serial.print("\tTEMP0:\t");
  jsonData["dht0_t"].printTo(Serial);
  Serial.print("\nHUMID1:\t ");
  jsonData["dht1_h"].printTo(Serial);
  Serial.print("\tTEMP1:\t");
  jsonData["dht1_t"].printTo(Serial);
  Serial.print("\nWATER:\t ");
  jsonData["water_t"].printTo(Serial);
  Serial.println();
}

void logStats()
{
  char _fileName[13];

  (return2digits(day()) + return2digits(month()) + (String)year() + ".420").toCharArray(_fileName, sizeof(_fileName));

  File logFile = SD.open(_fileName, FILE_WRITE);

  if (logFile)
  {
    DynamicJsonBuffer _json;
    JsonObject& _data = _json.createObject();

    _data["time"] = (unsigned long)RTC.get();
    _data["dht0_h"] = (float)jsonData["dht0_h"];
    _data["dht0_t"] = (float)jsonData["dht0_t"];
    _data["dht1_h"] = (float)jsonData["dht1_h"];
    _data["dht1_t"] = (float)jsonData["dht1_t"];
    _data["water_t"] = (float)jsonData["water_t"];
    _data["PUMP"] = (boolean)jsonData["PUMP"];
    _data["FINT"] = (boolean)jsonData["FINT"];
    _data["FEXT"] = (boolean)jsonData["FEXT"];
    _data["LIGHT"] = (boolean)jsonData["LIGHT"];
    _data.printTo(logFile);

    logFile.println();
    logFile.close();
  }
}

void printStates()
{
  Serial.print("PUMP:\t ");
  jsonData["PUMP"].printTo(Serial);
  Serial.print("\tFINT:\t");
  jsonData["FINT"].printTo(Serial);
  Serial.print("\tFEXT:\t");
  jsonData["FEXT"].printTo(Serial);
  Serial.print("\tLIGHT:\t");
  jsonData["LIGHT"].printTo(Serial);
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
  _data.toUpperCase();
  
  if (_data.length() == 0)
  {
    Serial.println("USAGE: XXX1 for ON XXX0 OFF");
    return;
  }

  if (_data.substring(0, _data.length() - 1) == "PUMP")
    setState(PUMP_PIN, (_data.charAt(_data.length() - 1) == '1'));
  if (_data.substring(0, _data.length() - 1) == "FINT")
    setState(FINT_PIN, (_data.charAt(_data.length() - 1) == '1'));
  if (_data.substring(0, _data.length() - 1) == "FEXT")
    setState(FEXT_PIN, (_data.charAt(_data.length() - 1) == '1'));
  if (_data.substring(0, _data.length() - 1) == "LIGHT")
    setState(LIGHT_PIN, (_data.charAt(_data.length() - 1) == '1'));
}

void setState(int _name, boolean _state)
{
  switch (_name)
  {
  case PUMP_PIN:
    jsonData["PUMP"] = (boolean)_state;
    digitalWrite(PUMP_PIN, _state);
    saveSettings();
    break;

  case FINT_PIN:
    jsonData["FINT"] = (boolean)_state;
    digitalWrite(FINT_PIN, _state);
    saveSettings();
    break;

  case FEXT_PIN:
    jsonData["FEXT"] = (boolean)_state;
    digitalWrite(FEXT_PIN, !_state);
    saveSettings();
    break;

  case LIGHT_PIN:
    jsonData["LIGHT"] = (boolean)_state;
    digitalWrite(LIGHT_PIN, !_state);
    saveSettings();
    break;
  }
}

void printAlarms()
{
  Serial.print("ON:\t ");
  jsonData["startTime"].printTo(Serial);
  Serial.print("\t\tOFF:\t");
  jsonData["endTime"].printTo(Serial);
  Serial.println();
  
  Serial.print("\t " + return2digits(hour(jsonData["startTime"])));
  Serial.print(":" + return2digits(minute(jsonData["startTime"])));
  Serial.print(":" +return2digits(second(jsonData["startTime"])));

  Serial.print("\t\t" + return2digits(hour(jsonData["endTime"])));
  Serial.print(":" + return2digits(minute(jsonData["endTime"])));
  Serial.println(":" +return2digits(second(jsonData["endTime"])));
}

void setAlarms(String _data)
{
  int _start = _data.indexOf("s");
  int _end = _data.indexOf("e");

  if ((_start == -1) && (_end == -1))
  {
    Serial.println("USAGE: XXXs for LIGHT ON, XXXe for LIGHT OFF");
    return;
  }

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
  jsonData["startTime"] = (unsigned long)_start;
  jsonData["endTime"] = (unsigned long)_end;

  Alarm.write(2, jsonData["startTime"]);
  Alarm.write(3, jsonData["endTime"]);

  saveSettings();
}

void setAlarm(boolean _type, unsigned long _time)
{
  if (_type)
  {
    jsonData["startTime"] = (unsigned long)_time;
    Alarm.write(2, jsonData["startTime"]);
  }
  else
  {
    jsonData["endTime"] = (unsigned long)_time;
    Alarm.write(3, jsonData["endTime"]);
  }

  saveSettings();
}

void printTime()
{   
  Serial.println("SECONDS: " + (String)seconds());

  Serial.print("TIME:\t ");
  Serial.print(return2digits(hour()));
  Serial.print(":");
  Serial.print(return2digits(minute()));
  Serial.print(":");
  Serial.print(return2digits(second()));

  Serial.print("\tDATE:\t");
  Serial.print(return2digits(day()));
  Serial.print("-");
  Serial.print(return2digits(month()));
  Serial.print("-");
  Serial.println(year());
}

unsigned long seconds()
{
  unsigned long _time = hour();
  _time *= 3600;
  _time += minute() * 60;
  _time += second();

  return _time;
}

void watchdog()
{
  jsonData["RTC"] = (int)RTC.chipPresent();

  if (((float)jsonData["dht0_t"] > 26) && (!(boolean)jsonData["FEXT"]))
    if ((float)jsonData["dht0_h"] > 60)
      setState(FEXT_PIN, true);
    else if (((float)jsonData["dht0_t"] < 26) && ((boolean)jsonData["FEXT"]))
      if ((float)jsonData["dht0_h"] < 60)
        setState(FEXT_PIN, false);

  if (((float)jsonData["water_t"] > 20) && (!(boolean)jsonData["PUMP"]))
    setState(PUMP_PIN, true);
  else if (((float)jsonData["water_t"] < 20) && ((boolean)jsonData["PUMP"]))
    setState(PUMP_PIN, false);

  if (((float)jsonData["dht0_h"] > 60) && (!(boolean)jsonData["FINT"]))
    setState(FINT_PIN, true);
  else if (((float)jsonData["dht0_h"] < 50) && ((boolean)jsonData["FINT"]))
    setState(FINT_PIN, false);
}

void showPorn()
{
  if (!SD.exists("README.420"))
  {
    Serial.println("NO PORN AVAILABLE!");
    
    return;
  }
  
  File pornFile = SD.open("README.420", FILE_READ);

  if (pornFile)
  {
    String _pornLines;
    
    Serial.println();

    while (pornFile.available())
    {
      char _readChar = (char)pornFile.read();
      
      if (_readChar == '\n')
      {
        Serial.println((String)_pornLines);
        
        _pornLines = "";
      }
      else _pornLines += (String)_readChar;
    }
  }

  pornFile.close();
}

void serialEvent()
{
  if (Serial.available())
  {
    String _serialString = Serial.readString();
    _serialString.trim();

    switch (_serialString.charAt(0))
    {
    case 49: // 1
      printStats();
      printStates();
      printAlarms();
      printTime();
      break;
      
    case 50: // 2
      printStats(); 
      break;
      
    case 51: // 3
      printStates(); 
      break;
      
    case 52: // 4 
      printAlarms(); 
      break;
      
    case 53: // 5 
      printTime(); 
      break;
      
    case 54: // 6 
      setState(_serialString); 
      break;
      
    case 55: // 7 
      setAlarms(_serialString); 
      break;
      
    case 56: // 8
      saveSettings(); 
      break;
      
    case 57: // 9
      showPorn();
      break;
      
    case 48: // 0
      Serial.println("\n\t        ________________");
      Serial.println("  \t       /^^^^^^^^^^^^^^^^\\");
      Serial.println("  \t      /------------------\\");
      Serial.println("  \t     /--------------------\\");
      Serial.println("  \t    /----------------------\\");
      Serial.println("  \t   /------------------------\\");
      Serial.println("  \t  /--------------------------\\");
      Serial.println("  \t /____________________________\\");
      Serial.println("  \t |-----[ Growputer 2016 ]-----|");
      Serial.println(  "\t |^^^^^^^^^^^^^^^^^^^^^^^^^^^^|");
      Serial.println(  "\t |1: all stats                |");
      Serial.println(  "\t |2: humidity and temperature |");
      Serial.println(  "\t |3: relay states             |");
      Serial.println(  "\t |4: day/night timers         |");
      Serial.println(  "\t |5: time/date                |");
      Serial.println(  "\t |6: set relay state          |");
      Serial.println(  "\t |7: set timer(s)     ______  |");
      Serial.println(  "\t |8: save settings    |    |  |");
      Serial.println(  "\t |9: watch porn       |   _|  |");
      Serial.println(  "\t |0: this help        |    |  |");
      Serial.println(  "\t |____________________|____|__|\n");
      break;
      
    default: 
      Serial.println("NOT IMPLEMENTED"); 
      break;
    }
  }
}

void loop()
{
  Alarm.delay(0);

  listenServer();
}

