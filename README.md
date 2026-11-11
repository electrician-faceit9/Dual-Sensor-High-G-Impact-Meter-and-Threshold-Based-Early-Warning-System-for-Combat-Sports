# Dual-Sensor-High-G-Impact-Meter-and-Threshold-Based-Early-Warning-System-for-Combat-Sports
Malzeme_Blok_Semasi.pdf
# Dual-Sensor High-G Impact Meter and Threshold-Based Early Warning System for Combat Sports

Dövüş sporlarında kafa darbelerini anlık ölçen, giyilebilir darbe ölçer ve triyaj sistemi (Bitirme projesi).

## Blok Şeması

![Malzeme blok şeması](Malzeme_Blok_Semasi.png)

PDF hâli: [Malzeme_Blok_Semasi.pdf](Malzeme_Blok_Semasi.pdf)

## Malzeme Listesi

| # | Malzeme | Adet | Kullanım |
|---|---|---|---|
| 1 | Adafruit ADXL375 ±200 g ivmeölçer (5374) | 1 | Asıl darbe sensörü, SPI |
| 2 | ESP32-C3 SuperMini (USB-C) | 2 | Sensör düğümü + alıcı (ESP-NOW) |
| 3 | GY-521 MPU6050 (6 eksen) | 1 | Jiroskop verisi, I2C |
| 4 | USB-C veri kablosu | 2 | Programlama, seri port |
| 5 | Breadboard, 830 delik | 1 | İlk testler |
| 6 | Jumper kablo seti, 20 cm | 1'er set | Breadboard bağlantıları |
| 7 | 2.54 mm erkek header pin şeridi | 2-3 | Kartlara lehim |
| 8 | Perfboard, 5×7 cm | 1-2 | Kalıcı prototip |
| 9 | Silikon kaplı kablo, 26 AWG | 1 makara | Kalıcı bağlantılar |
| 10 | Li-Po 3.7 V 450 mAh, korumalı | 1 | Taşınabilir güç |
| 11 | TP4056 Type-C korumalı şarj modülü | 2 | Pil şarjı ve koruma |
| 12 | Kaydırmalı anahtar IC-205 | 3-5 | Açma/kapama |
| 13 | 4.7 kΩ direnç | 5 | TP4056 şarj akımı ayarı |

Ayrıntılı liste: [Malzeme_Listesi.xlsx](Malzeme_Listesi.xlsx) · [Elektronik_Malzeme_Listesi.pdf](Elektronik_Malzeme_Listesi.pdf)
