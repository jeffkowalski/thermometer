#include <Arduino.h>
#include "credentials.h"
#include <DallasTemperature.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

// Data wire is plugged TO GPIO 4
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas
// temperature ICs)
OneWire oneWire (ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors (&oneWire);

// Number of temperature devices found
int numberOfDevices = 0;

#define SERVER "cube.local:8086"

class Blinker {
    const int           led = D4;
    unsigned long       previousMillis;   // will store last time LED was updated
    const unsigned long interval = 1000;  // interval at which to blink (milliseconds)

  public:
    Blinker() : previousMillis (0) {
        pinMode (led, OUTPUT);
    }

    void blink() {
        digitalWrite (led, LOW);
        delay (100);
        digitalWrite (led, HIGH);
        delay (100);
    }

    void update() {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            //        Serial.println("blink");
            blink();
            previousMillis = currentMillis;
        }
    }
} Blinker;

void stringifyAddress (DeviceAddress deviceAddress, char * deviceAddressString) {
    sprintf (deviceAddressString,
             "%02x%02x%02x%02x%02x%02x%02x%02x",
             deviceAddress[7],
             deviceAddress[6],
             deviceAddress[5],
             deviceAddress[4],
             deviceAddress[3],
             deviceAddress[2],
             deviceAddress[1],
             deviceAddress[0]);
}

void setup() {
    // start serial port
    Serial.begin (115200);

    // Start up the library
    sensors.begin();
    // locate devices on the bus
    numberOfDevices = sensors.getDeviceCount();
    Serial.printf ("Found %d devices\n", numberOfDevices);

    // Loop through each device, print out address
    for (int ii = 0; ii < numberOfDevices; ii++) {
        // Search the wire for address
        DeviceAddress deviceAddress;
        if (sensors.getAddress (deviceAddress, ii)) {
            char deviceAddressString[17];
            stringifyAddress (deviceAddress, deviceAddressString);
            Serial.printf ("Found device %d with address 0x%s\n", ii, deviceAddressString);
        } else {
            Serial.printf ("Found ghost device at %d but could not detect address. Check power and cabling\n", ii);
        }
    }

    Serial.print ("Connecting WiFi");
    WiFi.begin (WIFI_SSID, WIFI_PSK);  // defined in credentials.h
    WiFi.waitForConnectResult();       // so much neater than those stupid loops and dots
    Serial.println (WiFi.localIP());

    Blinker.blink();
}

void record_to_database (int device_id, char const * device_address, float tempF) {
    WiFiClient client;
    HTTPClient http;

    Serial.print ("[HTTP] begin...\n");
    http.begin (client, "http://" SERVER "/write?db=thermometer");
    http.addHeader ("Accept", "*/*");
    http.addHeader ("Content-Type", "application/json");

    Serial.print ("[HTTP] POST...\n");
    static char postval[256];
    sprintf (postval, "temperature,device_id=%d,device_address=0x%s value=%f", device_id, device_address, tempF);
    Serial.println (postval);
    int httpCode = http.POST (postval);

    // httpCode will be negative on error
    if (httpCode > 0)
        Serial.printf ("[HTTP] POST... code: %d\n", httpCode);
    else
        Serial.printf ("[HTTP] POST... failed, error: %s\n", http.errorToString (httpCode).c_str());

    http.end();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED)
        ESP.restart();

    sensors.requestTemperatures();  // Send the command to get temperatures
    Blinker.blink();

    // Loop through each device, print out temperature data
    for (int ii = 0; ii < numberOfDevices; ii++) {
        // Search the wire for address
        DeviceAddress deviceAddress;
        if (sensors.getAddress (deviceAddress, ii)) {
            char deviceAddressString[17];
            stringifyAddress (deviceAddress, deviceAddressString);
            Serial.printf ("Temperature of device %d with address 0x%s\n", ii, deviceAddressString);
            float tempC = sensors.getTempC (deviceAddress);
            float tempF = DallasTemperature::toFahrenheit (tempC);
            Serial.printf ("Temperature: %f C, %f F\n", tempC, tempF);

            record_to_database (ii, deviceAddressString, tempF);
        }
    }
    delay (5000);
}
