# Hafta 4: Sensörsüz ESP-NOW Testi

## Amaç
Sensör bağlamadan, README 5. bölümdeki 29 baytlık ÖZET paketinin (0x01) iki ESP32-C3 arasında kayıpsız ve hızlı gidip gitmediğini ölçmek.

## Düzenek
| Kart | Görev | Port | MAC | Kod |
|---|---|---|---|---|
| Kart 1 | Verici (sahte ÖZET paketi gönderir, yankıyı ölçer) | COM7 | B8:1F:3F:FD:AB:44 | `firmware/06_espnow_test_verici` |
| Kart 2 | Alıcı (paketi alır, yankı gönderir) | COM8 | 48:F6:EE:12:9D:A8 | `firmware/07_espnow_test_alici` |

## Test koşulları
- Gönderilen paket: 1000
- Gönderim aralığı: 100 ms (10 paket/s)
- Paket boyu: 29 bayt
- Verici gücü: 8.5 dBm (`WiFi.setTxPower(WIFI_POWER_8_5dBm)`)
- Mesafe: iki kart yan yana, aynı bilgisayara bağlı
- Arduino-ESP32 çekirdeği: 3.3.12

## Yöntem
İki kartın saatleri birbirinden bağımsız olduğu için alıcıdaki `t_rx − t_tx` farkı mutlak gecikme değildir. Bu yüzden gecikme, vericinin kendi saatiyle gidiş-dönüş süresi (RTT) olarak ölçüldü:

`RTT = (yankının geldiği an − gönderim anı) − alıcıdaki bekleme süresi`

Tek yön gecikme RTT/2 olarak tahmin edildi. Kesin uçtan uca gecikme ileride GPIO + osiloskop ile ölçülecek.

## Sonuçlar
| Ölçüm | Değer |
|---|---|
| Gönderilen / yankı alınan | 1000 / 1000 |
| Kayıp oranı | %0 |
| Gidiş-dönüş (RTT) min / ort / maks | 2.79 / 3.06 / 8.96 ms |
| RTT p50 / p95 / p99 | ≤ 3.00 / ≤ 4.25 / ≤ 5.75 ms |
| Tek yön tahmini (RTT/2) min / ort / maks | 1.40 / 1.53 / 4.48 ms |
| Alıcıdaki göreli gecikme oynaması | ___ |
| Menzil testi (5 m, duvar arkası, kafa üstü) | ___ |
| Osiloskopla uçtan uca gecikme | ___ |

### RTT dağılımı (250 µs aralıklar)
| Aralık (µs) | Paket |
|---|---|
| 2750–3000 | 800 |
| 3000–3250 | 109 |
| 3250–3500 | 14 |
| 3500–3750 | 14 |
| 3750–4000 | 12 |
| 4000–4250 | 9 |
| 4250–4500 | 6 |
| 4500–4750 | 6 |
| 4750–5000 | 8 |
| 5000–5250 | 4 |
| 5250–5500 | 7 |
| 5500–5750 | 3 |
| 5750–6000 | 1 |
| 6000–6250 | 3 |
| 6250–6500 | 2 |
| 7500–7750 | 1 |
| 8750–9000 | 1 |

## Bulgu: verici gücü
İlk denemede verici gücü ayarlanmamıştı (tam güç). Alıcı 1000 paketin yalnızca 48'ini aldı, vericiye hiç yankı dönmedi. `WiFi.setTxPower(WIFI_POWER_8_5dBm)` eklenince 1000/1000 alındı. Bu yüzden ESP32-C3 SuperMini kullanan tüm kodlara bu satır eklendi.

## Bilinen sınırlamalar
- Alıcı kodu sondaki kayıpları saymıyor (son alınan paketten sonrakiler görünmez).
- Ölçüm yalnızca yakın mesafede yapıldı.
- Tek yön değeri bir tahmindir (RTT/2).

## Sonraki adımlar
- Menzil testi: ___
- Osiloskopla gecikme ölçümü: ___
- `araclar/seri_kaydet.py` ile tam kayıt alıp göreli oynamayı hesaplamak: ___
