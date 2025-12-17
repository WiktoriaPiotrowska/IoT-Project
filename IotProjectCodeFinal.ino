#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER_PIN 13

// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire); // creates thr OLED display using I2C

float tankHeight = 16.0;   // Tank height in cm
float minDistance = 7.0;   // Distance from sensor
float maxDistance = minDistance + tankHeight; // Distance from the botton of the tank when is empty

float getDistance() { // Sends message from sensor and measures how long it takes to return, converts into cm
  digitalWrite(TRIG_PIN, LOW); // Trigger is slow before starting (0%)
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10); // sends a trigger pulse
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // measures how long the ECHO PIN stays high
  float distance = duration * 0.0343 / 2;

  return distance;
}

// Convert a measured distance to percentage
int getWaterLevelPercent(float distance) {
  if (distance <= minDistance) return 100; // full
  if (distance >= maxDistance) return 0;   // empty

  float level = 100 - ((distance - minDistance) / tankHeight) * 100; //calculates the percentage
  return (int)level;
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
}

void loop() {
  // Reads distance from ultrasonic sensor
  float distance = getDistance();
  // Converts to percentage
  int level = getWaterLevelPercent(distance);
//Prints values to thr display
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm | Level: ");
  Serial.print(level);
  Serial.println("%");

  // The buzzer activates when water level hit 20% or below
  if (level <= 20) {
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
  display.print(level);
  display.println("%");

  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Dist: ");
  display.print(distance);
  display.println(" cm");

  display.display(); // Shows readings on screen

  delay(500); //display to keep reading stable
}





