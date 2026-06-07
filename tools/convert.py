import os
import json
import csv
from datetime import datetime

def convert_json_to_csv():
    # 1. Meminta input nama file JSON
    json_filename = input("Masukkan nama file JSON input (misal: tomat_sehat.json): ").strip()
    if not json_filename:
        print("[ERROR] Nama file JSON tidak boleh kosong.")
        return

    # 2. Meminta input nama file CSV output
    csv_filename = input("Masukkan nama file CSV output (misal: tomat_sehat.csv): ").strip()
    if not csv_filename:
        print("[ERROR] Nama file CSV tidak boleh kosong.")
        return

    # 3. Validasi & Membaca File JSON
    if not os.path.exists(json_filename):
        print(f"[ERROR] File '{json_filename}' tidak ditemukan.")
        return

    try:
        with open(json_filename, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"[ERROR] Gagal membaca JSON. Format file tidak valid: {e}")
        return
    except Exception as e:
        print(f"[ERROR] Terjadi kesalahan saat membuka file: {e}")
        return

    # 4. Parsing data JSON sesuai struktur
    # Format: { "tanaman_id": { "nama": "...", "history_data": { "timestamp": { ... } } } }
    rows_to_write = []
    row_count = 0

    try:
        for tanaman_id, tanaman_info in data.items():
            if not isinstance(tanaman_info, dict):
                continue
            
            history_data = tanaman_info.get("history_data")
            if not history_data or not isinstance(history_data, dict):
                continue

            for timestamp_str, sensor_data in history_data.items():
                if not isinstance(sensor_data, dict):
                    continue

                try:
                    unix_timestamp = int(timestamp_str)
                    waktu_lokal = datetime.fromtimestamp(unix_timestamp).strftime('%Y-%m-%d %H:%M:%S')
                except ValueError:
                    waktu_lokal = "Invalid Timestamp"

                row_count += 1
                
                # Mengambil nilai sensor dengan default default -
                temperatur = sensor_data.get("temperatur", "-")
                kelembapan = sensor_data.get("kelembapan", "-")
                tekanan_udara = sensor_data.get("tekanan_udara", "-")
                gas_resistance = sensor_data.get("gas_resistance", "-")

                rows_to_write.append([
                    row_count,
                    timestamp_str,
                    waktu_lokal,
                    temperatur,
                    kelembapan,
                    tekanan_udara,
                    gas_resistance
                ])

    except Exception as e:
        print(f"[ERROR] Format data di dalam JSON tidak sesuai dengan spesifikasi: {e}")
        return

    if not rows_to_write:
        print("[WARNING] Tidak ada data history pembacaan sensor yang ditemukan dalam JSON.")
        return

    # 5. Menulis data ke file CSV
    headers = [
        "No", 
        "Unix_Timestamp", 
        "Waktu_Lokal", 
        "Temperatur_C", 
        "Kelembapan_Persen", 
        "Tekanan_hPa", 
        "Gas_Resistance_Ohm"
    ]

    try:
        # Buat folder output jika file CSV ditulis di subfolder
        output_dir = os.path.dirname(csv_filename)
        if output_dir and not os.path.exists(output_dir):
            os.makedirs(output_dir, exist_ok=True)

        with open(csv_filename, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(headers)
            writer.writerows(rows_to_write)
        
        print(f"[SUCCESS] Berhasil mengonversi {len(rows_to_write)} data ke '{csv_filename}'!")
    except Exception as e:
        print(f"[ERROR] Gagal menulis ke file CSV: {e}")

if __name__ == "__main__":
    convert_json_to_csv()
