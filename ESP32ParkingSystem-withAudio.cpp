// ## Pin Configuration

// ### MFRC522 RFID Module
// | Pin | ESP32 GPIO |
// |-----|------------|
// | RST | 27 |
// | SS  | 5 |
// | MOSI| 23 (default SPI) |
// | MISO| 19 (default SPI) |
// | SCK | 18 (default SPI) |
// | GND | GND |
// | VCC | 5V |

// ### Servo Motor
// | Pin | ESP32 GPIO |
// |-----|------------|
// | Signal | 13 |
// | VCC | 5V |
// | GND | GND |

// ### IR Sensors
// | Sensor | ESP32 GPIO | Logic |
// |--------|------------|-------|
// | IR Masuk (Entry) | 32 | LOW = object detected |
// | IR Keluar (Exit) | 33 | LOW = object detected |

// ### LCD I2C 1602
// | Pin | ESP32 GPIO |
// |-----|------------|
// | VCC | 5V |
// | GND | GND |
// | SDA | 21 (default I2C) |
// | SCL | 22 (default I2C) |

// ### DFPlayer Mini
// | Pin | ESP32 GPIO |
// |-----|------------|
// | TX | 16 | RX2
// | RX | 17 | TX2
// | VCC | 5V |
// | GND | GND |
// | SPK+ | Speaker positive |
// | SPK- | Speaker negative |

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <DFRobotDFPlayerMini.h>

// Definisi Pin Hardware
#define RST_PIN     27
#define SS_PIN      5
#define SERVO_PIN   13
#define IR_MASUK    32
#define IR_KELUAR   33

// Inisialisasi Objek Modul
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo servoPalang;
LiquidCrystal_I2C lcd(0x27, 16, 2);
DFRobotDFPlayerMini myDFPlayer;

// Variabel Status Sistem Parkir
String storedUID = "";
bool isVehicleInside = false;

// Fungsi untuk Mengubah UID RFID Menjadi Format String Hexadecimal
String getUIDString() {
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0";
        uidString += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidString.toUpperCase();
    return uidString;
}

// Fungsi Menangani Alur Masuk Kendaraan
void handleEntry() {
    // Sensor IR Masuk mendeteksi kendaraan (LOGIC: LOW saat mendeteksi objek)
    if (digitalRead(IR_MASUK) == LOW && !isVehicleInside) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Silahkan Tap");
        lcd.setCursor(0, 1);
        lcd.print("Kartu Anda");

        // Putar audio 001.mp3 (Silahkan tap kartu)
        myDFPlayer.play(1);

        // Menunggu kartu RFID di-tap (Timeout 10 detik)
        unsigned long startTime = millis();
        bool cardTapped = false;

        while (millis() - startTime < 10000) {
            if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                storedUID = getUIDString();
                isVehicleInside = true;
                cardTapped = true;
                
                mfrc522.PICC_HaltA();
                break;
            }
        }

        if (cardTapped) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Silahkan Masuk");
            
            // Putar audio 002.mp3 (Silahkan masuk)
            myDFPlayer.play(2);

            // Buka palang pintu (90 derajat)
            servoPalang.write(90);
            delay(2000); // Tahan terbuka selama 2 detik

            // Tutup kembali palang pintu (0 derajat)
            servoPalang.write(0);
            lcd.clear();
        } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Waktu Habis");
            delay(1500);
            lcd.clear();
        }
    }
}

// Fungsi Menangani Alur Keluar Kendaraan
void handleExit() {
    // Sensor IR Keluar mendeteksi kendaraan
    if (digitalRead(IR_KELUAR) == LOW && isVehicleInside) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Silahkan Tap");
        lcd.setCursor(0, 1);
        lcd.print("Kartu Keluar");

        // Menunggu kartu RFID keluar (Timeout 10 detik)
        unsigned long startTime = millis();
        bool cardTapped = false;
        String scannedUID = "";

        while (millis() - startTime < 10000) {
            if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                scannedUID = getUIDString();
                cardTapped = true;
                
                mfrc522.PICC_HaltA();
                break;
            }
        }

        if (cardTapped) {
            if (scannedUID == storedUID) {
                // UID Sesuai
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Terima Kasih");
                lcd.setCursor(0, 1);
                lcd.print("Silahkan Keluar");

                myDFPlayer.play(2); // Putar audio sukses

                // Buka palang pintu
                servoPalang.write(90);
                delay(2000);

                // Tutup palang dan reset status kendaraan
                servoPalang.write(0);
                storedUID = "";
                isVehicleInside = false;
                lcd.clear();
            } else {
                // UID Tidak Sesuai / Salah Kartu
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Kartu Salah!");
                lcd.setCursor(0, 1);
                lcd.print("Tap Pembayaran");
                
                delay(2000);
                lcd.clear();
            }
        } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Waktu Habis");
            delay(1500);
            lcd.clear();
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Inisialisasi Serial2 ESP32 untuk DFPlayer Mini (RX=16, TX=17)
    Serial2.begin(9600, SERIAL_8N1, 16, 17);

    // Inisialisasi SPI dan RFID
    SPI.begin();
    mfrc522.PCD_Init();
    
    // Inisialisasi Servo
    servoPalang.attach(SERVO_PIN);
    servoPalang.write(0); // Posisi awal tertutup

    // Konfigurasi Pin Sensor IR
    pinMode(IR_MASUK, INPUT);
    pinMode(IR_KELUAR, INPUT);

    // Inisialisasi LCD I2C
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Sistem Parkir");
    lcd.setCursor(0, 1);
    lcd.print("Siap Digunakan");

    // Inisialisasi DFPlayer Mini
    if (!myDFPlayer.begin(Serial2)) {
        Serial.println("DFPlayer Mini tidak terdeteksi!");
    } else {
        myDFPlayer.volume(20); // Atur volume (skala 0 - 30)
    }

    delay(2000);
    lcd.clear();
}

void loop() {
    // Memantau alur masuk dan keluar secara terus-menerus
    handleEntry();
    handleExit();
    
    // Jeda singkat untuk menjaga kestabilan loop ESP32
    delay(100);
}
