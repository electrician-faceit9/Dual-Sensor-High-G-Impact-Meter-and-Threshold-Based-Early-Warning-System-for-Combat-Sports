// ESP-NOW TEST VERİCİ (sensörsüz)
// README 5. bölümdeki 29 baytlık ÖZET paketini (tip 0x01) sabit sahte değerlerle gönderir.
// Alıcıdan gelen YANKI paketleriyle gidiş-dönüş süresini (RTT) kendi saatiyle ölçer.
// Test bitince RTT istatistiklerini (min/ort/maks/yüzdelik/dağılım) kendisi hesaplayıp yazar.
//
// Seri çıktı (CSV): sira_no,t_tx_us,rtt_us,tek_yon_tahmini_us
// '#' ile başlayan satırlar özet/bilgi satırıdır.

#include <WiFi.h>
#include <esp_now.h>

typedef struct __attribute__((packed)) {
  uint8_t  protokol_surumu;     // 1
  uint8_t  paket_tipi;          // 0x01
  uint8_t  cihaz_id;
  uint16_t sira_no;
  uint32_t zaman_damgasi_us;    // gönderim anı (vericinin saati)
  uint16_t tepe_ivme_bilesik;   // 0.01 g
  int16_t  tepe_ivme_x;         // 0.01 g
  int16_t  tepe_ivme_y;
  int16_t  tepe_ivme_z;
  uint16_t tepe_acisal_hiz;     // 0.1 deg/s
  uint16_t darbe_suresi_us;     // us
  uint16_t delta_v;             // mm/s
  uint16_t hic15;               // birimsiz
  uint8_t  seviye;              // 0 Yeşil, 1 Sarı, 2 Kırmızı
  uint8_t  bayraklar;
  uint16_t pil_mv;              // mV
} OzetPaket;
static_assert(sizeof(OzetPaket) == 29, "OZET paketi 29 bayt olmali");

typedef struct __attribute__((packed)) {
  uint8_t  protokol_surumu;     // 1
  uint8_t  paket_tipi;          // 0xA1
  uint16_t sira_no;
  uint32_t t_tx_us;
  uint32_t bekleme_us;
} YankiPaket;
static_assert(sizeof(YankiPaket) == 12, "YANKI paketi 12 bayt olmali");

const uint32_t PERIYOT_MS = 100;
const uint32_t TOPLAM     = 1000;
uint8_t yayin[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct YankiKayit { uint16_t sira; uint32_t t_tx; uint32_t bekleme; uint32_t t_geri; };
const int N = 64;
YankiKayit ytampon[N];
volatile int yYaz = 0;
int yOku = 0;

void alindi(const esp_now_recv_info_t *bilgi, const uint8_t *veri, int uzunluk) {
  uint32_t simdi = micros();
  if (uzunluk != sizeof(YankiPaket) || veri[1] != 0xA1) return;
  YankiPaket y;
  memcpy(&y, veri, sizeof(y));
  int i = yYaz;
  ytampon[i].sira = y.sira_no;
  ytampon[i].t_tx = y.t_tx_us;
  ytampon[i].bekleme = y.bekleme_us;
  ytampon[i].t_geri = simdi;
  yYaz = (i + 1) % N;
}

OzetPaket p;
uint16_t sira = 0;
uint32_t gonderilen = 0, gonderimHatasi = 0, yankiAlinan = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // SuperMini icin dusuk verici gucu
  Serial.printf("# Verici MAC: %s\n", WiFi.macAddress().c_str());

  if (esp_now_init() != ESP_OK) {
    Serial.println("# HATA: ESP-NOW baslatilamadi");
    while (true) delay(1000);
  }
  esp_now_peer_info_t es = {};
  memcpy(es.peer_addr, yayin, 6);
  es.channel = 0;
  es.ifidx = WIFI_IF_STA;
  es.encrypt = false;
  esp_now_add_peer(&es);
  esp_now_register_recv_cb(alindi);

  p.protokol_surumu   = 1;
  p.paket_tipi        = 0x01;
  p.cihaz_id          = 1;
  p.tepe_ivme_x       = 1000;
  p.tepe_ivme_y       = -500;
  p.tepe_ivme_z       = 600;
  p.tepe_ivme_bilesik = 1269;
  p.tepe_acisal_hiz   = 1500;
  p.darbe_suresi_us   = 4000;
  p.delta_v           = 2500;
  p.hic15             = 45;
  p.seviye            = 1;
  p.bayraklar         = 0;
  p.pil_mv            = 3900;

  Serial.printf("# Paket boyu: %u bayt, periyot: %lu ms, toplam: %lu\n",
                (unsigned)sizeof(OzetPaket), (unsigned long)PERIYOT_MS, (unsigned long)TOPLAM);
  Serial.println("sira_no,t_tx_us,rtt_us,tek_yon_tahmini_us");
}

const uint32_t KUTU_US = 250;
const int KUTU_SAYISI = 400;   // 0–100 ms; son kutu 'üst sınırın üstü' (önceden 80 kutu = 20 ms)
uint32_t hist[KUTU_SAYISI] = {0};
uint32_t rttMin = 0xFFFFFFFF, rttMax = 0, rttSayi = 0;
uint64_t rttToplam = 0;
bool sonucYazildi = false;

uint32_t yuzdelik(float oran) {
  uint32_t hedef = (uint32_t)ceilf(oran * rttSayi);
  uint32_t birikim = 0;
  for (int i = 0; i < KUTU_SAYISI; i++) {
    birikim += hist[i];
    if (birikim >= hedef) return (i + 1) * KUTU_US;
  }
  return KUTU_SAYISI * KUTU_US;
}

void sonucuYaz() {
  float ort = rttSayi ? (float)rttToplam / rttSayi : 0;
  float kayip = gonderilen ? 100.0 * (gonderilen - yankiAlinan) / gonderilen : 0;
  Serial.println("# ===== SONUC =====");
  Serial.printf("# gonderilen: %lu, yanki_alinan: %lu, gidis-donus kaybi: %.2f%%\n",
                (unsigned long)gonderilen, (unsigned long)yankiAlinan, kayip);
  Serial.printf("# RTT (us): min %lu, ort %.0f, maks %lu, p50 <= %lu, p95 <= %lu, p99 <= %lu\n",
                (unsigned long)rttMin, ort, (unsigned long)rttMax,
                (unsigned long)yuzdelik(0.50), (unsigned long)yuzdelik(0.95), (unsigned long)yuzdelik(0.99));
  Serial.printf("# Tek yon tahmini (us): min %lu, ort %.0f, maks %lu\n",
                (unsigned long)(rttMin / 2), ort / 2, (unsigned long)(rttMax / 2));
  Serial.println("# Dagilim (RTT araligi: paket sayisi):");
  for (int i = 0; i < KUTU_SAYISI; i++) {
    if (!hist[i]) continue;
    if (i == KUTU_SAYISI - 1) Serial.printf("#   >= %lu us: %lu\n", (unsigned long)(i * KUTU_US), (unsigned long)hist[i]);
    else Serial.printf("#   %lu-%lu us: %lu\n", (unsigned long)(i * KUTU_US),
                       (unsigned long)((i + 1) * KUTU_US), (unsigned long)hist[i]);
  }
  Serial.println("# ==================");
}

void loop() {
  static uint32_t sonGonderim = 0, sonOzet = 0;

  if ((TOPLAM == 0 || gonderilen < TOPLAM) && millis() - sonGonderim >= PERIYOT_MS) {
    sonGonderim = millis();
    p.sira_no = sira++;
    p.zaman_damgasi_us = micros();
    if (esp_now_send(yayin, (uint8_t *)&p, sizeof(p)) == ESP_OK) gonderilen++;
    else gonderimHatasi++;
  }

  while (yOku != yYaz) {
    YankiKayit k = ytampon[yOku];
    yOku = (yOku + 1) % N;
    yankiAlinan++;
    uint32_t rtt = (k.t_geri - k.t_tx) - k.bekleme;
    rttSayi++;
    rttToplam += rtt;
    if (rtt < rttMin) rttMin = rtt;
    if (rtt > rttMax) rttMax = rtt;
    uint32_t kutu = rtt / KUTU_US;
    hist[kutu < (uint32_t)KUTU_SAYISI ? kutu : KUTU_SAYISI - 1]++;
    Serial.printf("%u,%lu,%lu,%lu\n", k.sira, (unsigned long)k.t_tx,
                  (unsigned long)rtt, (unsigned long)(rtt / 2));
  }

  if (millis() - sonOzet >= 5000) {
    sonOzet = millis();
    Serial.printf("# gonderilen: %lu, gonderim_hatasi: %lu, yanki_alinan: %lu\n",
                  (unsigned long)gonderilen, (unsigned long)gonderimHatasi, (unsigned long)yankiAlinan);
  }

  if (TOPLAM > 0 && gonderilen >= TOPLAM && !sonucYazildi && millis() - sonGonderim > 2000) {
    sonucYazildi = true;
    sonucuYaz();
  }
}
