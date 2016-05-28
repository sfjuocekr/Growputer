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
#define SD_PIN        4
#define PUMP_PIN      5
#define FINT_PIN      6
#define FEXT_PIN      7
#define LIGHT_PIN     8
#define ONEWIRE_PIN   9
#define ETH_PIN      10
#define DHT0_TYPE DHT22
#define DHT1_TYPE DHT22
#define TEMPERATURE_PRECISION 12
#define NTP_PACKET_SIZE 48
#define NTP_PORT 123
#define HTTP_PORT 80

String IPToString(IPAddress _ip);
String readDate();
String readSettings();
String readTime();
String return2digits(int number);
boolean initHardware();
boolean init_DS18x20();
boolean loadSettings();
boolean saveSettings();
void listenServer();
void logStats();
void loop();
void printAlarms();
void printStates();
void printStats();
void printTime();
void read_DHT();
void read_DS18x20();
void sendNTPpacket(IPAddress &address);
void serialEvent();
void setAlarm(boolean _type, unsigned long _time);
void setAlarms(String _data);
void setAlarms(unsigned long _start, unsigned long _end);
void setState();
void setState(String _data);
void setState(int _name, boolean _state);
void setup();
void showPorn();
void syncTime();
void watchdog();
unsigned long seconds();

time_t getNTP();
boolean initHardware();
byte init_W5100();
byte init_DHT();
boolean init_DS18x20();
