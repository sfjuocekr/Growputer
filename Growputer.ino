#include <Arduino.h>

void setup()
{
    sensors();
    set_time();
}

void sensors()
{
    init_DHT22();
    init_DS18B20();
    init_DS1307();

    init_W5100();

    // connected?
    // connected = true
    setup_MySQL();
    set_time();

    // inserted?
    // inserted = true
    setup_SQLite();
}

void init_DHT22()
{
    // initialize air humidity and air temperature sensor
}

void init_DS18B20()
{
    // initialize water temperature sensor
}

void init_DS1307()
{
    // initialize RTC
}

void init_W5100()
{
    // initialize ethernet shield

    // return connected, inserted
}

void setup_MySQL()
{
    // initialize MySQL connection to server
}

void write_MySQL()
{
    // sent data to database
}

void setup_SQLite()
{
    // initialize SQLite on SD
}

void write_SQLite()
{
    //write data to database
}

void read_DHT22()
{
    // return air himidity and air temperature
}

void read_DS18B20()
{
    // return water temperature
}

void read_DS1307()
{
    // return RTC data
}

void write_DS1307()
{
    // write RTC
}

void set_time()
{
    init_DS1307();

    write_DS1307(NTP());
}

void NTP()
{
    // return time from nl.pool.ntp.org
}

void read_sensors()
{
    read_DHT22();
    read_DS18B20();
    read_DS1307();

    // connected?
    write_MySQL();

    // inserted?
    write_SQLite();

    // delay one minute
    delay(60 * 1000);
}

void loop()
{
    read_sensors();
}
