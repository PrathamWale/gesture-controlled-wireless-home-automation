#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <esp_now.h>
#include <WiFi.h>

// ✅ YOUR RECEIVER MAC (updated)
uint8_t receiverMAC[] = {0x1C, 0xC3, 0xAB, 0xD2, 0x33, 0x90};

// Thresholds
#define TILT_THRESHOLD 20.0
#define NEUTRAL_THRESHOLD 10.0

typedef struct {
  uint8_t applianceID;
  uint8_t action;
} GesturePacket;

Adafruit_MPU6050 mpu;
GesturePacket packet;

bool gestureLocked = false;

float smoothPitch = 0;
float smoothRoll = 0;

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("✅ Sent");
  } else {
    Serial.println("❌ Send Fail");
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);

  if (!mpu.begin()) {
    Serial.println("MPU6050 NOT FOUND");
    while (1);
  }

  mpu.setFilterBandwidth(MPU6050_BAND_10_HZ);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Peer Add Failed");
    return;
  } else {
    Serial.println("Peer Added Successfully");
  }
}

void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;

  float pitch = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / PI;
  float roll  = atan2(-ax, az) * 180.0 / PI;

  // 🔹 smoothing
  smoothPitch = 0.7 * smoothPitch + 0.3 * pitch;
  smoothRoll  = 0.7 * smoothRoll  + 0.3 * roll;

  uint8_t detected = 0;

  if (!gestureLocked) {
    if      (smoothPitch >  TILT_THRESHOLD) detected = 1;
    else if (smoothPitch < -TILT_THRESHOLD) detected = 2;
    else if (smoothRoll  < -TILT_THRESHOLD) detected = 3;
    else if (smoothRoll  >  TILT_THRESHOLD) detected = 4;

    if (detected != 0) {
      packet.applianceID = detected;
      packet.action = 0;

      esp_now_send(receiverMAC, (uint8_t *)&packet, sizeof(packet));

      Serial.printf("Gesture %d Sent\n", detected);

      gestureLocked = true;
    }
  }

  // 🔹 wait for neutral
  if (gestureLocked) {
    if (abs(smoothPitch) < NEUTRAL_THRESHOLD &&
        abs(smoothRoll)  < NEUTRAL_THRESHOLD) {

      gestureLocked = false;
      Serial.println("Neutral → Ready");
    }
  }

  delay(50);
}