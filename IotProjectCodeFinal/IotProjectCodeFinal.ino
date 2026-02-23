#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>

#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER_PIN 13

// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire); // creates thr OLED display using I2C
WebServer server(80);

const char* AP_SSID = "WaterLevelMonitor";
const char* AP_PASSWORD = "12345678";

float tankHeight = 16.0;   // Tank height in cm
float minDistance = 7.0;   // Distance from sensor
float maxDistance = minDistance + tankHeight; // Distance from the botton of the tank when is empty
float latestDistance = 0.0;
int latestLevel = 0;
unsigned long lastSensorReadMs = 0;
const unsigned long SENSOR_READ_INTERVAL_MS = 250;

void readAndRenderSensor();

float readDistanceOnce() { // Sends message from sensor and measures how long it takes to return, converts into cm
  digitalWrite(TRIG_PIN, LOW); // Trigger is slow before starting (0%)
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10); // sends a trigger pulse
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // measures how long the ECHO PIN stays high
  float distance = duration * 0.0343 / 2;

  return distance;
}

float getDistance() {
  // Ultrasonic sensors are noisy. Use median of 5 reads for stability.
  float samples[5];
  int validCount = 0;

  for (int i = 0; i < 5; i++) {
    float d = readDistanceOnce();
    // Reject impossible values and timeout echoes.
    if (d >= 2.0 && d <= 400.0) {
      samples[validCount++] = d;
    }
    delay(10);
  }

  if (validCount == 0) {
    return -1.0;
  }

  // Insertion sort (small fixed array) so we can take median.
  for (int i = 1; i < validCount; i++) {
    float key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = key;
  }

  return samples[validCount / 2];
}

// Convert a measured distance to percentage
int getWaterLevelPercent(float distance) {
  if (distance < 0) return latestLevel;
  if (distance <= minDistance) return 100; // full
  if (distance >= maxDistance) return 0;   // empty

  float level = 100 - ((distance - minDistance) / tankHeight) * 100; //calculates the percentage
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  return (int)(level + 0.5f);
}

void handleRoot() {
  server.send(200, "text/plain", "Water Level Monitor API is running.");
}

void handleData() {
  // Force a fresh sample so manual refresh matches OLED immediately.
  readAndRenderSensor();

  String payload = "{";
  payload += "\"distance\":";
  payload += String(latestDistance, 2);
  payload += ",\"level\":";
  payload += String(latestLevel);
  payload += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", payload);
}

void readAndRenderSensor() {
  float measuredDistance = getDistance();
  if (measuredDistance >= 0) {
    latestDistance = measuredDistance;
    latestLevel = getWaterLevelPercent(latestDistance);
  }

  //Prints values to thr display
  Serial.print("Distance: ");
  Serial.print(latestDistance);
  Serial.print(" cm | Level: ");
  Serial.print(latestLevel);
  Serial.println("%");

  // The buzzer activates when water level hit 20% or below
  if (latestLevel <= 20) {
    tone(BUZZER_PIN, 1500);
  } else {
    noTone(BUZZER_PIN);
  }

  // OLED display text
  display.clearDisplay();

  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Water Level Monitor");

  display.setTextSize(2);
  display.setCursor(0, 20); // where text is set
  display.print(latestLevel);
  display.println("%");

  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Dist: ");
  display.print(latestDistance);
  display.println(" cm");

  display.display(); // Shows readings on screen
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialise OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    for (;;); // stops excution if OLED is not detected
  }
  // OLED display
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  readAndRenderSensor();
  lastSensorReadMs = millis();
}

void loop() {
  server.handleClient();

  if (millis() - lastSensorReadMs >= SENSOR_READ_INTERVAL_MS) {
    lastSensorReadMs = millis();
    readAndRenderSensor();
  }
}

