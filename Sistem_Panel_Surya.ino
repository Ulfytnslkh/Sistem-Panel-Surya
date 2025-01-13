#define BLYNK_TEMPLATE_ID "TMPL6hwZ1bGju"
#define BLYNK_TEMPLATE_NAME "Solar Panel"
#define BLYNK_AUTH_TOKEN "SGfIAL9-mfybHus0_EyQ2ERDsIRspZRv"  // Your Blynk Auth Token

#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

Adafruit_INA219 ina219;
Servo servoX;
Servo servoY;

// Pin definitions
const int servoXPin = 5;
const int servoYPin = 18;
const int ldrTopLeft = 33;
const int ldrTopRight = 34;
const int ldrBottomLeft = 35;
const int ldrBottomRight = 32;

// Servo angle limits
const int minAngle = 0;
const int maxAngle = 180;

// Servo angle limits for Y-axis
const int minYAngle = 30;
const int maxYAngle = 150;

// Wi-Fi credentials
const char* ssid = "test";
const char* password = "testing1";

// Servo positions
int posX = 90;
int posY = 90;

// Dynamic threshold
const int filterSize = 5;
int ldrTopLeftBuffer[filterSize] = { 0 };
int ldrTopRightBuffer[filterSize] = { 0 };
int ldrBottomLeftBuffer[filterSize] = { 0 };
int ldrBottomRightBuffer[filterSize] = { 0 };
int threshold = 20;

// INA219 data
float voltage = 0.0;
float current_A = 0.0;

// Blynk variables
bool manualControl = false;  // Toggle between manual and automatic control
bool deviceOn = false;       // Device state (on/off)

BlynkTimer timer;

// Blynk button handler for turning the device on/off (Virtual Pin V6)
BLYNK_WRITE(V6) {
  deviceOn = param.asInt();
  if (deviceOn) {
    Serial.println("Device turned ON. Pairing with light.");
    posX = 0;
    posY = (minYAngle + maxYAngle) / 2;  // Set Y angle to the midpoint of min and max angles
    Blynk.virtualWrite(V7, 1);          // Sending feedback to Blynk app (Virtual Pin V7)
  } else {
    Serial.println("Device turned OFF. Setting Y angle to midpoint.");
    posX = 0;
    posY = (minYAngle + maxYAngle) / 2;  // Reset Y angle to midpoint
    Blynk.virtualWrite(V7, 0);          // Sending feedback to Blynk app (Virtual Pin V7)
  }
  servoY.write(posY);  // Update servo position
}

// Blynk button handler (Virtual Pin V1) for manual control
BLYNK_WRITE(V1) {
  manualControl = param.asInt();
  if (manualControl) {
    Serial.println("Manual control enabled.");
  } else {
    Serial.println("Automatic control enabled.");
  }
}

// Blynk sliders for manual control (Virtual Pins V2 and V3)
BLYNK_WRITE(V2) {
  if (manualControl) {
    posX = constrain(param.asInt(), minAngle, maxAngle);
    servoX.write(posX);
    Serial.print("Manual X angle: ");
    Serial.println(posX);
  }
}

BLYNK_WRITE(V3) {
  if (manualControl) {
    posY = constrain(param.asInt(), minYAngle, maxYAngle);  // Restrict to Y-axis limits
    servoY.write(posY);
    Serial.print("Manual Y angle: ");
    Serial.println(posY);
  }
}

// Blynk display angles on the app
void sendServoAngles() {
  Blynk.virtualWrite(V4, posX);  // Virtual Pin V4 for X angle
  Blynk.virtualWrite(V5, posY);  // Virtual Pin V5 for Y angle
}

// Read INA219 data
void readINA219() {
  float shuntVoltage_mV = ina219.getShuntVoltage_mV();  // Tegangan pada shunt dalam mV
  float busVoltage_V = ina219.getBusVoltage_V();        // Tegangan bus dalam V

  // Debugging untuk memantau nilai mentah
  Serial.print("Shunt Voltage (mV): ");
  Serial.print(shuntVoltage_mV);
  Serial.print(", Bus Voltage (V): ");
  Serial.println(busVoltage_V);

  // Hitung tegangan total
  voltage = busVoltage_V + (shuntVoltage_mV / 1000);
  current_A = ina219.getCurrent_mA() / 1000.0;

  // Validasi noise kecil
  if (voltage < 0.1) voltage = 0.0;        // Anggap noise kecil sebagai 0.0
  if (current_A < 0.001) current_A = 0.0;  // Anggap noise kecil sebagai 0.0

  // Tampilkan nilai akhir
  Serial.print("Voltage: ");
  Serial.print(voltage, 2);  // Dua angka di belakang koma
  Serial.print(" V, Current: ");
  Serial.print(current_A, 3);  // Tiga angka di belakang koma
  Serial.println(" A");

  // Send data to Blynk
  Blynk.virtualWrite(V8, current_A);  // Virtual Pin V8 for current
  Blynk.virtualWrite(V9, voltage);    // Virtual Pin V9 for voltage
}

void setup() {
  servoX.attach(servoXPin);
  servoY.attach(servoYPin);
  servoX.write(posX);
  servoY.write(posY);

  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Inisialisasi Wire dengan SDA dan SCL
  Wire.begin(21, 22);

  // Kalibrasi INA219
  ina219.setCalibration_16V_400mA();

  // Inisialisasi INA219
  if (!ina219.begin(&Wire)) {  // Menggunakan instance Wire dengan pin yang disesuaikan
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }
  Serial.println("INA219 initialized.");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  timer.setInterval(1000L, sendServoAngles);  // Update servo angles on the app every second
  timer.setInterval(2000L, readINA219);       // Read INA219 data every 2 seconds
}

int movingAverage(int* buffer) {
  int sum = 0;
  for (int i = 0; i < filterSize; i++) {
    sum += buffer[i];
  }
  return sum / filterSize;
}

void loop() {
  Blynk.run();
  timer.run();

  if (manualControl) {
    return;  // Exit the loop and skip automatic control
  }

  // Read LDR values
  int rawTopLeft = analogRead(ldrTopLeft);
  int rawTopRight = analogRead(ldrTopRight);
  int rawBottomLeft = analogRead(ldrBottomLeft);
  int rawBottomRight = analogRead(ldrBottomRight);

  // Update moving average buffers
  for (int i = filterSize - 1; i > 0; i--) {
    ldrTopLeftBuffer[i] = ldrTopLeftBuffer[i - 1];
    ldrTopRightBuffer[i] = ldrTopRightBuffer[i - 1];
    ldrBottomLeftBuffer[i] = ldrBottomLeftBuffer[i - 1];
    ldrBottomRightBuffer[i] = ldrBottomRightBuffer[i - 1];
  }

  ldrTopLeftBuffer[0] = rawTopLeft;
  ldrTopRightBuffer[0] = rawTopRight;
  ldrBottomLeftBuffer[0] = rawBottomLeft;
  ldrBottomRightBuffer[0] = rawBottomRight;

  // Calculate filtered values
  int filteredTopLeft = movingAverage(ldrTopLeftBuffer);
  int filteredTopRight = movingAverage(ldrTopRightBuffer);
  int filteredBottomLeft = movingAverage(ldrBottomLeftBuffer);
  int filteredBottomRight = movingAverage(ldrBottomRightBuffer);

  Serial.print("Top Left: ");
  //33
  Serial.println(rawTopLeft);
  Serial.print("Top Right: ");
  // Serial.println(analogRead(35));
  //34
  Serial.println(rawTopRight);
  Serial.print("Bottom Left: ");
  //35
  Serial.println(rawBottomLeft);
  Serial.print("Bottom Right: ");
  //32
  Serial.println(rawBottomRight);

  // Compute differences
  int avgTop = (filteredTopLeft + filteredTopRight) / 2;
  int avgBottom = (filteredBottomLeft + filteredBottomRight) / 2;
  int avgLeft = (filteredTopLeft + filteredBottomLeft) / 2;
  int avgRight = (filteredTopRight + filteredBottomRight) / 2;

  int verticalDiff = avgTop - avgBottom;
  int horizontalDiff = avgLeft - avgRight;

  // Dynamic threshold adjustment
  int maxLDR = max(max(filteredTopLeft, filteredTopRight), max(filteredBottomLeft, filteredBottomRight));
  int minLDR = min(min(filteredTopLeft, filteredTopRight), min(filteredBottomLeft, filteredBottomRight));
  threshold = (maxLDR - minLDR) * 0.1;

  // Adjust servo positions
  // Adjust servo positions in automatic mode
  if (abs(verticalDiff) > threshold) {
    if (verticalDiff > 0 && posY < maxYAngle) {  // Move upward if top is brighter
      posY++;
    } else if (verticalDiff < 0 && posY > minYAngle) {  // Move downward if bottom is brighter
      posY--;
    }
  }

  if (abs(horizontalDiff) > threshold) {
    if (horizontalDiff > 0 && posX > minAngle) {
      posX--;
    } else if (horizontalDiff < 0 && posX < maxAngle) {
      posX++;
    }
  }

  posX = constrain(posX, minAngle, maxAngle);
  posY = constrain(posY, minYAngle, maxYAngle);

  servoX.write(posX);
  servoY.write(posY);
}