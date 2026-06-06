#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// Definisi Kredensial WiFi dan Firebase (Silakan sesuaikan dengan punya Anda)
#define WIFI_SSID "NAMA_WIFI_KAMU"
#define WIFI_PASSWORD "PASSWORD_WIFI_KAMU"
#define FIREBASE_HOST "URL_DATABASE_KAMU.firebaseio.com" // URL database tanpa https:// dan tanpa tanda / di akhir
#define FIREBASE_AUTH "DATABASE_SECRET_KAMU"             // Database Secret

// Inisialisasi Objek Sensor dan Firebase
Adafruit_BME680 bme;
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 15000; // Interval pengiriman data 15 detik

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

  Serial.println("[Firebase] Mengirim data ke /monitoring/tanaman_1...");
  if (Firebase.setJSON(fbdo, "/monitoring/tanaman_1", json))
  {
    Serial.println("[Firebase] Data berhasil terkirim!");
  }
  else
  {
    Serial.print("[Firebase] Gagal terkirim. Alasan: ");
    Serial.println(fbdo.errorReason());
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== BME688 & Firebase Project ===");

  // Inisialisasi Sensor BME688
  if (!bme.begin(0x77))
  { // Ganti ke 0x76 jika SDO disambungkan ke GND
    Serial.println("[ERROR] Sensor tidak ditemukan! Periksa wiring.");
    while (1)
      ;
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C selama 150ms

  Serial.println("[OK] Sensor BME688 Siap!");

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

  // Pembacaan sensor secara kontinu agar heater tetap aktif/hangat
  if (!bme.performReading())
  {
    Serial.println("[ERROR] Gagal membaca sensor.");
    delay(1000);
    return;
  }

  unsigned long currentMillis = millis();

  // Tampilkan data ke Serial Monitor setiap 3 detik agar responsif
  static unsigned long lastPrintTime = 0;
  if (currentMillis - lastPrintTime >= 3000)
  {
    lastPrintTime = currentMillis;

    Serial.println("---------------------------");
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
    Serial.println("---------------------------");
  }

  // Kirim data ke Firebase setiap 15 detik
  if (currentMillis - lastSendTime >= sendInterval)
  {
    lastSendTime = currentMillis;

    float temp = bme.temperature;
    float hum = bme.humidity;
    float pres = bme.pressure / 100.0;
    float gas = (bme.gas_resistance == 0) ? -1.0 : (float)bme.gas_resistance;

    sendDataToFirebase(temp, hum, pres, gas);
  }
}