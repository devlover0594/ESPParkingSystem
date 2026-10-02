#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <esp_now.h>
#include <WiFi.h>
#include "DFRobotDFPlayerMini.h"

#define SS_PIN 5
#define RST_PIN 22
MFRC522 rfid(SS_PIN, RST_PIN);

#define OBSTACLE_PIN 13
#define SERVO_PIN 4

Servo gateServo;
HardwareSerial mySerial(2);
DFRobotDFPlayerMini myDFPlayer;

typedef struct struct_message {
  char uidStr[20];
} struct_message;

struct_message incomingData;
String activeUID = ""; // Menyimpan UID dari Gate Masuk
bool vehicleDetected = false;

// Callback saat data ESP-NOW diterima
void OnDataRecv(const esp_now_recv_info * info, const uint8_t *incomingDataBytes, int len) {
  memcpy(&incomingData, incomingDataBytes, sizeof(incomingData));
  activeUID = String(incomingData.uidStr);
  Serial.println("UID Diterima dari Gate Masuk: " + activeUID);
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, 16, 17);

  pinMode(OBSTACLE_PIN, INPUT);
  
  SPI.begin();
  rfid.PCD_Init();
  
  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  if (myDFPlayer.begin(mySerial)) {
    myDFPlayer.volume(25);
  }

  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
  }
}

void loop() {
  int sensorValue = digitalRead(OBSTACLE_PIN);

  if (sensorValue == LOW && !vehicleDetected) {
    vehicleDetected = true;
    myDFPlayer.playFolder(1, 3); // "Silahkan tempelkan kartu pembayaran"
    delay(2000);
  }

  if (vehicleDetected) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String currentUID = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        currentUID += String(rfid.uid.uidByte[i], HEX);
      }
      currentUID.toUpperCase();

      // Validasi UID
      if (currentUID == activeUID && activeUID != "") {
        myDFPlayer.playFolder(1, 5); // "Terima kasih, selamat jalan"
        gateServo.write(90); // Buka
        delay(3000);

        while (digitalRead(OBSTACLE_PIN) == LOW) {
          delay(100);
        }

        gateServo.write(0); // Tutup
        activeUID = ""; // Reset UID
        vehicleDetected = false;
      } else {
        myDFPlayer.playFolder(1, 4); // "Kartu tidak cocok"
        delay(2000);
      }
      rfid.PICC_HaltA();
    }
  }
}
