#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <esp_now.h>
#include <WiFi.h>
#include "DFRobotDFPlayerMini.h"

// Pin RFID
#define SS_PIN 5
#define RST_PIN 22
MFRC522 rfid(SS_PIN, RST_PIN);

// Pin Komponen
#define OBSTACLE_PIN 13
#define SERVO_PIN 4

Servo gateServo;
HardwareSerial mySerial(2); // UART2 untuk DFPlayer
DFRobotDFPlayerMini myDFPlayer;

// MAC Address ESP32 Gate Keluar (Ganti sesuai MAC Address ESP32 #2)
uint8_t receiverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct struct_message {
  char uidStr[20];
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

bool vehicleDetected = false;

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17

  pinMode(OBSTACLE_PIN, INPUT);
  
  SPI.begin();
  rfid.PCD_Init();
  
  gateServo.attach(SERVO_PIN);
  gateServo.write(0); // Palang Tertutup (0 derajat)

  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("DFPlayer Error!");
  } else {
    myDFPlayer.volume(25); // Set Volume (0-30)
  }

  // Init ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  int sensorValue = digitalRead(OBSTACLE_PIN);

  // Sensor Obstacle Active LOW (biasanya sensor IR bernilai LOW saat terhalang)
  if (sensorValue == LOW && !vehicleDetected) {
    vehicleDetected = true;
    myDFPlayer.playFolder(1, 1); // "Selamat datang, silahkan tap kartu"
    delay(2000);
  }

  if (vehicleDetected) {
    // Cek apakah ada kartu RFID di-tap
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String strUID = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        strUID += String(rfid.uid.uidByte[i], HEX);
      }
      strUID.toUpperCase();
      
      Serial.println("Card UID: " + strUID);

      // Kirim UID ke Gate Keluar via ESP-NOW
      strUID.toCharArray(myData.uidStr, 20);
      esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));

      // Buka Palang
      myDFPlayer.playFolder(1, 2); // "Silahkan masuk"
      gateServo.write(90); // Palang Terbuka
      delay(3000);

      // Tunggu kendaraan lewat
      while (digitalRead(OBSTACLE_PIN) == LOW) {
        delay(100);
      }

      gateServo.write(0); // Tutup Palang
      vehicleDetected = false;
      rfid.PICC_HaltA();
    }
  }
}
