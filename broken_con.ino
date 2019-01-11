
#include <SignalMQTT.h>
#include <AssetTracker.h>
#include <OneWire.h>
#include <DS18B20.h>
#include <math.h>
#include "application.h"
#include "MQTT.h"


// #############################################################
// Project Config
// Set Particle keep-alive ping interval.
// Each ping uses 121 bytes of data.
#ifndef TOKEN
#define TOKEN "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6IjdkYzUzNjgzLTljMTgtNGYyYS04Mzg3LWM0ODE1MTk5M2NjMyJ9.EAEUEwx6BiCEqsO9PnHeLNSI8LLwdQZMUfqzg79Be64"
#endif
#define PARTICLE_KEEPALIVE 20
// #############################################################

const int      MAXRETRY          = 4;
const uint32_t msSAMPLE_INTERVAL = 2500;
const uint32_t msMETRIC_PUBLISH  = 30000;

DS18B20  ds18b20(D2, true); //Sets Pin D2 for Water Temp Sensor and this is the only sensor on bus
char     szInfo[64];
double   celsius;
double   fahrenheit;
uint32_t msLastMetric;
uint32_t msLastSample;

// gps #####################################################
void callback(char* topic, byte* payload, unsigned int length);

// Track last time coordinates were published
long lastPublish = 0;

// How many milliseconds between publishes
int delayMillis = 10000;
AssetTracker t = AssetTracker();

byte PPServer[] = {128, 199, 157, 0 };
MQTT client(PPServer, 1883, callback);
// Define a client connection name 'signal'
MySignal signal(TOKEN, callback);



void callback(char* topic, uint8_t* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
}

void setup() {
  t.begin();
  t.gpsOn();
  // connect to the server
  client.connect("mqtt_broken_fish", "ppsmart", "ppmqtt");
  // publish/subscribe
  if (client.isConnected()) {
      client.publish("homeassistant/temp_room3","mqtt_broken_fish Connected.");
      client.subscribe("homeassistant/temp_room3");
    }
  Serial.begin(9600);

  // Initialize the connection to Signal
  signal.initialize();

  Particle.variable("tempHotWater", &celsius, DOUBLE);

  Particle.keepAlive(PARTICLE_KEEPALIVE);

}

void loop() {
  if (client.isConnected())
  client.loop();

  if (millis() - msLastSample >= msSAMPLE_INTERVAL){
    getTemp();
  }

  if (millis() - msLastMetric >= msMETRIC_PUBLISH){
    Serial.println("Publishing now.");
    publishData();
  }
  getGPS();
}

void publishData(){
  Particle.publish("celsius", String(celsius), PRIVATE);
  sprintf(szInfo, "%2.2f", fahrenheit);
  Particle.publish("fahrenheit", szInfo, PRIVATE);
  msLastMetric = millis();
  delay(5000);
}

void getTemp(){
  float _temp;
  int   i = 0;

  do {
    _temp = ds18b20.getTemperature();
  } while (!ds18b20.crcCheck() && MAXRETRY > i++);

  if (i < MAXRETRY) {
    celsius = _temp;
    fahrenheit = ds18b20.convertToFahrenheit(_temp);
  }
  else {
    celsius = fahrenheit = NAN;
    Serial.println("Invalid reading");
  }
  Serial.println("celsius : "+ String(celsius) + "°C");
  client.publish("homeassistant/temp_room3","celsius : "+ String(celsius) + "°C");
  Serial.println("fahrenheit : "+ String(fahrenheit) + "°F");
  client.publish("homeassistant/temp_room3","fahrenheit : "+ String(fahrenheit) + "°F");
  delay(5000);
  msLastSample = millis();
}

void getGPS(){
    // Reconnect if connection to Signal was lost
    if(!signal.isConnected()){
    signal.connect();
    }

    t.updateGPS();

    // Delay the loop
    if (millis()-lastPublish > delayMillis) {
    lastPublish = millis();

    if (t.gpsFix()) {
        if (signal.isConnected()) {
            // Publish coordinates
            signal.publishCoordinates(t.readLatLon());
            }
        }
        Serial.println(t.readLatLon());
        client.publish("homeassistant/temp_room3","GPS(Lat,Lon) : "+ t.readLatLon());
    }

    // Maintain the connection to Signal if connected
    if (signal.isConnected()){
        signal.loop();
    }
}
