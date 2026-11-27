// SENSÖR → PAKET VERİCİ (ilk uçtan uca sürüm)
// ADXL375'i (SPI, 3200 Hz) ve MPU6050'yi (I2C, 1 kHz) okur, README 5. bölümdeki
// 29 baytlık ÖZET paketini (tip 0x01) gerçek sensör verisiyle doldurup ESP-NOW ile gönderir.
// Alıcıdan gelen YANKI paketleriyle gidiş-dönüş süresini (RTT) kendi saatiyle ölçer (Hafta 4 yöntemi).
//
// İki tür 0x01 paketi gönderilir; ikisi de aynı sıra numarası sayacını kullanır:
//   TEST : TEST_MODU açıkken her PERIYOT_MS'de bir. Son PERIYOT_MS içindeki gerçek sensör özetini taşır.
//          bayraklar bit4 = 1 (test/periyodik paket, darbe değil). zaman_damgasi_us = gönderim anı.
//   OLAY : Bileşke ivme ESIK_G'yi aşınca, tetikten sonraki ~100 ms'lik pencerenin özeti.
//          bayraklar bit4 = 0. zaman_damgasi_us = tetik örneğinin işlendiği an.
//
// Bu sürümde doldurulmayan alanlar (README'de karar/ölçüm bekliyor):
//   hic15 = 0 (nerede hesaplanacağı açık soru), seviye = 0 (eşikler ___), pil_mv = 0 (pil bağlı değil)
//
// Bağlantı (ESP32-C3 SuperMini):
//   ADXL375: SCL→GPIO4 (SCK), SDO→GPIO5 (MISO), SDA→GPIO6 (MOSI), CS→GPIO7, VIN→3V3, GND→GND
//   MPU6050: SDA→GPIO8, SCL→GPIO9, AD0→GND (0x68), VCC→3V3, GND→GND
// Açılışta kartı Z ekseni yukarı bakacak şekilde ~2 s hareketsiz bırak (ofset ölçülüyor).
//
// Seri çıktı (CSV): tur,sira_no,t_tx_us,rtt_us,tek_yon_tahmini_us,tepe_g
// '#' ile başlayan satırlar özet/bilgi satırıdır.
// Arduino-ESP32 çekirdeği 3.x (3.3.12) için yazıldı.

#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <Wire.h>

// ======================= Test ayarları =======================
const uint8_t  CIHAZ_ID   = 1;
const bool     TEST_MODU  = true;    // false: yalnız darbe olayı paketleri
const uint32_t PERIYOT_MS = 100;     // Hafta 4 ile aynı (10 paket/s)
const uint32_t TOPLAM     = 1000;    // gönderilecek TEST paketi sayısı (0 = sınırsız)
const float    ESIK_G     = 10.0;    // tetik eşiği, doğrulanmalı (README açık soru 1)
const uint32_t PENCERE_ORNEK = 320;  // tetikten sonra ~100 ms (3200 Hz'te)

// ======================= Paket yapıları =======================
typedef struct __attribute__((packed)) {
  uint8_t  protokol_surumu;     // 1
  uint8_t  paket_tipi;          // 0x01
  uint8_t  cihaz_id;
  uint16_t sira_no;
  uint32_t zaman_damgasi_us;
  uint16_t tepe_ivme_bilesik;   // 0.01 g
  int16_t  tepe_ivme_x;         // 0.01 g
  int16_t  tepe_ivme_y;
  int16_t  tepe_ivme_z;
  uint16_t tepe_acisal_hiz;     // 0.1 deg/s
  uint16_t darbe_suresi_us;     // us
  uint16_t delta_v;             // mm/s
  uint16_t hic15;               // birimsiz
  uint8_t  seviye;              // 0 Yeşil, 1 Sarı, 2 Kırmızı
  uint8_t  bayraklar;           // bit0 ADXL doydu, bit1 MPU doydu, bit2 düşük pil, bit3 tampon taştı, bit4 test paketi
  uint16_t pil_mv;              // mV
} OzetPaket;
static_assert(sizeof(OzetPaket) == 29, "OZET paketi 29 bayt olmali");

typedef struct __attribute__((packed)) {
  uint8_t  protokol_surumu;
  uint8_t  paket_tipi;          // 0xA1
  uint16_t sira_no;
  uint32_t t_tx_us;
  uint32_t bekleme_us;
} YankiPaket;
static_assert(sizeof(YankiPaket) == 12, "YANKI paketi 12 bayt olmali");

const uint8_t B_ADXL_DOYDU = 0x01, B_MPU_DOYDU = 0x02, B_TAMPON = 0x08, B_TEST = 0x10;

// Özet penceresi. Fonksiyonlardan ÖNCE tanımlı olmalı: Arduino IDE fonksiyon prototiplerini
// ilk fonksiyonun önüne ekler; yapı daha aşağıda olursa 'Ozet was not declared' hatası verir.
struct Ozet {
  float tepe, px, py, pz;   // g
  float tepeW;              // °/s
  uint32_t esikUstu;        // eşik üstü örnek sayısı
  float dvToplam;           // eşik üstü örneklerde |a| toplamı (g)
  uint8_t bayrak;
};

// ======================= ADXL375 (SPI) =======================
const int PIN_SCK = 4, PIN_MISO = 5, PIN_MOSI = 6, PIN_CS = 7;
SPISettings spiAyar(2000000, MSBFIRST, SPI_MODE3);
const uint8_t A_DEVID = 0x00, A_BW_RATE = 0x2C, A_POWER_CTL = 0x2D, A_INT_SOURCE = 0x30,
              A_DATA_FORMAT = 0x31, A_DATAX0 = 0x32, A_FIFO_CTL = 0x38, A_FIFO_STATUS = 0x39;
const float   A_LSB_PER_G = 20.5;    // tipik (18.4–22.6), kalibre edilmedi
const int16_t A_DOYMA_LSB = 4000;    // GEÇİCİ: gerçek tavan ölçülecek
const float   ORNEK_US    = 312.5;   // nominal 1/3200 s (ölçülen hız 3187–3190 örnek/s)

void aYaz(uint8_t reg, uint8_t v) {
  SPI.beginTransaction(spiAyar); digitalWrite(PIN_CS, LOW);
  SPI.transfer(reg & 0x3F); SPI.transfer(v);
  digitalWrite(PIN_CS, HIGH); SPI.endTransaction();
}
uint8_t aOku(uint8_t reg) {
  SPI.beginTransaction(spiAyar); digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x80 | (reg & 0x3F)); uint8_t v = SPI.transfer(0x00);
  digitalWrite(PIN_CS, HIGH); SPI.endTransaction();
  return v;
}
void aOkuXYZ(int16_t &x, int16_t &y, int16_t &z) {
  uint8_t b[6];
  SPI.beginTransaction(spiAyar); digitalWrite(PIN_CS, LOW);
  SPI.transfer(0xC0 | A_DATAX0);
  for (int i = 0; i < 6; i++) b[i] = SPI.transfer(0x00);
  digitalWrite(PIN_CS, HIGH); SPI.endTransaction();
  delayMicroseconds(5);
  x = (int16_t)((b[1] << 8) | b[0]);
  y = (int16_t)((b[3] << 8) | b[2]);
  z = (int16_t)((b[5] << 8) | b[4]);
}

// ======================= MPU6050 (I2C) =======================
const int PIN_SDA = 8, PIN_SCL = 9;
const uint8_t M_ADR = 0x68;
const uint8_t M_SMPLRT_DIV = 0x19, M_CONFIG = 0x1A, M_GYRO_CONFIG = 0x1B, M_ACCEL_CONFIG = 0x1C,
              M_INT_ENABLE = 0x38, M_INT_STATUS = 0x3A, M_GYRO_XOUT_H = 0x43, M_PWR_MGMT_1 = 0x6B,
              M_WHO_AM_I = 0x75;
const float M_LSB_PER_DPS = 16.4;    // ±2000 °/s

void mYaz(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(M_ADR); Wire.write(reg); Wire.write(v); Wire.endTransmission();
}
uint8_t mOku(uint8_t reg) {
  Wire.beginTransmission(M_ADR); Wire.write(reg); Wire.endTransmission(false);
  Wire.requestFrom(M_ADR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}
bool mOkuGyro(int16_t g[3]) {        // yalnız jiroskop (6 bayt)
  Wire.beginTransmission(M_ADR); Wire.write(M_GYRO_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(M_ADR, (uint8_t)6) != 6) return false;
  for (int i = 0; i < 3; i++) { uint8_t h = Wire.read(), l = Wire.read(); g[i] = (int16_t)((h << 8) | l); }
  return true;
}

// ======================= Özet penceresi =======================
Ozet per, olay;
bool olayVar = false;
uint32_t olayOrnek = 0, olayTetikUs = 0;

float ofX = 0, ofY = 0, ofZ = 0;      // ADXL ofset (LSB)
float gbX = 0, gbY = 0, gbZ = 0;      // jiroskop sıfır noktası (LSB)
uint32_t adxlSayac = 0, mpuSayac = 0, mpuHata = 0, adxlTasma = 0;

void ozetGuncelle(Ozet &o, float a, float gx, float gy, float gz, bool sat) {
  if (a > o.tepe) { o.tepe = a; o.px = gx; o.py = gy; o.pz = gz; }
  if (a >= ESIK_G) { o.esikUstu++; o.dvToplam += a; }
  if (sat) o.bayrak |= B_ADXL_DOYDU;
}

// ======================= ESP-NOW =======================
uint8_t yayin[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint16_t sira = 0;
uint8_t  turTablo[256];               // sira_no % 256 → 'T' test, 'O' olay
uint16_t tepeTablo[256];              // sira_no % 256 → tepe (0.01 g)
uint32_t testGonderilen = 0, olayGonderilen = 0, gonderimHatasi = 0;

struct YankiKayit { uint16_t sira; uint32_t t_tx; uint32_t bekleme; uint32_t t_geri; };
const int N = 64;
YankiKayit ytampon[N];
volatile int yYaz = 0;
int yOku = 0;

void alindi(const esp_now_recv_info_t *bilgi, const uint8_t *veri, int uzunluk) {
  uint32_t simdi = micros();
  if (uzunluk != sizeof(YankiPaket) || veri[1] != 0xA1) return;
  YankiPaket y; memcpy(&y, veri, sizeof(y));
  int i = yYaz;
  ytampon[i].sira = y.sira_no; ytampon[i].t_tx = y.t_tx_us;
  ytampon[i].bekleme = y.bekleme_us; ytampon[i].t_geri = simdi;
  yYaz = (i + 1) % N;
}

uint16_t u16(float v) { if (v <= 0) return 0; if (v >= 65535) return 65535; return (uint16_t)(v + 0.5f); }
int16_t  s16(float v) { if (v >= 32767) return 32767; if (v <= -32768) return -32768; return (int16_t)lroundf(v); }

bool gonder(const Ozet &o, uint32_t zaman, uint8_t ekBayrak, char tur) {
  OzetPaket p;
  p.protokol_surumu   = 1;
  p.paket_tipi        = 0x01;
  p.cihaz_id          = CIHAZ_ID;
  p.sira_no           = sira;
  p.zaman_damgasi_us  = zaman;
  p.tepe_ivme_bilesik = u16(o.tepe * 100);
  p.tepe_ivme_x       = s16(o.px * 100);
  p.tepe_ivme_y       = s16(o.py * 100);
  p.tepe_ivme_z       = s16(o.pz * 100);
  p.tepe_acisal_hiz   = u16(o.tepeW * 10);
  p.darbe_suresi_us   = u16(o.esikUstu * ORNEK_US);
  p.delta_v           = u16(o.dvToplam * 9.80665f * (ORNEK_US * 1e-6f) * 1000.0f);  // mm/s
  p.hic15             = 0;
  p.seviye            = 0;
  p.bayraklar         = o.bayrak | ekBayrak;
  p.pil_mv            = 0;
  turTablo[sira & 0xFF]  = tur;
  tepeTablo[sira & 0xFF] = p.tepe_ivme_bilesik;
  sira++;
  return esp_now_send(yayin, (uint8_t *)&p, sizeof(p)) == ESP_OK;
}

// ======================= Kurulum =======================
void dur(const char *m) { Serial.println(m); while (true) delay(1000); }

void setup() {
  Serial.begin(115200);
  delay(1500);

  // --- ESP-NOW ---
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // SuperMini için düşük verici gücü (Hafta 4 bulgusu)
  Serial.printf("# Verici MAC: %s\n", WiFi.macAddress().c_str());
  if (esp_now_init() != ESP_OK) dur("# HATA: ESP-NOW baslatilamadi");
  esp_now_peer_info_t es = {};
  memcpy(es.peer_addr, yayin, 6); es.channel = 0; es.ifidx = WIFI_IF_STA; es.encrypt = false;
  esp_now_add_peer(&es);
  esp_now_register_recv_cb(alindi);

  // --- ADXL375 ---
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  delay(10);
  uint8_t devid = aOku(A_DEVID);
  Serial.printf("# ADXL375 DEVID = 0x%02X (beklenen 0xE5)\n", devid);
  if (devid != 0xE5) dur("# HATA: ADXL375 DEVID yanlis");
  aYaz(A_POWER_CTL, 0x00);
  aYaz(A_DATA_FORMAT, 0x0B);
  aYaz(A_BW_RATE, 0x0F);     // 3200 Hz
  aYaz(A_FIFO_CTL, 0x90);    // stream, watermark 16
  aYaz(A_POWER_CTL, 0x08);   // ölçüm
  delay(5);

  // --- MPU6050 ---
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  uint8_t who = mOku(M_WHO_AM_I);
  Serial.printf("# MPU6050 WHO_AM_I = 0x%02X (beklenen 0x68)\n", who);
  if (who != 0x68) dur("# HATA: MPU6050 WHO_AM_I yanlis");
  mYaz(M_PWR_MGMT_1, 0x01); delay(100);
  mYaz(M_CONFIG, 0x01); mYaz(M_SMPLRT_DIV, 0x00);
  mYaz(M_GYRO_CONFIG, 0x18); mYaz(M_ACCEL_CONFIG, 0x18);
  mYaz(M_INT_ENABLE, 0x01);
  delay(50);

  // --- Ofsetler: kart hareketsiz, Z yukarı ---
  Serial.println("# Kalibrasyon: karti ~2 s hareket ettirme...");
  long sx = 0, sy = 0, sz = 0; int al = 0;
  while (al < 3200) {
    int n = aOku(A_FIFO_STATUS) & 0x3F;
    for (int k = 0; k < n && al < 3200; k++) { int16_t x, y, z; aOkuXYZ(x, y, z); sx += x; sy += y; sz += z; al++; }
  }
  ofX = (float)sx / al; ofY = (float)sy / al; ofZ = (float)sz / al - A_LSB_PER_G;
  long gx = 0, gy = 0, gz = 0; int gal = 0;
  while (gal < 1000) {
    if (mOku(M_INT_STATUS) & 0x01) { int16_t g[3]; if (mOkuGyro(g)) { gx += g[0]; gy += g[1]; gz += g[2]; gal++; } }
  }
  gbX = (float)gx / gal; gbY = (float)gy / gal; gbZ = (float)gz / gal;
  Serial.printf("# ADXL ofset (LSB): X=%.2f Y=%.2f Z=%.2f | Jiroskop sifir (LSB): X=%.1f Y=%.1f Z=%.1f\n",
                ofX, ofY, ofZ + A_LSB_PER_G, gbX, gbY, gbZ);

  // Kalibrasyon sırasında biriken veriyi at, taşma bayrağını temizle
  while (aOku(A_FIFO_STATUS) & 0x3F) { int16_t a, b, c; aOkuXYZ(a, b, c); }
  aOku(A_INT_SOURCE);

  Serial.printf("# Paket: %u bayt | test modu: %s, periyot: %lu ms, toplam: %lu | esik: %.1f g\n",
                (unsigned)sizeof(OzetPaket), TEST_MODU ? "acik" : "kapali",
                (unsigned long)PERIYOT_MS, (unsigned long)TOPLAM, ESIK_G);
  Serial.println("tur,sira_no,t_tx_us,rtt_us,tek_yon_tahmini_us,tepe_g");
}

// ======================= RTT istatistikleri (yalnız TEST paketleri) =======================
const uint32_t KUTU_US = 250;
const int KUTU_SAYISI = 400;   // 0–100 ms; son kutu 'üst sınırın üstü' demektir (Hafta 5'te 80 kutu = 20 ms yetmedi)
uint32_t hist[KUTU_SAYISI] = {0};
uint32_t rttMin = 0xFFFFFFFF, rttMax = 0, rttSayi = 0, yankiTest = 0, yankiOlay = 0;
uint64_t rttToplam = 0;
bool sonucYazildi = false;

uint32_t yuzdelik(float oran) {
  uint32_t hedef = (uint32_t)ceilf(oran * rttSayi), birikim = 0;
  for (int i = 0; i < KUTU_SAYISI; i++) { birikim += hist[i]; if (birikim >= hedef) return (i + 1) * KUTU_US; }
  return KUTU_SAYISI * KUTU_US;
}

void sonucuYaz() {
  float ort = rttSayi ? (float)rttToplam / rttSayi : 0;
  float kayip = testGonderilen ? 100.0 * (testGonderilen - yankiTest) / testGonderilen : 0;
  Serial.println("# ===== SONUC (TEST paketleri) =====");
  Serial.printf("# gonderilen: %lu, yanki_alinan: %lu, gidis-donus kaybi: %.2f%%\n",
                (unsigned long)testGonderilen, (unsigned long)yankiTest, kayip);
  Serial.printf("# RTT (us): min %lu, ort %.0f, maks %lu, p50 <= %lu, p95 <= %lu, p99 <= %lu\n",
                (unsigned long)rttMin, ort, (unsigned long)rttMax,
                (unsigned long)yuzdelik(0.50), (unsigned long)yuzdelik(0.95), (unsigned long)yuzdelik(0.99));
  Serial.printf("# Tek yon tahmini (us): min %lu, ort %.0f, maks %lu\n",
                (unsigned long)(rttMin / 2), ort / 2, (unsigned long)(rttMax / 2));
  Serial.println("# Dagilim (RTT araligi: paket sayisi):");
  for (int i = 0; i < KUTU_SAYISI; i++)
    if (hist[i]) {
      if (i == KUTU_SAYISI - 1) Serial.printf("#   >= %lu us: %lu\n", (unsigned long)(i * KUTU_US), (unsigned long)hist[i]);
      else Serial.printf("#   %lu-%lu us: %lu\n", (unsigned long)(i * KUTU_US),
                         (unsigned long)((i + 1) * KUTU_US), (unsigned long)hist[i]);
    }
  Serial.printf("# Olay paketi: gonderilen %lu, yanki %lu | gonderim_hatasi: %lu\n",
                (unsigned long)olayGonderilen, (unsigned long)yankiOlay, (unsigned long)gonderimHatasi);
  Serial.println("# ==================================");
}

// ======================= Ana döngü =======================
void loop() {
  static uint32_t sonGonderim = millis(), sonOzet = millis();
  static uint32_t sonAdxl = 0, sonMpu = 0;

  // 1) ADXL375 FIFO'sunu boşalt
  if (aOku(A_INT_SOURCE) & 0x01) {       // overrun
    adxlTasma++; per.bayrak |= B_TAMPON; if (olayVar) olay.bayrak |= B_TAMPON;
  }
  int n = aOku(A_FIFO_STATUS) & 0x3F;
  for (int k = 0; k < n; k++) {
    int16_t x, y, z;
    aOkuXYZ(x, y, z);
    float gx = (x - ofX) / A_LSB_PER_G, gy = (y - ofY) / A_LSB_PER_G, gz = (z - ofZ) / A_LSB_PER_G;
    float a = sqrtf(gx * gx + gy * gy + gz * gz);
    bool sat = abs(x) >= A_DOYMA_LSB || abs(y) >= A_DOYMA_LSB || abs(z) >= A_DOYMA_LSB;
    ozetGuncelle(per, a, gx, gy, gz, sat);
    if (!olayVar && a >= ESIK_G) {
      olayVar = true; memset(&olay, 0, sizeof(olay)); olayOrnek = 0; olayTetikUs = micros();
    }
    if (olayVar) {
      ozetGuncelle(olay, a, gx, gy, gz, sat);
      if (++olayOrnek >= PENCERE_ORNEK) {
        if (gonder(olay, olayTetikUs, 0, 'O')) olayGonderilen++; else gonderimHatasi++;
        olayVar = false;
      }
    }
    adxlSayac++;
  }

  // 2) MPU6050 jiroskobu (veri hazırsa)
  if (mOku(M_INT_STATUS) & 0x01) {
    int16_t g[3];
    if (mOkuGyro(g)) {
      float wx = (g[0] - gbX) / M_LSB_PER_DPS, wy = (g[1] - gbY) / M_LSB_PER_DPS, wz = (g[2] - gbZ) / M_LSB_PER_DPS;
      float w = sqrtf(wx * wx + wy * wy + wz * wz);
      bool msat = false;
      for (int i = 0; i < 3; i++) if (g[i] >= 32767 || g[i] <= -32768) msat = true;
      if (w > per.tepeW) per.tepeW = w;
      if (msat) per.bayrak |= B_MPU_DOYDU;
      if (olayVar) { if (w > olay.tepeW) olay.tepeW = w; if (msat) olay.bayrak |= B_MPU_DOYDU; }
      mpuSayac++;
    } else mpuHata++;
  }

  // 3) TEST paketi: son PERIYOT_MS'nin gerçek sensör özeti
  if (TEST_MODU && (TOPLAM == 0 || testGonderilen < TOPLAM) && millis() - sonGonderim >= PERIYOT_MS) {
    sonGonderim = millis();
    if (gonder(per, micros(), B_TEST, 'T')) testGonderilen++; else gonderimHatasi++;
    memset(&per, 0, sizeof(per));
  }

  // 4) Yankılar → RTT
  while (yOku != yYaz) {
    YankiKayit k = ytampon[yOku];
    yOku = (yOku + 1) % N;
    uint32_t rtt = (k.t_geri - k.t_tx) - k.bekleme;
    char tur = turTablo[k.sira & 0xFF];
    if (tur == 'T') {
      yankiTest++; rttSayi++; rttToplam += rtt;
      if (rtt < rttMin) rttMin = rtt;
      if (rtt > rttMax) rttMax = rtt;
      uint32_t kutu = rtt / KUTU_US;
      hist[kutu < (uint32_t)KUTU_SAYISI ? kutu : KUTU_SAYISI - 1]++;
    } else {
      yankiOlay++;   // OLAY için rtt = tetikten yankıya süre − alıcıda bekleme
    }
    Serial.printf("%c,%u,%lu,%lu,%lu,%.2f\n", tur, k.sira, (unsigned long)k.t_tx,
                  (unsigned long)rtt, (unsigned long)(rtt / 2), tepeTablo[k.sira & 0xFF] / 100.0);
  }

  // 5) 5 s'lik durum satırı
  if (millis() - sonOzet >= 5000) {
    float sure = (millis() - sonOzet) / 1000.0;
    sonOzet = millis();
    Serial.printf("# adxl ornek/s: %.1f, mpu ornek/s: %.1f, adxl_tasma: %lu, mpu_hata: %lu | "
                  "test: %lu, olay: %lu, gonderim_hatasi: %lu, yanki T/O: %lu/%lu\n",
                  (adxlSayac - sonAdxl) / sure, (mpuSayac - sonMpu) / sure,
                  (unsigned long)adxlTasma, (unsigned long)mpuHata,
                  (unsigned long)testGonderilen, (unsigned long)olayGonderilen, (unsigned long)gonderimHatasi,
                  (unsigned long)yankiTest, (unsigned long)yankiOlay);
    sonAdxl = adxlSayac; sonMpu = mpuSayac;
  }

  // 6) Test bitince sonucu bir kez yaz (olay paketleri gönderilmeye devam eder)
  if (TEST_MODU && TOPLAM > 0 && testGonderilen >= TOPLAM && !sonucYazildi && millis() - sonGonderim > 2000) {
    sonucYazildi = true;
    sonucuYaz();
  }
}
