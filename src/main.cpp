#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "secrets.h"

// Inisialisasi Objek Sensor dan Firebase
Adafruit_BME680 bme;
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 15000; // Interval pengiriman/pengecekan data 15 detik
bool isSensorInitialized = false;          // Flag inisialisasi sensor

// Fungsi untuk menghubungkan ke WiFi dengan auto-reconnect
void connectWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  Serial.print("Menghubungkan ke WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20)
  {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n[WiFi] Terhubung!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\n[WiFi] Gagal terhubung. Akan dicoba kembali nanti.");
  }
}

// Fungsi untuk menginisialisasi sensor BME688
bool initializeSensor()
{
  Serial.println("[BME688] Menginisialisasi sensor...");
  if (!bme.begin(0x77)) // Ganti ke 0x76 jika SDO disambungkan ke GND
  {
    Serial.println("[ERROR] Sensor tidak ditemukan! Periksa wiring.");
    return false;
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C selama 150ms

  Serial.println("[OK] Sensor BME688 Siap!");
  return true;
}

// Fungsi untuk mengirim data sensor ke Firebase Realtime Database
void sendDataToFirebase(float temp, float hum, float pres, float gas)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[Firebase] Gagal mengirim, WiFi tidak terhubung.");
    return;
  }

  FirebaseJson json;
  json.set("temperatur", temp);
  json.set("kelembapan", hum);
  json.set("tekanan_udara", pres);
  json.set("gas_resistance", gas);
  json.set("timestamp", (int)Firebase.getCurrentTime());

  // 1) Update data terkini (overwrite)
  String pathTerakhir = String("/tanaman_list/") + DEVICE_TANAMAN_ID + "/data_terakhir";
  Serial.print("[Firebase] Mengupdate data terkini di ");
  Serial.println(pathTerakhir);

  if (Firebase.setJSON(fbdo, pathTerakhir.c_str(), json))
  {
    Serial.println("[Firebase] Data terkini berhasil diperbarui!");
  }
  else
  {
    Serial.print("[Firebase] Gagal update data terkini. Alasan: ");
    Serial.println(fbdo.errorReason());
  }

  // 2) Simpan ke history (append)
  String pathHistory = String("/tanaman_list/") + DEVICE_TANAMAN_ID + "/history_data";
  Serial.print("[Firebase] Menambahkan history data ke ");
  Serial.println(pathHistory);

  if (Firebase.pushJSON(fbdo, pathHistory.c_str(), json))
  {
    Serial.println("[Firebase] History data berhasil ditambahkan!");
  }
  else
  {
    Serial.print("[Firebase] Gagal push history. Alasan: ");
    Serial.println(fbdo.errorReason());
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== BME688 & Firebase Project ===");

  // Koneksi WiFi pertama kali
  connectWiFi();

  // Inisialisasi Konfigurasi Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("[OK] Firebase Siap!");
  Serial.println("==========================");
}

void loop()
{
  // Pastikan WiFi tetap terhubung
  connectWiFi();

  unsigned long currentMillis = millis();

  // Cek status keaktifan sensor dari Firebase setiap 15 detik
  if (currentMillis - lastSendTime >= sendInterval)
  {
    lastSendTime = currentMillis;

    if (WiFi.status() == WL_CONNECTED)
    {
      String pathStatus = String("/tanaman_list/") + DEVICE_TANAMAN_ID + "/status_sensor_aktif";
      Serial.print("[Firebase] Mengecek status aktif sensor di ");
      Serial.println(pathStatus);

      if (Firebase.getInt(fbdo, pathStatus.c_str()))
      {
        int statusVal = fbdo.intData();

        if (statusVal == 0)
        {
          if (isSensorInitialized)
          {
            Serial.println("Sensor dimatikan jarak jauh. Standby mode...");
            isSensorInitialized = false;
          }
        }
        else if (statusVal == 1)
        {
          if (!isSensorInitialized)
          {
            if (initializeSensor())
            {
              isSensorInitialized = true;
            }
          }

          // Lakukan pembacaan data sensor & kirim jika sudah diinisialisasi
          if (isSensorInitialized)
          {
            if (bme.performReading())
            {
              float temp = bme.temperature;
              float hum = bme.humidity;
              float pres = bme.pressure / 100.0;
              float gas = (bme.gas_resistance == 0) ? -1.0 : (float)bme.gas_resistance;

              sendDataToFirebase(temp, hum, pres, gas);
            }
            else
            {
              Serial.println("[ERROR] Gagal membaca sensor BME688.");
            }
          }
        }
      }
      else
      {
        Serial.print("[Firebase] Gagal membaca status aktif. Alasan: ");
        Serial.println(fbdo.errorReason());
      }
    }
  }

  // Tampilkan data ke Serial Monitor setiap 3 detik
  static unsigned long lastPrintTime = 0;
  if (currentMillis - lastPrintTime >= 3000)
  {
    lastPrintTime = currentMillis;

    Serial.println("---------------------------");
    Serial.printf("ID Tanaman    : %s\n", DEVICE_TANAMAN_ID);
    if (isSensorInitialized)
    {
      Serial.printf("Temperatur    : %.2f C\n", bme.temperature);
      Serial.printf("Kelembapan    : %.2f %%\n", bme.humidity);
      Serial.printf("Tekanan Udara : %.2f hPa\n", bme.pressure / 100.0);

      if (bme.gas_resistance == 0)
      {
        Serial.println("Gas Resistance: Heater warming up...");
      }
      else
      {
        Serial.printf("Gas Resistance: %.0f Ohm\n", (float)bme.gas_resistance);
      }
    }
    else
    {
      Serial.println("Status Sensor : Standby / Nonaktif (Heater OFF)");
    }
    Serial.println("---------------------------");
  }
}