#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

Adafruit_BME680 bme;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== BME688 Sensor Init ===");

  if (!bme.begin(0x77))
  { // Ganti 0x77 ke 0x76 jika SDO disambungkan ke GND
    Serial.println("[ERROR] Sensor tidak ditemukan! Periksa wiring.");
    while (1)
      ;
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C selama 150ms

  Serial.println("[OK] Sensor siap!");
  Serial.println("==========================");
}

void loop()
{
  if (!bme.performReading())
  {
    Serial.println("[ERROR] Gagal membaca sensor.");
    delay(1000);
    return;
  }

  Serial.println("---------------------------");
  Serial.printf("Temperatur    : %.2f C\n", bme.temperature);
  Serial.printf("Kelembapan    : %.2f %%\n", bme.humidity);
  Serial.printf("Tekanan Udara : %.2f hPa\n", bme.pressure / 100.0);

  // Jika gas_resistance bernilai 0, berarti heater belum stabil
  if (bme.gas_resistance == 0)
  {
    Serial.println("Gas Resistance: Heater warming up...");
  }
  else
  {
    Serial.printf("Gas Resistance: %.0f Ohm\n", (float)bme.gas_resistance);
  }

  Serial.println("---------------------------");

  delay(1000); // Dikurangi dari 3000ms supaya heater tetap aktif
}