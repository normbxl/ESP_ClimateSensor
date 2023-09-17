#include <Arduino.h>
ADC_MODE(ADC_VCC)
#include <ESP8266WiFi.h>
#include <AdafruitIO_WiFi.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "config.h"
#include <EEPROM.h>
#include "user_interface.h"

#define LED_ONBOARD			2

#define LIGHT_SLEEP_PERIOD_ms  600e3
#define DEEP_SLEEP_PERIOD_uS  600e6

#define DHTPIN 0 // ESP01 = 0, RC-board= 14     // Digital pin connected to the DHT sensor 
#define DHTTYPE    DHT11     // DHT 11
#define EEPROM_MAGIC_HDR 248726
DHT_Unified dht(DHTPIN, DHTTYPE);

AdafruitIO_WiFi* io;

AdafruitIO_Feed *temperature;
AdafruitIO_Feed *humidity;
AdafruitIO_Feed *battery;

typedef struct {
  int hdr;
  char ssid[40];
  char pwd[40];
} wifi_data_t;

wifi_data_t wifi_data;

bool connected=false;

void readSensorAndSend();

void initGPIO() {
	pinMode(LED_ONBOARD, OUTPUT);
	digitalWrite(LED_ONBOARD, HIGH);
}


void toggleLed() {
	int ledState = !digitalRead(LED_ONBOARD);
	digitalWrite(LED_ONBOARD, ledState);
}

void wakeupHandle() {
  connected=false;
  Serial.println("Wake up");
}
void sleep() {
  Serial.println("going to sleep");
  connected=false;
  if (!WiFi.mode(WIFI_OFF)) {
    Serial.println("Failed to set WiFi.mode(WIFI_OFF)");
    while(1);
  }
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
  digitalWrite(LED_ONBOARD, 0);
  if (WiFi.forceSleepBegin(LIGHT_SLEEP_PERIOD_ms*1000)) {
    delay(LIGHT_SLEEP_PERIOD_ms);
  }
  
}

bool runProvisioning() {
  bool done=false;
  memset(&wifi_data, 0, sizeof(wifi_data_t));

  unsigned long timeout=millis()+50000;
  digitalWrite(LED_ONBOARD, HIGH);
  Serial.setTimeout(50000);
  EEPROM.begin(sizeof(wifi_data_t));
  Serial.println("\n==Provisioning Mode==");
  Serial.println("Commands (one per line):");
  Serial.println(" ssid=<WIFI SSID>");
  Serial.println(" pwd=<WIFI PASSWORD>");
  Serial.println(" save   Save WiFi configuration.");
  String str="";
  while (!done) {
    str="";
    while (millis()<timeout) {
      if (Serial.available()) {
        timeout=millis()+50000;
        char c=(char)Serial.read();
        // echo read value
        Serial.print(c);
        if (c=='\n')
          break;
        if (c==0x08 && str.length() > 0) {
          str.remove(str.length()-1, 1);
        }
        else if (c!='\r') 
          str.concat(c);
      }
    }
    if (!str.isEmpty()) {
      int colPos = str.indexOf("=");
      if (colPos > 0) {
        String cmd = str.substring(0, colPos);
        cmd.toLowerCase();
        String value = str.substring(colPos+1);
        
        if (value.endsWith("\r")) {
          value.remove(value.length()-2,1);
        }
        if (cmd == "ssid") {
          value.toCharArray(wifi_data.ssid, sizeof(wifi_data.ssid));
          wifi_data.hdr = EEPROM_MAGIC_HDR;
          Serial.println("OK");
        }
        if (cmd == "pwd") {
          value.toCharArray(wifi_data.pwd, sizeof(wifi_data.pwd));
          wifi_data.hdr = EEPROM_MAGIC_HDR;
          Serial.println("OK");
        }
      }
      if (str.startsWith("save")) {
        EEPROM.put<wifi_data_t>(0, wifi_data);
        if (EEPROM.commit()) {
          Serial.println("OK");
          Serial.print("ssid=");
          Serial.println(wifi_data.ssid);
          Serial.print("pwd=");
          Serial.println(wifi_data.pwd);
          done=true;
        }
        else {
          Serial.println("ERROR");
        }
      }
      Serial.flush();
    }
    else {
      break;
    }
  }
  EEPROM.end();
  digitalWrite(LED_ONBOARD, LOW);
  return done;
}

void connectWifi() {
  int wifi_timeout;
  initGPIO();
  EEPROM.begin(sizeof(wifi_data_t));
  EEPROM.get<wifi_data_t>(0, wifi_data);
  EEPROM.end();

  if (wifi_data.hdr != EEPROM_MAGIC_HDR) {
    runProvisioning();
  }

  while (!connected) {

    io = new AdafruitIO_WiFi(IO_USERNAME, IO_KEY, wifi_data.ssid, wifi_data.pwd);

    Serial.print("SSID: ");
    Serial.println(wifi_data.ssid);
    
    // connect to io.adafruit.com
    Serial.print("Connecting to Adafruit IO");
    io->connect();
    wifi_timeout=10000;
    // wait for a connection
    while(io->status() < AIO_CONNECTED) {
      Serial.print(".");

      if (wifi_timeout<=0) {
        // shut down if provisioning fails
        if (!runProvisioning()) {
          sleep();
        }
        else {
          break;
        }
      }
      wifi_timeout -= 500;
      delay(500);
    } 
    connected= (io->status() == AIO_CONNECTED);
  }  
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(2000);
  Serial.println("=== ESP Temperature and Humidity Monitor ===");

  
 
  connectWifi();
  temperature = io->feed("temperature");
  humidity = io->feed("humidity");
  battery = io->feed("battery");
  
}

void readSensorAndSend() {
  sensors_event_t event;
  bool success=false;
  float vcc;
  io->run();
  digitalWrite(LED_ONBOARD, HIGH);
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  }
  else {
    Serial.print("Temperature: ");
    Serial.print(event.temperature);
    Serial.println("°C");
    temperature->save(event.temperature);
    success=true;
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  }
  else {
    Serial.print("Humidity: ");
    Serial.print(event.relative_humidity);
    Serial.println("%");
    humidity->save(event.relative_humidity);
  }
  
  vcc = (float)ESP.getVcc() / 1000.f;
  battery->save(vcc);

  Serial.flush();
  io->run();
  io->wifi_disconnect();
  if (!success) {
    for (int i=0; i<10; i++) {
      toggleLed();
      delay(200);
    }
  }
  digitalWrite(LED_ONBOARD, LOW);
  
  
}
void loop() {
  if (!connected) {
    
    connectWifi();
  }
  Serial.println();
  Serial.println(io->statusText());
  dht.begin();

  readSensorAndSend();

  sleep();
}