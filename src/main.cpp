#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== BME688 Sensor Init ===");

  // Inisialisasi I2C dengan alamat 0x77 (default Adafruit)
  // Ganti ke 0x76 jika pin SDO sensor disambungkan ke GND
  if (!bme.begin(0x77)) {
    Serial.println("[ERROR] Sensor BME688 tidak ditemukan! Periksa wiring.");
    while (1);
  }

  // Konfigurasi oversampling dan filter
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);  // Target pemanasan gas sensor: 320°C selama 150ms

  Serial.println("[OK] Sensor siap!");
  Serial.println("==========================");
}

void loop() {
  if (!bme.performReading()) {
    Serial.println("[ERROR] Gagal membaca sensor.");
    delay(3000);
    return;
  }

  Serial.println("---------------------------");
  Serial.printf("Temperatur    : %.2f °C\n",    bme.temperature);
  Serial.printf("Kelembapan    : %.2f %%\n",    bme.humidity);
  Serial.printf("Tekanan Udara : %.2f hPa\n",   bme.pressure / 100.0);
  Serial.printf("Gas Resistance: %.0f Ohm\n",   bme.gas_resistance);
  Serial.println("---------------------------");

  delay(3000);
}