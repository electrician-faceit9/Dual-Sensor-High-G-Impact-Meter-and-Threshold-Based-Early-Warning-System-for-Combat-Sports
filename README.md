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

Literatür bölümü için 5 cümlelik özet

İncelenen çalışmalarda giyilebilir ataletsel algılayıcılarla düşme ve darbe tespiti yaygın olmakla birlikte, doğrulama çoğunlukla sınıflandırma başarımı ve gecikme ölçütleriyle yapılmış, algılayıcının ölçtüğü ivmenin bir referansa göre doğruluğu nadiren raporlanmıştır [4], [5]. Kask tabanlı bir IoT sistemi darbe ve düşmeyi algılayıp gerçek zamanlı uyarı göndermekte, ancak ivme ölçümünün doğruluğuna ilişkin bir değerlendirme sunmamaktadır [1]. Tepe kafa ivmesi ve HIC15 gibi kafa yaralanması ölçütleri araç-yaya çarpışması bağlamında çok gövdeli model ve manken deneyleriyle incelenmiş, giyilebilir bir algılayıcıyla ölçülmemiştir [2]. Referans ivmeölçerle algılayıcı düzeyinde metrolojik karakterizasyon yapan çalışma düşük genlik (1–4 m/s²) ve düşük frekans (1–8 Hz) aralığıyla sınırlı kalmış [3], yüksek g darbeler altında doyma ve ölçüm aralığı davranışı ise algılayıcı tasarımı çalışmalarında ele alınmamıştır [6]. Bu proje, ±200 g’lik bir ivmeölçeri kontrollü düşürme deneylerinde düşme yüksekliğinden hesaplanan çarpma hızına göre doğrulayarak ve doyan olayları işaretleyerek, incelenen çalışmalardaki bu boşluğun algılayıcı düzeyindeki kısmına yanıt vermeyi amaçlamaktadır.

Kaynak listesi

Yazar adları tabloda yok, o yüzden ___ bıraktım. DOI’leri senin mesajındaki bağlantılardan aldım, bağlantısı olmayanlarda ___ yazdım.

[1] ___, “Intelligent Life Guard: An IoT-Enabled Smart Helmet for Vital Monitoring & Fall Detection,” International Journal of Drug Delivery Technology, 2026. doi: 10.25258/ijddt.16.35s.5

[2] ___, “Contact-Triggered Active Bonnet System for Pedestrian Head Injury Mitigation: Multibody Modelling and Experimental Validation,” Sensors, 2026. doi: 10.3390/s26154743

[3] ___, “Simultaneous Bench-Based Metrological Characterization of Smartwatches’ Accelerometers for Accurate Measurement,” Technologies, 2026. doi: ___

[4] ___, “Wearable Fall Detection System with Real-Time Localization and Notification Capabilities,” Sensors, 2025. doi: ___

[5] ___, “Machine Learning-Based Framework for Pre-Impact Same-Level Fall and Fall-from-Height Detection in Construction Sites Using a Single Wearable Inertial Measurement Unit,” Biosensors, 2025. doi: 10.3390/bios15090618

[6] ___, “Square-ring mass triaxial piezoresistive accelerometer with decoupled sensing elements for matched three-axis performance,” Microsystem Technologies, 2026. doi: 10.1007/s00542-026-06162-6
- [ ] **Çalışma ortamını kur:** Arduino IDE'ye ESP32 paketini yükle, iki ESP32-C3'e de `01_merhaba.ino` yükleyip seri monitörde çalıştığını gör.
- [ ] **Lehim ve güç rayları:** ESP32-C3'lere ve GY-521'e header pinleri lehimle. ESP32'yi breadboard'a tak, 3V3/GND'yi raylara bağla ve 3.3 V ölç.
- [ ] **MPU6050'yi test et:** I2C ile bağla (SDA → GPIO8, SCL → GPIO9). Tarayıcıda 0x68'i gör, kart masada dururken az ≈ +1 g okuduğunu doğrula.
- [ ] **ESP-NOW hattını test et:** Verici ve alıcı kodlarını yükle. MPU6050 verisinin kablosuz gelip gelmediğine ve kayıp paket sayısına bak.
- [ ] **Güç hattını hazırla:** TP4056'daki PROG direncini 4.7 kΩ ile değiştir, pilin gerilimini ve kutbunu ölç, şarj akımını kontrol et. Pili ESP32'ye henüz bağlama.

