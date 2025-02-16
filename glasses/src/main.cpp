#include "esp_camera.h"
#include "BluetoothSerial.h"
#include "esp_system.h"
#include <ESP32Servo.h>  // Use the ESP32-compatible servo library

// Create a BluetoothSerial object
BluetoothSerial SerialBT;

// Create a Servo object for the servo on GPIO14
Servo myServo;
const int servoPin = 14; // Using GPIO14 for the servo

// Global variable to track the last commanded angle
int currentServoAngle = 90;

// Print the Bluetooth MAC address for reference
void printBTMacAddress() {
  uint8_t btMac[6];
  // Read the Bluetooth MAC address
  esp_read_mac(btMac, ESP_MAC_BT);
  
  Serial.print("ESP32 Bluetooth MAC: ");
  for (int i = 0; i < 6; i++) {
    if (btMac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(btMac[i], HEX);
    if (i < 5) {
      Serial.print(":");
    }
  }
  Serial.println();
}

// Camera configuration for an AI‑Thinker ESP32‑CAM module
camera_config_t camera_config = {
  .pin_pwdn  = 32,
  .pin_reset = -1,
  .pin_xclk = 0,
  .pin_sccb_sda = 26,
  .pin_sccb_scl = 27,

  .pin_d7 = 35,
  .pin_d6 = 34,
  .pin_d5 = 39,
  .pin_d4 = 36,
  .pin_d3 = 21,
  .pin_d2 = 19,
  .pin_d1 = 18,
  .pin_d0 = 5,
  .pin_vsync = 25,
  .pin_href = 23,
  .pin_pclk = 22,

  // XCLK 20MHz or 10MHz for OV2640 camera module
  .xclk_freq_hz = 20000000,
  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,
  .pixel_format = PIXFORMAT_JPEG, // For streaming, use JPEG format
  .frame_size = FRAMESIZE_QVGA,   // 320x240
  .jpeg_quality = 24,
  .fb_count = 1,
};

// Initialize the camera
void initCamera() {
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    while (true) { delay(1000); } // Halt if camera initialization fails
  }
}

// Process incoming Bluetooth commands
// Expected command format: "S14:<angle>" e.g., "S14:90"
void processCommands() {
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim();
    if (command.startsWith("S14:")) {
      int angle = command.substring(4).toInt();
      angle = constrain(angle, 0, 180);
      myServo.write(angle);
      currentServoAngle = angle;
      Serial.print("Servo on GPIO14 set to ");
      Serial.println(angle);
      // Optionally send a confirmation back to the client
      SerialBT.print("Servo set to ");
      SerialBT.println(angle);
    } else {
      Serial.print("Unknown command received: ");
      Serial.println(command);
    }
  }
}


void setup() {
  Serial.begin(115200);
  
  // Initialize the camera
  initCamera();

  // Initialize servo on GPIO14 and set to a default angle (e.g., 90°)
  myServo.attach(servoPin);
  myServo.write(currentServoAngle);
  Serial.println("Servo on GPIO14 initialized at 90°");

  printBTMacAddress();

  // Start Bluetooth Serial with the name "ESP32_CAM_BT"
  if (!SerialBT.begin("ESP32_CAM_BT")) {
    Serial.println("An error occurred initializing Bluetooth");
} else {
    Serial.println("Bluetooth initialized. Waiting for connection...");
  }
}

void loop() {
  // Only attempt to send if a Bluetooth client is connected
  if (!SerialBT.hasClient()) {
    Serial.println("No Bluetooth client connected. Waiting...");
    delay(500);
    return;
  }

  // // Process any incoming commands first
  processCommands();

  // Capture a frame from the camera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    return;
  }

  // Prepare to send the frame:
  // 1. Send a 4-byte header (frame size)
  // 2. Then send the JPEG data
  uint32_t frameSize = fb->len;
  SerialBT.write((uint8_t*)&frameSize, sizeof(frameSize));
  SerialBT.write(fb->buf, frameSize);
  
  esp_camera_fb_return(fb);

  // Serial.printf("Frame sent: %lu bytes\n", frameSize);
  delay(1000); // Delay to allow client time to process frame
}
