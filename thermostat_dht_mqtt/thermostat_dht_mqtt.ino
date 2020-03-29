// Thermostat / DHT temp sensor to graphite via the Ethernet shield

#include "DHT.h"
#include <SPI.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include <Ethernet.h>
#include <EthernetClient.h>
#include <Dns.h>
#include <Dhcp.h>

#define DHTPIN 2     // what pin we're connected to

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// HVAC defines
#define HEATPIN 4
#define COOLPIN 5
#define FANPIN  6

// Connect pin 1 (on the left) of the sensor to +5V (red)
// Connect pin 2 of the sensor to whatever your DHTPIN is (yellow)
// Connect pin 4 (on the right) of the sensor to GROUND (black)
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
DHT dht(DHTPIN, DHTTYPE);

#define LOC 0   // basement
// #define LOC 1   // crawlspace
#if LOC
#define LOCATION         "crawlspace"
const byte mac[] PROGMEM = { 0x90, 0xA2, 0xDA, 0x0F, 0x03, 0xA3 };
#else
#define LOCATION         "basement"
const byte mac[] PROGMEM = { 0x90, 0xA2, 0xDA, 0x0E, 0x30, 0xF5 };
#endif


#define MQTT_SERVER      "192.168.1.31"
#define MQTT_SERVERPORT  1883
#define MQTT_USERNAME    ""
#define MQTT_KEY         ""
#define MQTT_RETRIES     3

// Initialize the Ethernet client library
EthernetClient client;

// Have we sent the status packet (setup for the will packet)
byte status_sent = 0;
// HVAC Mode (0 = off, 1 = heat, 2 = cool)
byte hvac_mode = 0;
float cur_temp = 0;
float set_temp = 0;
bool fan_on   = false;
bool cool_pin = false;
bool heat_pin = false;
byte mqtt_loop;

// Initialize Adafruit_MQTT
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);
// Publish temp/humidity
Adafruit_MQTT_Publish temp = Adafruit_MQTT_Publish(&mqtt, "house/temperature/" LOCATION);
Adafruit_MQTT_Publish humidity = Adafruit_MQTT_Publish(&mqtt, "house/humidity/" LOCATION);
Adafruit_MQTT_Publish stat = Adafruit_MQTT_Publish(&mqtt, "house/status/" LOCATION);
// Setup HVAC
Adafruit_MQTT_Publish hvac_set_mode = Adafruit_MQTT_Publish(&mqtt, "house/hvac/set_mode");
Adafruit_MQTT_Subscribe mqtt_get_mode = Adafruit_MQTT_Subscribe(&mqtt, "house/hvac/get_mode");
Adafruit_MQTT_Subscribe mqtt_cur_temp = Adafruit_MQTT_Subscribe(&mqtt, "house/temperature/frontroom");
Adafruit_MQTT_Subscribe mqtt_set_temp = Adafruit_MQTT_Subscribe(&mqtt, "house/hvac/set_temp");
Adafruit_MQTT_Subscribe mqtt_set_fan = Adafruit_MQTT_Subscribe(&mqtt, "house/hvac/fan");

void setup() {
  Serial.begin(9600);
  Serial.println(F("starting DHT.."));
  dht.begin();

  // start the Ethernet connection:
  Serial.println("starting Ethernet..");
  Ethernet.begin(mac);

  // give the Ethernet shield a second to initialize:
  delay(1000);
}

void loop() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h)) {
    Serial.println(F("Failed to read from DHT"));
  }
  else {
    Serial.print(F("Temp: "));
    Serial.print(t);
    Serial.print(F("\tHumidity: "));
    Serial.print(h);
    Serial.println(F("%"));
    // Ensure that the connection to the MQTT broker is alive
    if (MQTT_connect()) {

      if (!status_sent) {
        // Init
        // house/status/LOCATION = true
        // house/hvac/mode = 0
        if (! stat.publish("true") && hvac_set_mode.publish("0") ) {
          Serial.println(F("Stat / mode publish failed"));
        }
        else {
          Serial.println(F("house/status/"LOCATION" -> MQTT"));
          status_sent = 1;
        }
      }
      // Publish temp via MQTT
      if (! temp.publish(t)) {
        Serial.println(F("Temp Failed.. "));
      }
      else {
        Serial.println(F("house/temperature/"LOCATION" -> MQTT"));
      }

      if (! humidity.publish(h)) {
        Serial.println(F("Humidity Failed.. "));
      }
      else {
        Serial.println(F("house/humidity/"LOCATION" -> MQTT"));
      }


      // MQTT Loop
      for (mqtt_loop = 0; mqtt_loop < 60; mqtt_loop++) {
        Adafruit_MQTT_Subscribe *subscription;
        while (subscription = mqtt.readSubscription(1000)) {
          if (subscription == &mqtt_get_mode) {
            // Mode
            Serial.print(F("MQTT -> house/hvac/get_mode: "));
            Serial.println((char *)mqtt_get_mode.lastread);
            // Mode change
            if (mqtt_get_mode.lastread != hvac_mode) {
              // Off
              if (mqtt_get_mode.lastread == 0) {
                if (cool_pin != false) {
                  digitalWrite(HEATPIN, LOW);
                  cool_pin = false;
                }
                if (heat_pin != false) {
                  digitalWrite(COOLPIN, LOW);
                  cool_pin = false;
                }
              } // END OFf
              // Heat
              if (mqtt_get_mode.lastread == 1) {
                // not needed?
              } // END Heat
              hvac_mode = mqtt_get_mode.lastread;
            } // END Mode Change
          }
          else if (subscription == &mqtt_cur_temp) {
            // Current temp
            Serial.print(F("MQTT -> home/temperature/frontroom: "));
            Serial.println((char *)mqtt_cur_temp.lastread);
            cur_temp = atof(mqtt_cur_temp.lastread);
          }
          else if (subscription == &mqtt_set_temp) {
            // Desired temp
            Serial.print(F("MQTT -> home/hvac/set_temp: "));
            Serial.println((char *)mqtt_set_temp.lastread);
            set_temp = atof(mqtt_set_temp.lastread);
          }
          else if (subscription == &mqtt_set_fan) {
            // Fan
            Serial.print(F("MQTT -> home/hvac/fan: "));
            Serial.println((char *)mqtt_set_fan.lastread);
            if (fan_on != (int)mqtt_set_fan.lastread) {
              fan_on = (int)mqtt_set_fan.lastread;
              if (fan_on == 0) {
                digitalWrite(FANPIN, LOW);
              }
              else {
                digitalWrite(FANPIN, HIGH);
              }
            }
          }
        }
        delay(1000);
      } // End of MQTT Loop
      run_hvac();
    }
  }
  // Renew DHCP if needed
  Ethernet.maintain();

  // Sleep for 1 sec
  delay(1000);
}


// HVAC Control
void run_hvac() {
  // Run heat?
  switch (hvac_mode) {
    case 0:
      // Off
      Serial.print(F("hvac_mode: "));
      if (heat_pin == true) {
        Serial.println(F("heat -> off"));
        digitalWrite(HEATPIN, LOW);
        heat_pin = false;
      }
      if (cool_pin == true) {
        Serial.println(F("cool -> off"));
        digitalWrite(COOLPIN, LOW);
        cool_pin = false;
      }
    case 1:
      // Heat
      Serial.print(F("hvac_mode: heat: "));
      if (cur_temp < set_temp) {
        Serial.print(cur_temp);
        Serial.print(F(" < "));
        Serial.print(set_temp);
        Serial.println(F("; "));
        digitalWrite(HEATPIN, HIGH);
        heat_pin = true;
      }
      else {
        Serial.print(cur_temp);
        Serial.print(F(" > "));
        Serial.print(set_temp);
        if (heat_pin == true) {
          Serial.println(F("; off"));
          digitalWrite(HEATPIN, LOW);
          heat_pin = false;
        }
        else {
          Serial.println(F("; on"));
        }
      }
      break;
    case 2:
      // Cool
      Serial.print(F("hvac_mode: cool: cur("));
      if (cur_temp > set_temp) {
        Serial.print(cur_temp);
        Serial.print(F(") > set("));
        Serial.print(set_temp);
        Serial.println(F("); "));
        digitalWrite(COOLPIN, HIGH);
        cool_pin = true;
      }
      else {
        Serial.print(cur_temp);
        Serial.print(F(") < set("));
        Serial.print(set_temp);
        if (cool_pin == true) {
          Serial.println(F("); off"));
          digitalWrite(HEATPIN, LOW);
          cool_pin = false;
        }
        else {
          Serial.println(F("); on"));
        }
      }
      break;
  }
}


// Function to connect and reconnect as necessary to the MQTT server.
int MQTT_connect() {
  int8_t ret;
  int8_t loop = 0;

  // Stop if already connected.
  if (mqtt.connected()) {
    return 1;
  }

  Serial.print("Connecting to MQTT... ");

  // Set will
  // will(const char *topic, const char *payload, uint8_t qos, uint8_t retain)
  mqtt.will("house/status/"LOCATION, "false", 1, 1);

  while ((ret = mqtt.connect()) != 0 && loop <= MQTT_RETRIES) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println(F("Retrying MQTT connection in 5 seconds..."));
    mqtt.disconnect();
    loop++;
    delay(5000);  // wait 5 seconds
  }
  if (ret == 0) {
    Serial.println(F("MQTT Connected!"));
  }

  return 0;
}
