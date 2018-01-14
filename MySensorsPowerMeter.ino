#define MY_DEBUG 
#define MY_GATEWAY_MQTT_CLIENT
#define MY_GATEWAY_ESP8266
#define MY_ESP8266_HOSTNAME "PowerMeterSensor"

#define MY_CONTROLLER_IP_ADDRESS 192, 168, 88, 99
#define MY_PORT 1883

// Set this nodes subscripe and publish topic prefix
#define MY_MQTT_PUBLISH_TOPIC_PREFIX "MySensorsOut"
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX "MySensorsOut"

// Set MQTT client id
#define MY_MQTT_CLIENT_ID "PowerMeterSensor"

// Set WIFI SSID and password
#define MY_ESP8266_SSID ""
#define MY_ESP8266_PASSWORD ""

#include <EthernetClient.h>
#include <Ethernet.h>
#include <Dhcp.h>
#include <EthernetServer.h>
#include <Dns.h>
#include <EthernetUdp.h>
#include <ESP8266WiFi.h>
#include <MySensors.h>

#define PULSE_FACTOR 1000       // Nummber of blinks per KWH of your meeter
#define MAX_WATT 10000          // Max watt value to report. This filetrs outliers.
#define CHILD_ID_WATT 150
#define CHILD_ID_KWH 151
#define CHILD_ID_PULSE 152

int sensorPin = 5; //D1 on wemos

uint32_t SEND_FREQUENCY =
    20000; // Minimum time between send (in milliseconds). We don't wnat to spam the gateway.
double ppwh = ((double)PULSE_FACTOR)/1000; // Pulses per watt hour
bool pcReceived = false;
volatile uint32_t pulseCount = 0;
volatile uint32_t lastBlink = 0;
volatile uint32_t watt = 0;
uint32_t oldPulseCount = 0;
uint32_t oldWatt = 0;
double oldKwh;
uint32_t lastSend;
MyMessage wattMsg(CHILD_ID_WATT,V_WATT);
MyMessage kwhMsg(CHILD_ID_KWH,V_KWH);
MyMessage pcMsg(CHILD_ID_PULSE,V_VAR1);

void setup()
{       
  // Fetch last known pulse count value from gw
  request(CHILD_ID_PULSE, V_VAR1);

  // Use the internal pullup to be able to hook up this sketch directly to an energy meter with S0 output
  // If no pullup is used, the reported usage will be too high because of the floating pin
  pinMode(sensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sensorPin), onPulse, RISING);

  lastSend=millis();
}

void presentation()
{
  // Register this device as power sensor
  present(CHILD_ID_WATT, S_POWER);
  present(CHILD_ID_KWH, S_POWER);
  present(CHILD_ID_PULSE, S_POWER);
}

void loop()
{
 
  uint32_t now = millis();
  // Only send values at a maximum frequency or woken up from sleep
  bool sendTime = now - lastSend > SEND_FREQUENCY;
  if (pcReceived && sendTime) {
    // New watt value has been calculated
    if (watt != oldWatt) {
      // Check that we dont get unresonable large watt value.
      // could hapen when long wraps or false interrupt triggered
      if (watt<((uint32_t)MAX_WATT)) {
        send(wattMsg.set(watt));  // Send watt value to gw
      }
      Serial.print("Watt:");
      Serial.println(watt);
      oldWatt = watt;
    }

    // Pulse cout has changed
    if (pulseCount != oldPulseCount) {
      send(pcMsg.set(pulseCount));  // Send pulse count value to gw
      double kwh = ((double)pulseCount/((double)PULSE_FACTOR));
      oldPulseCount = pulseCount;
      if (kwh != oldKwh) {
        send(kwhMsg.set(kwh, 4));  // Send kwh value to gw
        oldKwh = kwh;
      }
    }
    lastSend = now;
  } else if (sendTime && !pcReceived) {
    // No count received. Try requesting it again
    request(CHILD_ID_PULSE, V_VAR1);
    lastSend=now;
  }
}

void receive(const MyMessage &message)
{
  if (message.type==V_VAR1) {
    pulseCount = oldPulseCount = message.getLong();
    Serial.print("Received last pulse count from gw:");
    Serial.println(pulseCount);
    pcReceived = true;
  }
}

void onPulse()
{
    uint32_t newBlink = micros();
    uint32_t interval = newBlink-lastBlink;
    if (interval<10000L) { // Sometimes we get interrupt on RISING
      return;
    }
    watt = (3600000000.0 /interval) / ppwh;
    lastBlink = newBlink;
  
    pulseCount++;
}

