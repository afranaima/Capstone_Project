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

bool slaveActive = false;

float humidity = 0;
float temperature = 0;
int moisture = 0;
float moisturePercent = 0;
bool sensorError = false;

const int MOISTURE_AIR = 1024;    
const int MOISTURE_WATER = 2597;  

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 2000; // 2 detik

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(SLAVE_TRIGGER_PIN, INPUT_PULLDOWN);

  Wire.begin();
  WiFi.begin(ssid, pass);
  Blynk.config(auth);
  dht.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(true); // stop jika OLED gagal
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

  int triggerState = digitalRead(SLAVE_TRIGGER_PIN);
  if (triggerState == HIGH) {
    if (!slaveActive) {
      slaveActive = true;
    }
    unsigned long now = millis();
    if (now - lastSensorRead > SENSOR_INTERVAL) {
      readSensors();
      lastSensorRead = now;
    }
  } else {
    if (slaveActive) {
      slaveActive = false;
      Serial.println("Slave standby (master trigger LOW).");
    }
  }

  updateDisplay();

  delay(100);
}

void readSensors() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  int moistureSum = 0;
  for (int i=0; i<5; i++) {
    moistureSum += analogRead(MOISTURE_PIN);
    delay(5);
  }
  moisture = moistureSum / 5;
  moisturePercent = map(moisture, MOISTURE_AIR, MOISTURE_WATER, 100, 0);
  moisturePercent = constrain(moisturePercent, 0, 100);

  if (!isnan(humidity) && !isnan(temperature)) {
    sensorError = false;
    Blynk.virtualWrite(V3, temperature);
    Blynk.virtualWrite(V4, humidity);
    Blynk.virtualWrite(V5, moisturePercent);

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
  } else {
    sensorError = true;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0,0);
  display.println("Plant Monitor Backup");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, WHITE);

  display.setCursor(0, 15);
  display.print("Sensor : ");
  display.println(sensorError ? "Error" : "Aman");

  display.setCursor(0, 25);
  display.printf("Temp : %.1f C\n", temperature);

  display.setCursor(0, 35);
  display.printf("Humidity   : %.1f%%\n", humidity);

  display.setCursor(0, 45);
  display.printf("Soil : %.1f%%\n", moisturePercent);

  display.setCursor(0, 55);
  display.print("Status : ");
  display.print(stressStatus);

  display.display();
}
