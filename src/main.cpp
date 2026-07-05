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

// Variabel untuk melacak status koneksi
int firebaseFailCount = 0;
const int maxFirebaseFails = 3;
int currentWiFiIndex = -1;

// Fungsi untuk sinkronisasi NTP
void syncNTP()
{
  configTime(7 * 3600, 0, "pool.ntp.org");
  Serial.println("[NTP] Memulai sinkronisasi waktu...");
  
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
}

// Fungsi untuk mencari dan menghubungkan ke WiFi terbaik yang tersedia
bool connectToBestWiFi()
{
  Serial.println("[WiFi] Melakukan scanning jaringan...");
  int n = WiFi.scanNetworks();
  Serial.printf("[WiFi] Scan selesai, %d jaringan ditemukan.\n", n);

  if (n == 0)
  {
    Serial.println("[WiFi] Tidak ada jaringan WiFi yang ditemukan.");
    return false;
  }

  // Cari WiFi dari WIFI_LIST yang ada di hasil scan
  for (int i = 0; i < WIFI_COUNT; i++)
  {
    for (int j = 0; j < n; j++)
    {
      if (WiFi.SSID(j) == WIFI_LIST[i].ssid)
      {
        Serial.printf("[WiFi] Menemukan jaringan cocok: %s. Mencoba menghubungkan...\n", WIFI_LIST[i].ssid);
        
        WiFi.disconnect();
        WiFi.begin(WIFI_LIST[i].ssid, WIFI_LIST[i].password);

        // Tunggu koneksi
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20)
        {
          delay(500);
          Serial.print(".");
          retries++;
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED)
        {
          currentWiFiIndex = i;
          Serial.printf("[WiFi] Berhasil terhubung ke: %s!\n", WIFI_LIST[i].ssid);
          Serial.print("[WiFi] IP Address: ");
          Serial.println(WiFi.localIP());
          syncNTP();
          return true;
        }
        else
        {
          Serial.printf("[WiFi] Gagal terhubung ke: %s\n", WIFI_LIST[i].ssid);
        }
      }
    }
  }

  return false;
}

// Fallback jika tidak ada WiFi dari WIFI_LIST yang terhubung
void startFallbackPortal()
{
  Serial.println("[WiFi] Memulai WiFiManager Captive Portal sebagai fallback...");
  WiFiManager wm;
  // autoConnect akan memblokir sampai terhubung ke WiFi baru via AP Portal
  if (wm.autoConnect("Smart-Plant-Monitor"))
  {
    Serial.println("[WiFi] Terhubung via WiFiManager!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    syncNTP();
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
bool sendDataToFirebase(float temp, float hum, float pres, float gas)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[Firebase] Gagal mengirim, WiFi tidak terhubung.");
    return false;
  }

  if (targetTanamanID == "")
  {
    Serial.println("[Firebase] Gagal mengirim, ID tanaman target belum diset.");
    return false;
  }

  // A. Ambil nilai benchmark_gas dari Firebase
  String pathBenchmark = String("/tanaman_list/") + targetTanamanID + "/benchmark_gas";
  if (Firebase.getDouble(fbdo, pathBenchmark.c_str()))
  {
    benchmarkGas = fbdo.doubleData();
    Serial.printf("[Firebase] Benchmark Gas: %.2f Ohm\n", benchmarkGas);
  }
  else
  {
    Serial.print("[Firebase] Gagal mengambil benchmark_gas. Menggunakan default. Alasan: ");
    Serial.println(fbdo.errorReason());
    return false;
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

  // 1) Update data terkini (overwrite)
  String pathTerakhir = String("/tanaman_list/") + targetTanamanID + "/data_terakhir";
  Serial.print("[Firebase] Mengupdate data terkini di ");
  Serial.println(pathTerakhir);

  if (!Firebase.setJSON(fbdo, pathTerakhir.c_str(), json))
  {
    Serial.print("[Firebase] Gagal update data terkini. Alasan: ");
    Serial.println(fbdo.errorReason());
    return false;
  }
  Serial.println("[Firebase] Data terkini berhasil diperbarui!");

  // 2) Simpan ke history (append)
  String pathHistory = String("/tanaman_list/") + targetTanamanID + "/history_data";
  Serial.print("[Firebase] Menambahkan history data ke ");
  Serial.println(pathHistory);

  if (!Firebase.pushJSON(fbdo, pathHistory.c_str(), json))
  {
    Serial.print("[Firebase] Gagal push history. Alasan: ");
    Serial.println(fbdo.errorReason());
    return false;
  }
  Serial.println("[Firebase] History data berhasil ditambahkan!");
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== BME688 & Firebase Project ===");

  // Set mode WiFi station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Mulai koneksi ke WiFi terbaik dari daftar
  if (!connectToBestWiFi())
  {
    // Jika tidak ada WiFi dari daftar yang terhubung, gunakan WiFiManager
    startFallbackPortal();
  }

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
  unsigned long currentMillis = millis();

  // Pastikan koneksi WiFi tetap aktif
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[WiFi] Koneksi terputus! Mencoba menghubungkan kembali...");
    if (!connectToBestWiFi())
    {
      // Berikan jeda agar tidak spam scan jika semua WiFi mati
      delay(5000);
      return;
    }
  }

  // Jalankan logika pembacaan status dan data sensor setiap 15 detik
  if (currentMillis - lastSendTime >= sendInterval)
  {
    lastSendTime = currentMillis;

    // A. Ambil ID Tanaman target secara dinamis dari Firebase terlebih dahulu
    String pathDeviceMapping = String("/devices/") + DEVICE_ID + "/target_tanaman_id";
    Serial.print("[Firebase] Mendapatkan target tanaman dari ");
    Serial.println(pathDeviceMapping);

    if (Firebase.getString(fbdo, pathDeviceMapping.c_str()))
    {
      targetTanamanID = fbdo.stringData();
      Serial.printf("[Firebase] Device dipetakan ke ID tanaman: %s\n", targetTanamanID.c_str());
      firebaseFailCount = 0; // Reset counter karena berhasil terkoneksi ke Firebase
    }
    else
    {
      Serial.print("[Firebase] Gagal mendapatkan target tanaman. Alasan: ");
      Serial.println(fbdo.errorReason());
      firebaseFailCount++;
    }

    // Jalankan proses sensor hanya jika targetTanamanID tidak kosong
    if (targetTanamanID != "" && firebaseFailCount == 0)
    {
      // Baca status aktif dan parameter timer dari Firebase
      bool success = true;
      int statusVal = 0;
      int timerDuration = 0;
      int timerStart = 0;

      String pathStatus = String("/tanaman_list/") + targetTanamanID + "/status_sensor_aktif";
      if (Firebase.getInt(fbdo, pathStatus.c_str())) {
        statusVal = fbdo.intData();
      } else {
        success = false;
      }

      String pathTimerDuration = String("/tanaman_list/") + targetTanamanID + "/sensor_timer_duration";
      if (Firebase.getInt(fbdo, pathTimerDuration.c_str())) {
        timerDuration = fbdo.intData();
      } else {
        timerDuration = 0; // Default
      }

      String pathTimerStart = String("/tanaman_list/") + targetTanamanID + "/sensor_timer_start";
      if (Firebase.getInt(fbdo, pathTimerStart.c_str())) {
        timerStart = fbdo.intData();
      } else {
        timerStart = 0; // Default
      }

      if (success)
      {
        // Cek jika timer aktif dan waktu sudah habis
        if (statusVal == 1 && timerDuration > 0 && timerStart > 0)
        {
          time_t now = time(nullptr);
          if (now >= (timerStart + timerDuration))
          {
            Serial.println("[Timer] Waktu habis! Mematikan sensor...");
            statusVal = 0;
            
            // Update status di Firebase
            Firebase.setInt(fbdo, pathStatus.c_str(), 0);
            Firebase.setInt(fbdo, pathTimerDuration.c_str(), 0);
            Firebase.setInt(fbdo, pathTimerStart.c_str(), 0);
            
            // Update pemetaan perangkat (opsional, tergantung logic)
            String pathDeviceMapping = String("/devices/") + DEVICE_ID + "/target_tanaman_id";
            Firebase.setString(fbdo, pathDeviceMapping.c_str(), "");
          }
        }

        if (statusVal == 0)
        {
          if (isSensorInitialized)
          {
            Serial.println("Sensor dimatikan. Standby mode...");
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

              if (sendDataToFirebase(temp, hum, pres, gas))
              {
                firebaseFailCount = 0; // Reset
              }
              else
              {
                firebaseFailCount++;
              }
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
        Serial.print("[Firebase] Gagal membaca status dari database. Alasan: ");
        Serial.println(fbdo.errorReason());
        firebaseFailCount++;
      }
    }
    else if (targetTanamanID == "")
    {
      Serial.println("[WARN] Target tanaman kosong di Firebase. Mode Standby.");
    }

    // Jika terjadi kegagalan Firebase berulang kali, kemungkinan WiFi bermasalah/lemot.
    // Lakukan pemindahan WiFi (Switching).
    if (firebaseFailCount >= maxFirebaseFails)
    {
      Serial.printf("[Firebase] Gagal berturut-turut %d kali. Mengganti WiFi...\n", firebaseFailCount);
      firebaseFailCount = 0; // Reset counter
      WiFi.disconnect();
      delay(500);
      connectToBestWiFi();
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