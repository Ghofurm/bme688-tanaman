import os
import json
import csv
from datetime import datetime

def convert_json_to_csv():
    # 1. Meminta input nama file JSON
    json_filename = input("Masukkan nama file JSON input (misal: export.json): ").strip()
    if not json_filename:
        print("[ERROR] Nama file JSON tidak boleh kosong.")
        return

    # 2. Validasi & Membaca File JSON
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

    # Cek apakah root berisi tanaman_list atau langsung tanaman_id
    tanaman_list = data
    if "tanaman_list" in data:
        tanaman_list = data["tanaman_list"]

    if not isinstance(tanaman_list, dict):
        print("[ERROR] Format data JSON salah. Harus berupa list/object spesimen tanaman.")
        return

    # Tentukan folder output
    output_dir = "dataset_output"
    os.makedirs(output_dir, exist_ok=True)

    # 3. Proses tiap tanaman_id secara terpisah
    for tanaman_id, tanaman_info in tanaman_list.items():
        if not isinstance(tanaman_info, dict):
            continue

        history_data = tanaman_info.get("history_data")
        if not history_data or not isinstance(history_data, dict):
            print(f"[INFO] Tanaman '{tanaman_id}' tidak memiliki history_data. Dilewati.")
            continue

        # Ekstraksi semua keys secara dinamis dari seluruh history_data tanaman ini
        all_sensor_keys = set()
        for timestamp_key, sensor_data in history_data.items():
            if isinstance(sensor_data, dict):
                for key in sensor_data.keys():
                    if key != "timestamp":  # timestamp diatur manual di depan
                        all_sensor_keys.add(key)

        # Urutkan kunci secara logis: parameter standar lalu parameter advanced / ML
        preferred_order = ["temperatur", "kelembapan", "tekanan_udara", "gas_resistance", "gas_log", "gas_delta", "abs_humidity"]
        sorted_keys = []
        # masukkan preferred keys jika ada di sensor data
        for k in preferred_order:
            if k in all_sensor_keys:
                sorted_keys.append(k)
        # masukkan sisa keys yang lain (jika ada masa depan)
        for k in sorted(all_sensor_keys):
            if k not in sorted_keys:
                sorted_keys.append(k)

        # Susun headers
        headers = ["No", "Unix_Timestamp", "Waktu_Format"] + sorted_keys

        # Parsing rows
        rows_to_write = []
        row_count = 0

        # Sort berdasarkan timestamp string atau numeric
        sorted_timestamps = sorted(history_data.keys(), key=lambda x: int(x) if x.isdigit() else 0)

        for ts_str in sorted_timestamps:
            sensor_data = history_data[ts_str]
            if not isinstance(sensor_data, dict):
                continue

            row_count += 1
            
            # Normalisasi timestamp pintar
            try:
                unix_ts = int(ts_str)
                # Ambil juga parameter timestamp dari dalam object jika ada, prioritaskan key ts_str
                if unix_ts > 1000000000:
                    waktu_lokal = datetime.fromtimestamp(unix_ts).strftime('%Y-%m-%d %H:%M:%S')
                else:
                    waktu_lokal = f"Detik ke-{unix_ts} (Sesi Dingin)"
            except ValueError:
                # Coba baca key 'timestamp' di dalam sensor_data jika key luar bukan int
                inner_ts = sensor_data.get("timestamp")
                if isinstance(inner_ts, int) and inner_ts > 1000000000:
                    unix_ts = inner_ts
                    waktu_lokal = datetime.fromtimestamp(inner_ts).strftime('%Y-%m-%d %H:%M:%S')
                elif isinstance(inner_ts, int):
                    unix_ts = inner_ts
                    waktu_lokal = f"Detik ke-{inner_ts} (Sesi Dingin)"
                else:
                    unix_ts = ts_str
                    waktu_lokal = "Invalid Timestamp"

            row = [row_count, unix_ts, waktu_lokal]
            
            # Tambahkan nilai sesuai sorted_keys
            for key in sorted_keys:
                row.append(sensor_data.get(key, "-"))

            rows_to_write.append(row)

        if not rows_to_write:
            continue

        # Tulis ke file terpisah
        csv_filename = os.path.join(output_dir, f"dataset_{tanaman_id}.csv")
        try:
            with open(csv_filename, 'w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(headers)
                writer.writerows(rows_to_write)
            print(f"[SUCCESS] Spesimen '{tanaman_id}' -> {len(rows_to_write)} baris ke '{csv_filename}'")
        except Exception as e:
            print(f"[ERROR] Gagal menulis dataset tanaman '{tanaman_id}': {e}")

if __name__ == "__main__":
    convert_json_to_csv()
