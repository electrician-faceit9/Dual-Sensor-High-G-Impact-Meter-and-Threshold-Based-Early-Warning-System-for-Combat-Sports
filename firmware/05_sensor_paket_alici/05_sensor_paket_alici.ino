// SENSÖR PAKETİ ALICI (ilk uçtan uca sürüm)
// 07_espnow_test_alici'nin devamı. 29 baytlık ÖZET paketlerini (tip 0x01) alır,
// her paketi YANKI (0xA1) olarak hemen geri gönderir, kayıpları sıra numarasıyla sayar ve
// paketin TÜM alanlarını birimlerine çevrilmiş CSV satırı olarak USB seri porta yazar.
// Bu CSV, karar destek yazılımının (bilgisayar tarafı) okuyacağı biçimdir.
//
// 07'ye göre değişenler:
//   - CSV artık paketin bütün alanlarını içeriyor (tur, cihaz, ivme, açısal hız, süre, Δv, HIC15, seviye, bayrak, pil)
//   - tur: bayraklar bit4 = 1 ise T (test/periyodik paket), 0 ise O (darbe olayı)
//   - Özet satırı test ve olay paketlerini ayrı sayıyor
//   - Yankı gönderme ve kayıp sayımı 07 ile aynı
//
// Seri çıktı (CSV):
// tur,sira_no,cihaz_id,t_tx_us,t_rx_us,fark_us,kayip_toplam,tepe_g,x_g,y_g,z_g,w_dps,darbe_suresi_us,delta_v_mps,hic15,seviye,bayraklar,pil_mv
//   fark_us = t_rx_us - t_tx_us: iki kartın saat farkını İÇERİR, tek başına gecikme değildir.
// '#' ile başlayan satırlar özet/bilgi satırıdır.
// Arduino-ESP32 çekirdeği 3.x (3.3.12) için yazıldı.

#include <WiFi.h>
#include <esp_now.h>

typedef struct __attribute__((packed)) {
  uint8_t  protokol_surumu;
  uint8_t  paket_tipi;
  uint8_t  cihaz_id;
  uint16_t sira_no;
  uint32_t zaman_damgasi_us;
  uint16_t tepe_ivme_bilesik;
  int16_t  tepe_ivme_x;
  int16_t  tepe_ivme_y;
  int16_t  tepe_ivme_z;
  uint16_t tepe_acisal_hiz;
  uint16_t darbe_suresi_us;
  uint16_t delta_v;
  uint16_t hic15;
  uint8_t  seviye;
  uint8_t  bayraklar;
  uint16_t pil_mv;
} OzetPaket;
static_assert(sizeof(OzetPaket) == 29, "OZET paketi 29 bayt olmali");

typedef struct __attribute__((packed)) {
  uint8_t  protokol_surumu;
  uint8_t  paket_tipi;      // 0xA1
  uint16_t sira_no;
  uint32_t t_tx_us;
  uint32_t bekleme_us;
} YankiPaket;
static_assert(sizeof(YankiPaket) == 12, "YANKI paketi 12 bayt olmali");

const bool YANKI = true;    // false: yalnızca alır, geri göndermez
const uint8_t B_TEST = 0x10;

// ---------------- Alım tamponu (geri çağırmadan loop'a) ----------------
struct Kayit { OzetPaket p; uint32_t t_rx; uint8_t mac[6]; };
const int N = 64;
Kayit tampon[N];
volatile int yaz = 0;
int oku = 0;
volatile uint32_t tamponTasmasi = 0;

void alindi(const esp_now_recv_info_t *bilgi, const uint8_t *veri, int uzunluk) {
  uint32_t simdi = micros();                       // önce zamanı al
  if (uzunluk != sizeof(OzetPaket) || veri[1] != 0x01) return;
  int i = yaz;
  int sonraki = (i + 1) % N;
  if (sonraki == oku) { tamponTasmasi++; return; }
  memcpy(&tampon[i].p, veri, sizeof(OzetPaket));
  tampon[i].t_rx = simdi;
  memcpy(tampon[i].mac, bilgi->src_addr, 6);
  yaz = sonraki;
}

uint32_t alinanTest = 0, alinanOlay = 0, kayip = 0;
uint16_t beklenen = 0;
bool ilk = true;

void setup() {
  Serial.begin(115200);
  delay(1500);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // SuperMini için düşük verici gücü
  Serial.printf("# Alici MAC: %s\n", WiFi.macAddress().c_str());
  if (esp_now_init() != ESP_OK) {
    Serial.println("# HATA: ESP-NOW baslatilamadi");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(alindi);
  Serial.println("tur,sira_no,cihaz_id,t_tx_us,t_rx_us,fark_us,kayip_toplam,tepe_g,x_g,y_g,z_g,w_dps,"
                 "darbe_suresi_us,delta_v_mps,hic15,seviye,bayraklar,pil_mv");
}

void loop() {
  static uint32_t sonOzet = 0;

  while (oku != yaz) {
    Kayit k = tampon[oku];
    oku = (oku + 1) % N;

    // 1) Yankıyı hemen gönder (yazdırmadan önce, bekleme kısa kalsın)
    if (YANKI) {
      if (!esp_now_is_peer_exist(k.mac)) {
        esp_now_peer_info_t es = {};
        memcpy(es.peer_addr, k.mac, 6);
        es.channel = 0;
        es.ifidx = WIFI_IF_STA;
        es.encrypt = false;
        esp_now_add_peer(&es);
      }
      YankiPaket y;
      y.protokol_surumu = 1;
      y.paket_tipi = 0xA1;
      y.sira_no = k.p.sira_no;
      y.t_tx_us = k.p.zaman_damgasi_us;
      y.bekleme_us = micros() - k.t_rx;
      esp_now_send(k.mac, (uint8_t *)&y, sizeof(y));
    }

    // 2) Kayıp sayımı (uint16 sıra numarası sıfıra dönse de doğru çalışır)
    if (ilk) {
      ilk = false;
      Serial.printf("# ILK PAKET: surum=%u tip=0x%02X cihaz=%u sira=%u\n",
                    k.p.protokol_surumu, k.p.paket_tipi, k.p.cihaz_id, k.p.sira_no);
    } else {
      uint16_t atlama = (uint16_t)(k.p.sira_no - beklenen);
      if (atlama > 0 && atlama < 30000) kayip += atlama;
    }
    beklenen = k.p.sira_no + 1;
    char tur = (k.p.bayraklar & B_TEST) ? 'T' : 'O';
    if (tur == 'T') alinanTest++; else alinanOlay++;

    // 3) CSV satırı: tüm alanlar, birimlerine çevrilmiş
    const OzetPaket &p = k.p;
    int32_t fark = (int32_t)(k.t_rx - p.zaman_damgasi_us);
    Serial.printf("%c,%u,%u,%lu,%lu,%ld,%lu,%.2f,%.2f,%.2f,%.2f,%.1f,%u,%.3f,%u,%u,0x%02X,%u\n",
                  tur, p.sira_no, p.cihaz_id, (unsigned long)p.zaman_damgasi_us, (unsigned long)k.t_rx,
                  (long)fark, (unsigned long)kayip,
                  p.tepe_ivme_bilesik / 100.0, p.tepe_ivme_x / 100.0, p.tepe_ivme_y / 100.0, p.tepe_ivme_z / 100.0,
                  p.tepe_acisal_hiz / 10.0, p.darbe_suresi_us, p.delta_v / 1000.0, p.hic15,
                  p.seviye, p.bayraklar, p.pil_mv);
  }

  if (millis() - sonOzet >= 5000) {
    sonOzet = millis();
    uint32_t alinan = alinanTest + alinanOlay;
    float oran = (alinan + kayip) ? 100.0 * kayip / (alinan + kayip) : 0;
    Serial.printf("# alinan test/olay: %lu/%lu, kayip: %lu, kayip_orani: %.2f%%, tampon_tasmasi: %lu\n",
                  (unsigned long)alinanTest, (unsigned long)alinanOlay, (unsigned long)kayip, oran,
                  (unsigned long)tamponTasmasi);
  }
}
