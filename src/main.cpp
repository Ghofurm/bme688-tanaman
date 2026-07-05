#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <FirebaseESP32.h>
#include <time.h>
#include <math.h>
#include "secrets.h"

// Inisialisasi Objek Sensor dan Firebase
Adafruit_BME680 bme;
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 15000; // Interval pengiriman/pengecekan data 15 detik
bool isSensorInitialized = false;         // Flag inisialisasi sensor
String targetTanamanID = "";              // ID Tanaman target yang didapat dinamis dari Firebase
float benchmarkGas = 100000.0;            // Default benchmark gas
bool hasFetchedBenchmark = false;         // Flag to fetch benchmark only once per plant ID

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

  if (targetTanamanID == "")
  {
    Serial.println("[Firebase] Gagal mengirim, ID tanaman target belum diset.");
    return;
  }

  // A. Ambil nilai benchmark_gas dari Firebase jika belum ada
  if (!hasFetchedBenchmark)
  {
    String pathBenchmark = String("/tanaman_list/") + targetTanamanID + "/benchmark_gas";
    Serial.print("[Firebase] Mengambil benchmark_gas dari ");
    Serial.println(pathBenchmark);
    if (Firebase.getDouble(fbdo, pathBenchmark.c_str()))
    {
      benchmarkGas = fbdo.doubleData();
      hasFetchedBenchmark = true;
      Serial.printf("[Firebase] Benchmark Gas berhasil diambil: %.2f Ohm\n", benchmarkGas);
    }
    else
    {
      Serial.print("[Firebase] Gagal mengambil benchmark_gas. Menggunakan default. Alasan: ");
      Serial.println(fbdo.errorReason());
    }
  }

  // B. Hitung parameter ilmiah / feature engineering
  float gasLog = (gas > 0) ? log(gas) : -1.0;
  float gasDelta = (gas > 0) ? (gas - benchmarkGas) : 0.0;
  // Rumus kelembapan absolut (g/m3): (6.112 * exp((17.67 * T)/(T + 243.5)) * hum * 2.1674) / (273.15 + T)
  float absHumidity = (6.112 * exp((17.67 * temp) / (temp + 243.5)) * hum * 2.1674) / (273.15 + temp);

  FirebaseJson json;
  json.set("timestamp", (int)time(nullptr));
  json.set("temperatur", temp);
  json.set("kelembapan", hum);
  json.set("tekanan_udara", pres);
  json.set("gas_resistance", gas);
  json.set("gas_log", gasLog);
  json.set("gas_delta", gasDelta);
  json.set("abs_humidity", absHumidity);

  // 1) Update data terkini (overwrite) dengan retry
  String pathTerakhir = String("/tanaman_list/") + targetTanamanID + "/data_terakhir";
  Serial.print("[Firebase] Mengupdate data terkini di ");
  Serial.println(pathTerakhir);

  bool updateSuccess = false;
  int retryCount = 0;
  while (retryCount < 2)
  {
    if (Firebase.setJSON(fbdo, pathTerakhir.c_str(), json))
    {
      updateSuccess = true;
      break;
    }
    retryCount++;
    Serial.print("[Firebase] Gagal update data terkini. Alasan: ");
    Serial.println(fbdo.errorReason());
    if (retryCount < 2)
    {
      Serial.println("[Firebase] Mencoba kembali dalam 1.5 detik...");
      delay(1500);
    }
  }

  if (updateSuccess)
  {
    Serial.println("[Firebase] Data terkini berhasil diperbarui!");
  }
  else
  {
    Serial.println("[Firebase] Gagal update data terkini setelah percobaan ulang.");
  }

  // 2) Simpan ke history (append) dengan retry
  String pathHistory = String("/tanaman_list/") + targetTanamanID + "/history_data";
  Serial.print("[Firebase] Menambahkan history data ke ");
  Serial.println(pathHistory);

  bool pushSuccess = false;
  retryCount = 0;
  while (retryCount < 2)
  {
    if (Firebase.pushJSON(fbdo, pathHistory.c_str(), json))
    {
      pushSuccess = true;
      break;
    }
    retryCount++;
    Serial.print("[Firebase] Gagal push history. Alasan: ");
    Serial.println(fbdo.errorReason());
    if (retryCount < 2)
    {
      Serial.println("[Firebase] Mencoba kembali dalam 1.5 detik...");
      delay(1500);
    }
  }

  if (pushSuccess)
  {
    Serial.println("[Firebase] History data berhasil ditambahkan!");
  }
  else
  {
    Serial.println("[Firebase] Gagal push history setelah percobaan ulang.");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== BME688 & Firebase Project ===");

  // WiFiManager: otomatis konek atau buka captive portal
  WiFiManager wm;
  wm.autoConnect("Smart-Plant-Monitor");

  Serial.println("[WiFi] Terhubung via WiFiManager!");
  Serial.print("[WiFi] IP Address: ");
  Serial.println(WiFi.localIP());

  // NTP Server Config (timezone Asia/Jakarta GMT+7)
  configTime(7 * 3600, 0, "pool.ntp.org");
  Serial.println("[NTP] Memulai sinkronisasi waktu...");
  
  // Tunggu sinkronisasi NTP selesai (time(nullptr) mengembalikan unix timestamp valid)
  time_t now = time(nullptr);
  int ntpTimeout = 0;
  while (now < 1000000000 && ntpTimeout < 20)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    ntpTimeout++;
  }
  Serial.println();
  if (now >= 1000000000) {
    Serial.print("[NTP] Waktu tersinkronisasi: ");
    Serial.println(ctime(&now));
  } else {
    Serial.println("[WARN] Gagal mendapatkan waktu NTP (Timeout).");
  }

  // Inisialisasi Konfigurasi Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  config.timeout.serverResponse = 10 * 1000;  // 10 detik (default terlalu pendek)
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("[OK] Firebase Siap!");
  Serial.println("==========================");
}

void loop()
{
  unsigned long currentMillis = millis();

  // Jalankan logika pembacaan status dan data sensor setiap 15 detik
  if (currentMillis - lastSendTime >= sendInterval)
  {
    lastSendTime = currentMillis;

    if (WiFi.status() == WL_CONNECTED)
    {
      // A. Ambil ID Tanaman target secara dinamis dari Firebase terlebih dahulu
      String pathDeviceMapping = String("/devices/") + DEVICE_ID + "/target_tanaman_id";
      Serial.print("[Firebase] Mendapatkan target tanaman dari ");
      Serial.println(pathDeviceMapping);

      if (Firebase.getString(fbdo, pathDeviceMapping.c_str()))
      {
        String newTanamanID = fbdo.stringData();
        if (newTanamanID != targetTanamanID)
        {
          targetTanamanID = newTanamanID;
          hasFetchedBenchmark = false; // Reset agar benchmark_gas diambil ulang untuk tanaman baru
        }
        Serial.printf("[Firebase] Device dipetakan ke ID tanaman: %s\n", targetTanamanID.c_str());
      }
      else
      {
        Serial.print("[Firebase] Gagal mendapatkan target tanaman. Alasan: ");
        Serial.println(fbdo.errorReason());
      }

      // Jalankan proses sensor hanya jika targetTanamanID tidak kosong
      if (targetTanamanID != "")
      {
        String pathStatus = String("/tanaman_list/") + targetTanamanID + "/status_sensor_aktif";
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
      else
      {
        Serial.println("[WARN] Target tanaman kosong di Firebase. Mode Standby.");
      }
    }
  }

  // Tampilkan data ke Serial Monitor setiap 3 detik
  static unsigned long lastPrintTime = 0;
  if (currentMillis - lastPrintTime >= 3000)
  {
    lastPrintTime = currentMillis;

    Serial.println("---------------------------");
    Serial.printf("Device ID     : %s\n", DEVICE_ID);
    Serial.printf("Target Tanaman: %s\n", (targetTanamanID == "") ? "Belum diset" : targetTanamanID.c_str());
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