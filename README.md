# Brankas Pintar Berbasis Keypad dan Sensor Getaran

Simulasi brankas pintar yang menggabungkan keypad, sensor getaran MPU6050, servo, buzzer, dan LED pada ESP32 dengan dashboard web mandiri. Proyek ini dirancang untuk dijalankan secara **full-software menggunakan Wokwi dan Node.js**, sehingga alur dapat dicoba tanpa membeli atau merakit perangkat keras fisik.

Dashboard berkomunikasi dengan simulasi ESP32 melalui MQTT. Sistem dapat menerima status brankas dan mengirim perintah buka, kunci, serta reset alarm.

> **Catatan cakupan:** repositori ini menyediakan sketch ESP32 (`firmware/smart_safe_esp32.ino`) dan dashboard. Berkas proyek Wokwi (misalnya `diagram.json`) dan tautan simulasi spesifik tidak disertakan; rangkaian perlu dibuat atau diimpor ke Wokwi terlebih dahulu.

## Arsitektur Sistem

```text
Status:
Wokwi ESP32 ── MQTT: smartsafe/groupX/data ──> Broker MQTT
                                                    │
                                                    v
Web Dashboard <── HTTP /api/status ── Backend Node.js + Express

Perintah:
Web Dashboard ── HTTP POST /api/control ──> Backend Node.js + Express
                                                    │
                   Broker MQTT <── MQTT: smartsafe/groupX/cmd ──┘
                        │
                        v
                    Wokwi ESP32
```

1. ESP32 di Wokwi membaca keypad dan sensor, mengendalikan servo/buzzer/LED, lalu menerbitkan status JSON ke topik `smartsafe/groupX/data`.
2. Backend Node.js berlangganan topik status tersebut dan menyimpan status terbaru dalam memori.
3. Dashboard mengambil status dari endpoint `GET /api/status` setiap dua detik.
4. Saat pengguna mengirim kontrol, dashboard mengirim `POST /api/control`. Backend menerbitkan perintah JSON ke topik `smartsafe/groupX/cmd`, yang dilanggan oleh ESP32.

Kedua sisi menggunakan broker publik EMQX `broker.emqx.io` pada port MQTT tanpa TLS `1883`. Topik `groupX` harus sama pada sketch dan server. Jika menjalankan beberapa salinan proyek, gunakan nama grup unik dan ubah nilainya pada kedua berkas agar pesan antar-simulasi tidak bercampur.

### Format pesan MQTT

Contoh status yang diterbitkan perangkat:

```json
{
  "lock": "LOCKED",
  "vibration": "NORMAL",
  "alarm": "OFF",
  "pin_errors": 0,
  "last_event": "Brankas siap"
}
```

`lock`, `vibration`, `alarm`, dan `last_event` adalah string. `pin_errors` adalah angka JSON, bukan string. Contoh perintah dari backend:

```json
{"action":"UNLOCK"}
```

Nilai `action` yang didukung adalah `UNLOCK`, `LOCK`, dan `RESET_ALARM`.

## Prasyarat

- **Node.js 18 atau lebih baru** dan npm. Node.js 18+ direkomendasikan untuk lingkungan pengembangan modern.
- Browser modern seperti Google Chrome, Microsoft Edge, atau Mozilla Firefox.
- Koneksi internet untuk mengakses Wokwi, broker MQTT publik, dan stylesheet Bootstrap 5 dari CDN.
- Akun Wokwi jika ingin menyimpan proyek simulasi: [https://wokwi.com/](https://wokwi.com/).
- Komponen simulasi Wokwi: ESP32, keypad 4×4, MPU6050, OLED SSD1306 I²C, servo, buzzer, LED merah, dan LED hijau.

## Menjalankan Web Dashboard

1. Buka terminal pada direktori utama proyek, yaitu direktori yang berisi `package.json`.
2. Pasang dependensi:

   ```bash
   npm install
   ```

3. Jalankan server:

   ```bash
   npm start
   ```

4. Buka [http://localhost:3000](http://localhost:3000) di browser.
5. Biarkan terminal tetap berjalan. Log server akan menunjukkan koneksi ke broker dan status subscription pada topik `smartsafe/groupX/data`.

Dashboard dan backend berjalan sebagai satu proses Node.js. Backend menggunakan Express untuk menyajikan berkas dalam `public/` serta menyediakan API status dan kontrol.

## Menjalankan Simulasi Wokwi

1. Buka [Wokwi](https://wokwi.com/) dan buat proyek ESP32 baru (atau buka proyek Wokwi yang sudah berisi rangkaian).
2. Tambahkan/import sketch dari `firmware/smart_safe_esp32.ino`.
3. Susun dan hubungkan komponen sesuai pin pada sketch:

   | Komponen | Pin ESP32 yang digunakan |
   | --- | --- |
   | Keypad baris 1–4 | GPIO 13, 12, 14, 27 |
   | Keypad kolom 1–4 | GPIO 26, 25, 33, 32 |
   | Servo | GPIO 18 |
   | Buzzer | GPIO 4 |
   | LED merah | GPIO 5 |
   | LED hijau | GPIO 19 |
   | MPU6050 dan OLED SSD1306 | I²C: SDA GPIO 21, SCL GPIO 22 |

4. Pastikan dashboard Node.js sudah berjalan. Sketch menggunakan Wi-Fi simulasi `Wokwi-GUEST` dan broker `broker.emqx.io:1883`.
5. Klik **Play** di Wokwi dan buka Serial Monitor.
6. Tunggu pesan `WiFi connected`, `Connected to MQTT broker`, dan `Subscribed to smartsafe/groupX/cmd`. Jika pesan ini muncul, perangkat telah tersambung dan mendengarkan perintah. Status awal juga akan diterbitkan ke topik data.

Jika repositori atau tugas Anda memiliki tautan proyek Wokwi tersendiri, gunakan tautan tersebut untuk langkah pertama. Tautan spesifik itu belum tercantum di repositori ini.

## Panduan Penggunaan Web

### Kartu status

| Kartu | Arti | Nilai yang diterbitkan sketch saat ini |
| --- | --- | --- |
| **Status Kunci** | Kondisi servo/kunci brankas. | `LOCKED` atau `UNLOCKED` |
| **Getaran** | Apakah MPU6050 mendeteksi getaran melebihi ambang. | `NORMAL` atau `DETECTED` |
| **Alarm** | Apakah alarm aktif. Alarm menyala setelah tiga PIN salah atau getaran terdeteksi. | `OFF` atau `ON` |
| **Kesalahan PIN** | Jumlah kegagalan PIN sejak terakhir direset atau PIN benar. | Angka, misalnya `0`, `1`, atau `3` |

Dashboard saat ini menampilkan nilai MQTT tersebut secara langsung. Dengan demikian, `UNLOCKED`, `DETECTED`, `ON`, dan angka `3` adalah tampilan aktual; padanannya secara makna adalah “TERBUKA”, “TERDETEKSI”, “AKTIF”, dan “3/3”. Dashboard belum menerjemahkan nilai ke label lokal atau menambahkan format batas `3/3`.

### Tombol kontrol

| Tombol pada dashboard | Perintah MQTT | Fungsi |
| --- | --- | --- |
| **Buka Kunci** (buka paksa) | `UNLOCK` | Menggerakkan servo ke posisi buka (90°) dan memperbarui status menjadi `UNLOCKED`. |
| **Kunci** (kunci paksa) | `LOCK` | Menggerakkan servo ke posisi terkunci (0°) dan memperbarui status menjadi `LOCKED`. |
| **Reset Alarm** (hapus alarm) | `RESET_ALARM` | Mematikan alarm/buzzer, mereset penghitung kesalahan PIN ke 0, dan mematikan LED merah. |

Saat tombol ditekan, dashboard menampilkan apakah perintah berhasil dikirim oleh backend. Konfirmasi pengiriman bukan konfirmasi bahwa perangkat telah menerapkan perintah; periksa perubahan status dan perilaku komponen di Wokwi.

### Riwayat aktivitas

- Tabel menampilkan waktu lokal browser dan teks `last_event` dari perangkat.
- Baris baru ditambahkan saat teks peristiwa berubah. Riwayat hanya berada di halaman yang sedang terbuka dan dibatasi hingga 10 baris terbaru; riwayat tidak disimpan permanen.
- Contoh peristiwa meliputi PIN benar/salah, getaran terdeteksi, brankas dikunci, dan alarm direset.

## Skenario Pengujian (Test Cases)

Jalankan server dan simulasi bersamaan. Nilai dalam kolom hasil mengacu pada perilaku sketch dan dashboard saat ini. Status MQTT dikirim berkala setiap dua detik.

| No. | Langkah pengujian | Hasil yang diharapkan |
| --- | --- | --- |
| 1 | Masukkan PIN benar `1234` pada keypad, lalu tekan `#`. | Servo bergerak ke posisi buka (90°); kartu **Status Kunci** menampilkan `UNLOCKED` (maknanya “TERBUKA”). |
| 2 | Masukkan PIN yang salah tiga kali; tekan `#` sesudah setiap PIN. | Alarm/buzzer aktif; kartu **Alarm** menampilkan `ON` (maknanya “AKTIF”) dan **Kesalahan PIN** menampilkan `3`. Tampilan `3/3` belum diterapkan di dashboard. |
| 3 | Goyangkan/gerakkan MPU6050 di Wokwi hingga melewati ambang deteksi. | Kartu **Getaran** menampilkan `DETECTED` dan **Alarm** menampilkan `ON`. Setelah getaran berhenti, status getaran dapat kembali ke `NORMAL`, sedangkan alarm tetap aktif sampai direset. |
| 4 | Klik **Buka Kunci** di dashboard. | Backend mengirim `UNLOCK`; servo bergerak ke posisi buka dan status menjadi `UNLOCKED`. |
| 5 | Klik **Reset Alarm** saat alarm aktif. | Backend mengirim `RESET_ALARM`; buzzer berhenti, status alarm menjadi `OFF`, dan hitungan kesalahan PIN kembali ke `0`. |
| 6 | Hentikan simulasi Wokwi dengan **Stop** sementara backend dan dashboard tetap berjalan. | **Kriteria yang diharapkan:** dashboard menampilkan “Perangkat Terputus”. **Kondisi implementasi saat ini:** deteksi perangkat yang berhenti belum tersedia; backend mempertahankan status MQTT terakhir dan endpoint `/api/status` tetap merespons, sehingga dashboard dapat terus menampilkan status lama dan badge “Terhubung” (koneksi ke backend, bukan perangkat). Tes ini belum lulus sebelum mekanisme heartbeat/timeout ditambahkan. |

## Troubleshooting

### MQTT gagal terhubung di Wokwi

- Pastikan simulasi berjalan dan Serial Monitor menunjukkan `WiFi connected`.
- Pastikan Wi-Fi sketch tetap `Wokwi-GUEST` dengan kata sandi kosong saat digunakan di Wokwi.
- Pastikan broker dan port di sketch benar: `broker.emqx.io`, `1883`.
- Pastikan koneksi internet tersedia dan coba jalankan ulang simulasi. Broker publik dapat mengalami gangguan atau pembatasan sementara.
- Pesan `MQTT connection failed, state=...` menyertakan kode status dari PubSubClient yang dapat membantu diagnosis.

### Web tidak menerima pembaruan

- Pastikan proses `npm start` masih berjalan dan browser membuka `http://localhost:3000`.
- Periksa log server untuk `Connected to MQTT broker` dan `Subscribed to smartsafe/groupX/data`.
- Pastikan nilai topik data di `server.js` dan sketch sama persis: `smartsafe/groupX/data`. Topik MQTT peka terhadap huruf besar/kecil.
- Periksa Serial Monitor untuk koneksi MQTT dan subscription perangkat pada `smartsafe/groupX/cmd`.
- Pastikan sketch menerbitkan JSON valid. `pin_errors` harus berupa angka JSON; tanda kutip yang tidak di-escape pada teks `last_event` dapat membuat JSON tidak valid.
- Pastikan dashboard dapat mengakses internet untuk memuat Bootstrap dari CDN. Kegagalan CDN hanya memengaruhi tampilan, bukan API dashboard.

### Tombol kontrol tidak menggerakkan servo

- Pastikan backend terhubung ke broker; jika tidak, API kontrol akan mengembalikan kesalahan koneksi MQTT.
- Pastikan Serial Monitor menunjukkan perangkat telah subscribe ke `smartsafe/groupX/cmd`.
- Pastikan topik command di server dan sketch sama: `smartsafe/groupX/cmd`.
- Periksa sambungan servo pada GPIO 18 serta pesan Serial Monitor. Pesan bahwa perintah diabaikan dapat menandakan JSON atau nilai `action` tidak sesuai.

### Status tetap tampil setelah simulasi dihentikan

Ini adalah batasan implementasi saat ini. Backend tidak memeriksa kapan status terakhir diterima atau menandai perangkat offline. Oleh karena itu, dashboard dapat menampilkan data terakhir yang diterima; label “Terhubung” hanya menunjukkan bahwa permintaan dashboard ke backend berhasil. Diperlukan heartbeat atau timeout sisi backend dan indikator perangkat tersendiri agar kondisi “Perangkat Terputus” dapat ditampilkan dengan andal.

## Struktur Folder Proyek

```text
.
├── firmware/
│   └── smart_safe_esp32.ino  # Sketch ESP32, kendali perangkat, dan klien MQTT
├── public/
│   └── index.html            # Antarmuka dashboard dan pemanggilan API
├── package.json              # Metadata proyek, dependensi, dan perintah npm
├── package-lock.json         # Versi dependensi yang terkunci
├── server.js                 # Server Express dan jembatan MQTT
└── README.md                 # Dokumentasi proyek
```

Direktori `node_modules/` dibuat oleh `npm install` dan tidak perlu diedit secara manual. Berkas rangkaian Wokwi belum disertakan dalam struktur proyek saat ini.

## Catatan Keamanan dan Batasan

- Broker yang digunakan bersifat publik dan koneksinya tidak dienkripsi. Topik `smartsafe/groupX/...` bukan kanal privat; perangkat atau pengguna lain dapat memublikasikan pesan ke topik yang sama.
- Gunakan proyek ini hanya sebagai simulasi/pembelajaran. Jangan menghubungkannya ke brankas atau sistem keamanan fisik tanpa autentikasi broker, transport terenkripsi, otorisasi perintah, dan pengujian keamanan yang sesuai.
- Status perangkat tidak bertahan setelah proses Node.js dimulai ulang, dan belum ada penyimpanan riwayat permanen.
