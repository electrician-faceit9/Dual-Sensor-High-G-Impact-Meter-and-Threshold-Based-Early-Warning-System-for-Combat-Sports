// ESP-NOW TEST ALICI (sensörsüz)
// 29 baytlık ÖZET paketlerini (tip 0x01) alır, her paket için CSV satırı yazar,
// kayıp paketleri sıra numarasıyla sayar ve her paketi YANKI (0xA1) olarak geri gönderir.
// Arduino-ESP32 çekirdeği 3.x için yazıldı (3.3.12 ile denenecek).
//
// Seri çıktı (CSV): sira_no,t_tx_us,t_rx_us,fark_us,kayip_toplam
//   t_tx_us : vericinin saatiyle gönderim anı (paketten)
//   t_rx_us : alıcının saatiyle alım anı
//   fark_us : t_rx_us - t_tx_us  (iki kartın saat farkını İÇERİR, tek başına gecikme değildir)
// '#' ile başlayan satırlar özet/bilgi satırıdır.

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

uint32_t alinan = 0, kayip = 0;
uint16_t beklenen = 0;
bool ilk = true;

void ilkPaketiYaz(const OzetPaket &p) {
  // Sözleşme kontrolü: vericideki sabit değerlerle aynı çıkmalı
  Serial.printf("# ILK PAKET: surum=%u tip=0x%02X cihaz=%u tepe=%.2f g x=%.2f y=%.2f z=%.2f "
                "w=%.1f dps sure=%u us dv=%.3f m/s hic=%u seviye=%u bayrak=0x%02X pil=%u mV\n",
                p.protokol_surumu, p.paket_tipi, p.cihaz_id, p.tepe_ivme_bilesik / 100.0,
                p.tepe_ivme_x / 100.0, p.tepe_ivme_y / 100.0, p.tepe_ivme_z / 100.0,
                p.tepe_acisal_hiz / 10.0, p.darbe_suresi_us, p.delta_v / 1000.0, p.hic15,
                p.seviye, p.bayraklar, p.pil_mv);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // SuperMini icin dusuk verici gucu
  Serial.printf("# Alici MAC: %s\n", WiFi.macAddress().c_str());
  if (esp_now_init() != ESP_OK) {
    Serial.println("# HATA: ESP-NOW baslatilamadi");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(alindi);
  Serial.println("sira_no,t_tx_us,t_rx_us,fark_us,kayip_toplam");
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
      ilkPaketiYaz(k.p);
    } else {
      uint16_t atlama = (uint16_t)(k.p.sira_no - beklenen);
      if (atlama > 0 && atlama < 30000) kayip += atlama;
    }
    beklenen = k.p.sira_no + 1;
    alinan++;

    // 3) CSV satırı
    int32_t fark = (int32_t)(k.t_rx - k.p.zaman_damgasi_us);
    Serial.printf("%u,%lu,%lu,%ld,%lu\n", k.p.sira_no, (unsigned long)k.p.zaman_damgasi_us,
                  (unsigned long)k.t_rx, (long)fark, (unsigned long)kayip);
  }

  if (millis() - sonOzet >= 5000) {
    sonOzet = millis();
    float oran = (alinan + kayip) ? 100.0 * kayip / (alinan + kayip) : 0;
    Serial.printf("# alinan: %lu, kayip: %lu, kayip_orani: %.2f%%, tampon_tasmasi: %lu\n",
                  (unsigned long)alinan, (unsigned long)kayip, oran, (unsigned long)tamponTasmasi);
  }
}
