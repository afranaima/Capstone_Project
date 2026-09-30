#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include "PlantHealth.h" // Model TensorFlow Lite untuk prediksi stres tanaman
#include <tflm_esp32.h>
#include <eloquent_tinyml.h>

#define BLYNK_TEMPLATE_ID "TMPL6tOocHz-n"
#define BLYNK_TEMPLATE_NAME "Plant Monitor"
#define BLYNK_AUTH_TOKEN "FznrJVrhv3ub-mFUc9g7wr5sdSsAy1Zm"

#include <BlynkSimpleEsp32.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 4
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MOISTURE_PIN 34
#define SLAVE_TRIGGER_PIN 26

#define ARENA_SIZE 4000
Eloquent::TF::Sequential<TF_NUM_OPS, ARENA_SIZE> tf;

float modelInput[3];

int plantStressLevel = 0;  // Level stres (0-2: 0=healthy, 1=high stress, 2=moderate stress)
String stressStatus = "Healthy";

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "iphone hasna";
char pass[] = "capstoneproject";

float humidity = 0;
float temperature = 0;
int moisture = 0;
float moisturePercent = 0;
bool sensorError = false;

// Moisture calibration values
const int MOISTURE_AIR = 1024;
const int MOISTURE_WATER = 2597;

unsigned long lastSensorRead = 0;
unsigned long sensorInterval = 2000;

void setup() {
  Serial.begin(9600);
  pinMode(SLAVE_TRIGGER_PIN, OUTPUT);
  digitalWrite(SLAVE_TRIGGER_PIN, LOW);

  Blynk.begin(auth, ssid, pass);
  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }

  tf.setNumInputs(3);  // Soil_Moisture, Ambient_Temperature, Humidity
  tf.setNumOutputs(3); // 1 kelas output (normal, stres ringan, atau stres berat)
  
  // Tambahkan operator yang diperlukan (sesuaikan dengan model Anda)
  tf.resolver.AddFullyConnected();
  tf.resolver.AddSoftmax();
  
  // Mulai model
  if (!tf.begin(PlantHealth).isOk()) {
    Serial.println("Model initialization failed:");
    Serial.println(tf.exception.toString());
  } else {
    Serial.println("Model initialized successfully");
  }

  display.clearDisplay();
  readSensors();
}

void loop() {
  Blynk.run();

  if (millis() - lastSensorRead >= sensorInterval) {
    readSensors();
    lastSensorRead = millis();
  }

  updateDisplay();
}

void readSensors() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  int moistureSum = 0;
  for (int i = 0; i < 5; i++) {
    moistureSum += analogRead(MOISTURE_PIN);
    delay(10);
  }
  moisture = moistureSum / 5;
  moisturePercent = map(moisture, MOISTURE_AIR, MOISTURE_WATER, 100, 0);
  moisturePercent = constrain(moisturePercent, 0, 100);

  if (isnan(humidity) || isnan(temperature)) {
    digitalWrite(SLAVE_TRIGGER_PIN, HIGH);
    sensorError = true;
  } else {
    digitalWrite(SLAVE_TRIGGER_PIN, LOW);
    sensorError = false;

    Blynk.virtualWrite(V0, temperature);
    Blynk.virtualWrite(V1, humidity);
    Blynk.virtualWrite(V2, moisturePercent);

    modelInput[0] = moisturePercent / 100.0;  // Normalize to 0-1
    modelInput[1] = temperature / 100.0;       // Normalize (assuming max temp 50°C)
    modelInput[2] = humidity / 100.0;         // Normalize to 0-1
    
    // Run prediction
    if (!tf.predict(modelInput).isOk()) {
      Serial.println("Prediction error:");
      Serial.println(tf.exception.toString());
      stressStatus = "Err Prediksi";
      return;
    }
    
    // Get prediction results
    plantStressLevel = tf.classification;
    
    // Set status based on prediction
    switch (plantStressLevel) {
      case 0:
        stressStatus = "Healthy";
        break;
      case 1:
        stressStatus = "High Stress";
        break;
      case 2:
        stressStatus = "Moderate Stress";
        break;
      default:
        stressStatus = "Unknown";
    }
  }

}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Header
  display.setCursor(22, 0);
  display.print("Plant Monitor");
  display.drawLine(0, 10, 128, 10, WHITE);

  // Status Sensor
  display.setCursor(0, 12);
  display.print("Sensor : ");
  display.print(sensorError ? "Error" : "Aman");

  // Temp
  display.setCursor(0, 22);
  display.printf("Temp : %.1f C", temperature);

  // Humidity
  display.setCursor(0, 32);
  display.printf("Humidity : %.1f%%", humidity);

  // Moisture
  display.setCursor(0, 42);
  display.printf("Soil : %.1f%%", moisturePercent);

  // AI Status
  display.setCursor(0, 52);
  display.print("Status  : ");
  display.print(stressStatus);

  display.display();
}
