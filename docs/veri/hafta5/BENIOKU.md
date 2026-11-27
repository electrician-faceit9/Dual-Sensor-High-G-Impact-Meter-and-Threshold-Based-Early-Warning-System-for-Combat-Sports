# Hafta 5 ham seri port kayıtları

| Dosya | Kart | Kod | İçerik |
|---|---|---|---|
| `sensorlu_verici.txt` | Verici (breadboard, MAC B8:1F:3F:FD:AB:44) | `firmware/04_sensor_paket_verici` | İlk satırlar (sıra 20–511) USB takılınca kendiliğinden başlayan önceki koşuya aittir. Asıl test `# Verici MAC` satırının ikinci geçtiği yerden başlar (sıra 0–999), sonuç bloğu ve kalemle vurma denemesinin `O` satırları en sondadır. |
| `sensorlu_alici.txt` | Alıcı (MAC 48:F6:EE:12:9D:A8) | `firmware/05_sensor_paket_alici` | İlk bölüm önceki koşuya aittir. Asıl test için yalnız sıra 190–999 arası kopyalanabildi (başı eksik). Son özet satırı önceki koşudan gelen paketleri de saydığı için kayıp hesabında kullanılmadı. |
| `kontrol_verici_ozet.txt` | Verici | `firmware/06_espnow_test_verici` | Sensörsüz kontrol testinin sonuç bloğu (yalnız özet kopyalandı). |
| `kontrol_alici.txt` | Alıcı | `firmware/07_espnow_test_alici` | Sensörsüz kontrol testinin alıcı tarafı (sıra 0–999 tam). |

Koşullar: kalabalık bir kafe (3–4 Wi-Fi ağı, çok sayıda elektronik cihaz, kahve makineleri), kartlar arası ~20 cm, verici gücü 8.5 dBm, 29 baytlık paket, 100 ms aralık, 1000 paket. Arduino IDE 2.3.11, esp32 3.3.12.
