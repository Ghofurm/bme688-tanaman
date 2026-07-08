# IoT Deteksi Penyakit Tanaman 🌱

Proyek ini adalah sistem Internet of Things (IoT) yang dirancang untuk mendeteksi penyakit pada tanaman menggunakan mikrokontroler **ESP32** dan sensor VOC **BME688**. Sistem ini mengumpulkan metrik dari sensor gas/udara yang kemudian akan diproses dan dianalisis menggunakan *machine learning* pada platform **Edge Impulse**.

## 🚀 Fitur Utama
- **Pembacaan Sensor BME688:** Merekam data kualitas udara, VOC (Volatile Organic Compounds), suhu, kelembapan, dan tekanan.
- **Konversi Otomatis ke Dataset AI:** Dilengkapi dengan *script* Python untuk mengonversi data mentah JSON menjadi format CSV.
- **Auto-Split Dataset (80/20):** Saat proses konversi, data langsung dibagi secara otomatis menjadi `_train.csv` (80%) dan `_test.csv` (20%), sehingga output langsung siap (ready-to-use) untuk diunggah ke platform Edge Impulse.

## 📁 Struktur Direktori
- `src/` - Berisi *source code* utama untuk mikrokontroler ESP32 (C/C++).
- `include/` - Berisi file *header* konfigurasi. (Catatan: File kredensial `secrets.h` tidak dilacak oleh Git demi keamanan).
- `tools/` - Kumpulan *script* pendukung.
  - `convert.py` - Script untuk mengonversi dan membagi dataset JSON ke CSV (`train` & `test`).
  - `dataset_output/` - Direktori *output* lokal tempat dataset `.csv` disimpan setelah dikonversi. (Folder ini diabaikan dalam `.gitignore`).

## 🛠️ Persyaratan (Requirements)
- **Perangkat Keras:** ESP32, Sensor BME688.
- **Perangkat Lunak:** 
  - [PlatformIO](https://platformio.org/) (digunakan sebagai *build system* proyek ini).
  - Python 3.x (untuk menjalankan *tools* konversi).

## ⚙️ Cara Menggunakan Alat Konversi (Tools)
1. Pastikan Anda sudah memiliki file data mentah berformat JSON dari alat.
2. Buka terminal dan masuk ke folder proyek Anda.
3. Jalankan script konversi menggunakan perintah berikut:
   ```bash
   python tools/convert.py
   ```
4. Masukkan nama file JSON Anda ketika diminta oleh program.
5. Hasil konversi (`_train.csv` dan `_test.csv` untuk tiap spesimen tanaman) akan secara otomatis tersimpan di dalam folder `tools/dataset_output/`. Output ini siap diunggah ke Edge Impulse!

## 🔐 Keamanan
Pastikan Anda membuat file `include/secrets.h` secara lokal jika ingin mengatur kredensial seperti SSID dan Password WiFi. File ini telah dimasukkan ke dalam `.gitignore` sehingga tidak akan secara tidak sengaja terunggah ke repositori publik.
