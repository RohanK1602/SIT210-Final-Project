#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "Adafruit_CCS811.h"
#include <WiFiNINA.h>
#include "ThingSpeak.h"
#include "arduino_secrets.h"

// Pin configuration for 5110 LCD
#define CLK_PIN 13 // Serial clock out (SCLK)
#define DIN_PIN 11 // Serial data out (DIN)
#define DC_PIN 9   // Data/Command select (D/C)
#define CS_PIN 10  // LCD chip select (CS)
#define RST_PIN 8  // LCD reset (RST)

// Create an instance of the Adafruit_PCD8544 class
Adafruit_PCD8544 display = Adafruit_PCD8544(CLK_PIN, DIN_PIN, DC_PIN, CS_PIN, RST_PIN);

// Create an instance of the Adafruit_CCS811 class
Adafruit_CCS811 ccs;

// WiFi credentials
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

// IFTTT
char HOST_NAME[] = "maker.ifttt.com";
String PATH_NAME = "MY KEY";  

// ThingSpeak
WiFiClient client;
unsigned long myChannelNumber = strtoul(SECRET_CH_ID, NULL, 10);
const char *myWriteAPIKey = SECRET_WRITE_APIKEY;

const uint16_t CO2_LIMIT = 1000; // Limit for CO2
const uint16_t TVOC_LIMIT = 500; // Limit for TVOC

unsigned long lastIFTTTTriggerTime = 10 * 60 * 1000; 
const unsigned long IFTTT_TRIGGER_INTERVAL = 10 * 60 * 1000;  

void setup() {
  Serial.begin(9600);

  if (!ccs.begin()) {
    Serial.println("Failed to start sensor! Please check your wiring.");
    while (1);
  }

  while (!ccs.available());

  display.begin();
  display.setContrast(50);
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.println("Air Quality");
  display.setCursor(0, 10);
  display.println("Monitor");
  display.display();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi.");

  if (testIFTTTConnection()) {
    display.setCursor(0, 20);
    display.println("IFTTT: ACTIVE");
  } else {
    display.setCursor(0, 20);
    display.println("IFTTT: FAILED");
  }

  ThingSpeak.begin(client);
  if (testThingSpeakConnection()) {
    display.setCursor(0, 30);
    display.println("TS: ACTIVE");
  } else {
    display.setCursor(0, 30);
    display.println("TS: FAILED");
  }

  display.display();
  delay(2000);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Ready");
  display.display();
}

void loop() {
  if (ccs.available()) {
    if (!ccs.readData()) {
      uint16_t co2 = ccs.geteCO2();
      uint16_t tvoc = ccs.getTVOC();

      display.clearDisplay();
      display.setCursor(0, 0);

      Serial.print("CO2: " + String(co2) + " ppm, ");
      Serial.println("TVOC: " + String(tvoc) + " ppb");

      display.println("CO2: " + String(co2) + " ppm");
      display.println("TVOC: " + String(tvoc) + " ppb");

      unsigned long currentTime = millis();
      if ((co2 > CO2_LIMIT || tvoc > TVOC_LIMIT) && (currentTime - lastIFTTTTriggerTime > IFTTT_TRIGGER_INTERVAL)) {
        display.setCursor(0, 20);
        display.println("Warning!");
        display.setCursor(0, 30);
        display.println("Poor Air Quality");

        display.display();

        Serial.println("Triggering IFTTT event");
        String iftttPath = PATH_NAME + "?value1=" + String(co2) + "&value2=" + String(tvoc);
        sendHttpRequest(iftttPath);
        lastIFTTTTriggerTime = currentTime; // Update the last trigger time
      }

      display.display();

      ThingSpeak.setField(1, co2);
      ThingSpeak.setField(2, tvoc);
      int responseCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
      Serial.println("ThingSpeak write response: " + String(responseCode));
    } else {
      Serial.println("CCS811 read error!");
    }
  } else {
    Serial.println("CCS811 not available!");
  }

  delay(5000);
}

bool testIFTTTConnection() {
  if (client.connect(HOST_NAME, 80)) {
    client.println("GET " + PATH_NAME + " HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println();

    int statusCode = -1;
    while (client.connected()) {
      while (client.available()) {
        String line = client.readStringUntil('\r');
        if (line.startsWith("HTTP/1.1")) {
          statusCode = line.substring(9, 12).toInt();
        }
      }
    }

    client.stop();
    return statusCode == 200;
  }
  return false;
}

bool testThingSpeakConnection() {
  ThingSpeak.setField(1, 0);
  ThingSpeak.setField(2, 0);
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  return x == 200;
}

void sendHttpRequest(String path) {
  if (client.connect(HOST_NAME, 80)) {
    Serial.println("Connected to server");
    client.println("GET " + path + " HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println(); // End HTTP header

    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        Serial.print(c);
      }
    }

    client.stop();
    Serial.println("\nDisconnected.");
  } else {
    Serial.println("Connection failed");
  }
}
