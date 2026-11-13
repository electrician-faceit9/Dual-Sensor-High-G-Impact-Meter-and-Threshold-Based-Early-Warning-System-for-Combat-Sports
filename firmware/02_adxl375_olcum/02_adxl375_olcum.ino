// ADXL375 ölçüm testi: 3200 Hz, SPI mod 3, FIFO stream modu, halka tampon, yazılım tetiği
// Kaynak: Analog Devices ADXL375 veri sayfası, Rev. B (sayfa numaraları açıklamalarda)
// Kart: ESP32-C3 SuperMini, Arduino-ESP32 çekirdeği 3.x
//
// Bağlantı (Adafruit ADXL375 → ESP32-C3):
//   VIN → 3V3, GND → GND, SCL → GPIO4, SDO → GPIO5, SDA → GPIO6, CS → GPIO7
//
// Açılışta kartı Z ekseni yukarı bakacak şekilde masada 1 saniye hareketsiz bırak
// (ofset ölçülüyor).

#include <SPI.h>

// ---------------- Pinler ve SPI ----------------
const int PIN_SCK = 4, PIN_MISO = 5, PIN_MOSI = 6, PIN_CS = 7;
const uint32_t SPI_HZ = 2000000;                 // 3200 Hz için ≥2 MHz önerilir (s. 15)
SPISettings ayar(SPI_HZ, MSBFIRST, SPI_MODE3);   // CPOL = 1, CPHA = 1 (s. 15)

// ---------------- Kayıtlar (s. 20) ----------------
const uint8_t R_DEVID       = 0x00;
const uint8_t R_BW_RATE     = 0x2C;
const uint8_t R_POWER_CTL   = 0x2D;
const uint8_t R_INT_SOURCE  = 0x30;
const uint8_t R_DATA_FORMAT = 0x31;
const uint8_t R_DATAX0      = 0x32;
const uint8_t R_FIFO_CTL    = 0x38;
const uint8_t R_FIFO_STATUS = 0x39;

const uint8_t V_BW_RATE     = 0x0F;  // 3200 Hz, normal güç (s. 10, 22)
const uint8_t V_DATA_FORMAT = 0x0B;  // D3, D1, D0 = 1; 4 telli SPI, sağa yaslı, INT aktif-yüksek (s. 24)
const uint8_t V_FIFO_CTL    = 0x90;  // stream modu (10), watermark = 16 (s. 24)
const uint8_t V_POWER_CTL   = 0x08;  // Measure biti (s. 22)

// ---------------- Ölçüm ayarları ----------------
const float   LSB_PER_G  = 20.5;   // tipik duyarlılık; 18.4–22.6 aralığında olabilir (s. 3), kalibre edilecek
const float   ESIK_G     = 10.0;   // tetik eşiği, doğrulanmalı
const int16_t DOYMA_LSB  = 4000;   // GEÇİCİ: ±200 g × 20.5 ≈ 4100'ün biraz altı; gerçek tavan ölçülecek
const int     KAL_ORNEK  = 3200;   // açılışta ~1 s durağan ortalama
const int     N          = 1024;   // halka tampon (~320 ms)
const int     ON         = 160;    // tetikten önce saklanan örnek (~50 ms)
const int     SONRA      = 320;    // tetikten sonra beklenen örnek (~100 ms)
const bool    HAM_DOKUM  = false;  // true: olay penceresini CSV olarak yaz (yazarken FIFO taşabilir)

// ---------------- Durum ----------------
int16_t bx[N], by[N], bz[N];
uint32_t toplam = 0;               // toplam örnek sayacı; tampon indeksi = toplam % N
float ofX = 0, ofY = 0, ofZ = 0;   // ofset (ham LSB)

bool olayVar = false;
uint32_t tetikIdx = 0, tepeIdx = 0, esikUstu = 0;
float tepe = 0;
bool doydu = false;

// ---------------- SPI yardımcıları ----------------
void yaz(uint8_t reg, uint8_t deger) {
  SPI.beginTransaction(ayar);
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(reg & 0x3F);          // R/W = 0 (yazma), MB = 0
  SPI.transfer(deger);
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();
}

uint8_t oku(uint8_t reg) {
  SPI.beginTransaction(ayar);
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x80 | (reg & 0x3F)); // R/W = 1 (okuma)
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();
  return v;
}

// 6 bayt tek blok okuma (s. 12: FIFO verisi blok halinde okunmalı)
void okuXYZ(int16_t &x, int16_t &y, int16_t &z) {
  uint8_t b[6];
  SPI.beginTransaction(ayar);
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0xC0 | R_DATAX0);     // R/W = 1, MB = 1
  for (int i = 0; i < 6; i++) b[i] = SPI.transfer(0x00);
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();
  delayMicroseconds(5);              // FIFO okumaları arası ≥5 µs (s. 12)
  x = (int16_t)((b[1] << 8) | b[0]);
  y = (int16_t)((b[3] << 8) | b[2]);
  z = (int16_t)((b[5] << 8) | b[4]);
}

// ---------------- Olay işleme ----------------
void olayRaporla() {
  float ms = 1000.0 / 3200.0;        // nominal örnek aralığı
  Serial.printf("# OLAY: tepe=%.1f g, tepe_ani=+%.2f ms, esik_ustu=%.2f ms, doyma=%s\n",
                tepe, (tepeIdx - tetikIdx) * ms, esikUstu * ms, doydu ? "EVET" : "hayir");
  if (HAM_DOKUM) {
    Serial.println("ornek,x_lsb,y_lsb,z_lsb");
    for (uint32_t k = tetikIdx - ON; k < tetikIdx + SONRA; k++) {
      uint32_t i = k % N;
      Serial.printf("%ld,%d,%d,%d\n", (long)(k - tetikIdx), bx[i], by[i], bz[i]);
    }
  }
}

void ornekIsle(int16_t x, int16_t y, int16_t z) {
  uint32_t i = toplam % N;
  bx[i] = x; by[i] = y; bz[i] = z;

  float gx = (x - ofX) / LSB_PER_G;
  float gy = (y - ofY) / LSB_PER_G;
  float gz = (z - ofZ) / LSB_PER_G;
  float a = sqrtf(gx * gx + gy * gy + gz * gz);
  bool sat = abs(x) >= DOYMA_LSB || abs(y) >= DOYMA_LSB || abs(z) >= DOYMA_LSB;

  if (!olayVar) {
    if (a >= ESIK_G && toplam >= (uint32_t)ON) {
      olayVar = true;
      tetikIdx = toplam; tepeIdx = toplam;
      tepe = a; esikUstu = 1; doydu = sat;
    }
  } else {
    if (a > tepe) { tepe = a; tepeIdx = toplam; }
    if (a >= ESIK_G) esikUstu++;
    if (sat) doydu = true;
    if (toplam - tetikIdx >= (uint32_t)SONRA) {
      olayRaporla();
      olayVar = false;
    }
  }
  toplam++;
}

// ---------------- Kurulum ----------------
void setup() {
  Serial.begin(115200);
  delay(1500);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);        // CS yüksek: SPI ayarlanana kadar seçili değil
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  delay(10);

  uint8_t id = oku(R_DEVID);
  Serial.printf("DEVID = 0x%02X (beklenen 0xE5)\n", id);
  if (id != 0xE5) {
    Serial.println("HATA: DEVID yanlis. Baglantilari kontrol et.");
    while (true) delay(1000);
  }

  yaz(R_POWER_CTL, 0x00);            // önce bekleme modu (s. 10)
  yaz(R_DATA_FORMAT, V_DATA_FORMAT);
  yaz(R_BW_RATE, V_BW_RATE);
  yaz(R_FIFO_CTL, V_FIFO_CTL);
  yaz(R_POWER_CTL, V_POWER_CTL);     // ölçüme geç
  delay(5);

  Serial.printf("BW_RATE=0x%02X DATA_FORMAT=0x%02X FIFO_CTL=0x%02X POWER_CTL=0x%02X\n",
                oku(R_BW_RATE), oku(R_DATA_FORMAT), oku(R_FIFO_CTL), oku(R_POWER_CTL));
  Serial.printf("SPI = %lu Hz\n", (unsigned long)SPI_HZ);

  // Ofset: Z yukarı, kart hareketsiz
  Serial.println("Kalibrasyon: karti 1 s hareket ettirme...");
  long sx = 0, sy = 0, sz = 0;
  int alinan = 0;
  while (alinan < KAL_ORNEK) {
    int n = oku(R_FIFO_STATUS) & 0x3F;
    for (int k = 0; k < n && alinan < KAL_ORNEK; k++) {
      int16_t x, y, z;
      okuXYZ(x, y, z);
      sx += x; sy += y; sz += z;
      alinan++;
    }
  }
  float mx = (float)sx / KAL_ORNEK, my = (float)sy / KAL_ORNEK, mz = (float)sz / KAL_ORNEK;
  ofX = mx; ofY = my; ofZ = mz - LSB_PER_G;   // Z ekseninde +1 g varsayımı (s. 28)
  Serial.printf("Durgun ortalama (LSB): X=%.2f Y=%.2f Z=%.2f\n", mx, my, mz);
  Serial.println("Hazir. Olcum basladi.");
}

// ---------------- Ana döngü ----------------
void loop() {
  static uint32_t sonRapor = 0, sonToplam = 0, tasma = 0, maxDolu = 0;

  if (oku(R_INT_SOURCE) & 0x01) tasma++;   // overrun: stream modunda FIFO doldu (s. 14)
  int n = oku(R_FIFO_STATUS) & 0x3F;
  if ((uint32_t)n > maxDolu) maxDolu = n;

  for (int k = 0; k < n; k++) {
    int16_t x, y, z;
    okuXYZ(x, y, z);
    ornekIsle(x, y, z);
  }

  if (millis() - sonRapor >= 5000) {
    sonRapor = millis();
    Serial.printf("# ornek/s: %.1f, fifo_max_doluluk: %lu, tasma: %lu\n",
                  (toplam - sonToplam) / 5.0, (unsigned long)maxDolu, (unsigned long)tasma);
    sonToplam = toplam;
    maxDolu = 0;
    tasma = 0;
  }
}
