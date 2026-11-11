# Dual-Sensor-High-G-Impact-Meter-and-Threshold-Based-Early-Warning-System-for-Combat-Sports



Dövüş sporlarında kafa darbelerini anlık ölçen, giyilebilir darbe ölçer ve triyaj sistemi (Bitirme projesi).

## Blok Şeması

![Malzeme blok şeması](Malzeme_Blok_Semasi.png)

PDF hâli: [Malzeme_Blok_Semasi.pdf](Malzeme_Blok_Semasi.pdf)

## Malzeme Durum Tablosu

| Parça | Blok şemadaki görevi | Durum | Eksikse yapılacak iş | Yedek seçenek (ölçüm aralığı, arayüz) |
|---|---|---|---|---|
| Adafruit ADXL375 (5374) | Asıl darbe sensörü, yüksek g ivme ölçümü | ⏳ Sipariş edildi | Kargoyu takip et. Gelince header lehimle, SPI testi yap (DEVID 0xE5, durağanken Z ≈ 20 LSB) | H3LIS331DL: ±100/200/400 g, SPI/I2C. Yazılım geliştirme için ADXL345: ±16 g, SPI/I2C |
| ESP32-C3 SuperMini (×2) | #1 sensör düğümü (veri okuma, tetik, ESP-NOW gönderici); #2 alıcı (USB ile bilgisayara) | ✅ Elimde | — | ESP32-S3 geliştirme kartı: ESP-NOW, SPI/I2C |
| GY-521 MPU6050 | Dönme (jiroskop) verisi + ikincil ivme | ✅ Elimde | — | LSM6DSO32: ivme ±32 g, jiroskop ±2000 °/s, SPI/I2C |
| USB-C veri kablosu | Programlama, seri port, alıcı-bilgisayar bağlantısı | ✅ Elimde | — | Veri hatlı başka bir USB-C kablo |
| Breadboard, 830 delik | Lehimsiz ilk kurulum | ✅ Elimde | — | 400 delikli breadboard |
| Jumper kablo seti | Breadboard bağlantıları | ✅ Elimde | — | ___ |
| 2.54 mm erkek header pin | Kartları breadboard'a takmak için | ✅ Elimde | — | 2.54 mm dişi header (soket) |
| Perfboard, 5×7 cm | Kalıcı prototip | ✅ Elimde | — | Farklı boyutta 2.54 mm delikli plaket |
| Silikon kaplı kablo, 26 AWG | Kalıcı, esnek bağlantılar | ✅ Elimde | — | 24-28 AWG çok damarlı kablo |
| Sertec Li-Po 3.7 V 450 mAh, korumalı | Sensör düğümünün güç kaynağı | ✅ Elimde | — | 3.7 V 1S Li-Po, korumalı, 500 mAh civarı |
| TP4056 Type-C korumalı modül | Pil şarjı + aşırı deşarj/kısa devre koruması | ✅ Elimde | — | 1S Li-Po şarj modülü, şarj akımı ayarlanabilir (___) |
| IC-205 kaydırmalı anahtar | Açma/kapama (OUT+ → ESP32 5V) | ✅ Elimde | — | SPDT kaydırmalı anahtar, akım değeri ___ |
| 4.7 kΩ SMD direnç (0805/0603) | TP4056 şarj akımını ~255 mA'e indirmek | ✅ Elimde | — | 4.7 kΩ 1/4 W bacaklı direnç |

Excel hâli: [Malzeme_Durum_Tablosu.xlsx](Malzeme_Durum_Tablosu.xlsx)

## ADXL375 Gelmeden Yapılacaklar

- [ ] **Çalışma ortamını kur:** Arduino IDE'ye ESP32 paketini yükle, iki ESP32-C3'e de `01_merhaba.ino` yükleyip seri monitörde çalıştığını gör.
- [ ] **Lehim ve güç rayları:** ESP32-C3'lere ve GY-521'e header pinleri lehimle. ESP32'yi breadboard'a tak, 3V3/GND'yi raylara bağla ve 3.3 V ölç.
- [ ] **MPU6050'yi test et:** I2C ile bağla (SDA → GPIO8, SCL → GPIO9). Tarayıcıda 0x68'i gör, kart masada dururken az ≈ +1 g okuduğunu doğrula.
- [ ] **ESP-NOW hattını test et:** Verici ve alıcı kodlarını yükle. MPU6050 verisinin kablosuz gelip gelmediğine ve kayıp paket sayısına bak.
- [ ] **Güç hattını hazırla:** TP4056'daki PROG direncini 4.7 kΩ ile değiştir, pilin gerilimini ve kutbunu ölç, şarj akımını kontrol et. Pili ESP32'ye henüz bağlama.

