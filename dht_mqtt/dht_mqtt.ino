// DHT temp sensor to graphite via the Ethernet shield

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

// Connect pin 1 (on the left) of the sensor to +5V (red)
// Connect pin 2 of the sensor to whatever your DHTPIN is (yellow)
// Connect pin 4 (on the right) of the sensor to GROUND (black)
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
DHT dht(DHTPIN, DHTTYPE);

#define LOC 0   // basement
#define LOC 1   // crawlspace
#if LOC
#define LOCATION         "crawlspace"
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0x03, 0xA3 };
#else
#define LOCATION         "basement"
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0x30, 0xF5 };
#endif


#define MQTT_SERVER      "192.168.1.31"
#define MQTT_SERVERPORT  1883
#define MQTT_USERNAME    ""
#define MQTT_KEY         ""
#define MQTT_RETRIES     3

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 23 is default for telnet;
// if you're using Processing's ChatServer, use  port 10002):
EthernetClient client;

// Have we sent the status packet (setup for the will packet)
uint32_t status_sent = 0;

// Initialize Adafruit_MQTT
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);
// Publish temp/humidity
Adafruit_MQTT_Publish temp = Adafruit_MQTT_Publish(&mqtt, MQTT_USERNAME "/house/temp/crawlspace");
Adafruit_MQTT_Publish humidity = Adafruit_MQTT_Publish(&mqtt, MQTT_USERNAME "/house/humidity/crawlspace");


void setup() {
  Serial.begin(9600);
  Serial.println("starting DHT..");
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
    Serial.println("Failed to read from DHT");
  }
  else {
    // Ensure that the connection to the MQTT broker is alive
    MQTT_connect();

    // Publish via MQTT
    Serial.print("Publishing.. ");
    if (! temp.publish(t)) {
      Serial.print("Temp Failed.. ");
    }
    else {
      Serial.print("Temp OK! ");
    }
    if (! humidity.publish(h)) {
      Serial.print("Humidity Failed.. ");
    }
    else {
      Serial.print("Humidity OK! ");
    }
    Serial.println();
  }

  // Renew DHCP if needed
  Ethernet.maintain();

  // Sleep for 60s
  delay(60000);
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
  mqtt.will("house/status/" LOCATION, "false", 1, 1);

  while ((ret = mqtt.connect()) != 0 && loop <= MQTT_RETRIES) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       loop++;
       delay(5000);  // wait 5 seconds
  }
  if(ret == 0) {
    Serial.println("MQTT Connected!");
  }

  return 0;
}
