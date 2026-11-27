// MPU6050 (GY-521) kimlik ve ilk veri testi: I2C taraması, WHO_AM_I, 1 kHz örnekleme, ilk ham değerler
// Kaynak: InvenSense MPU-6000/MPU-6050 Register Map and Descriptions, Rev. 4.2
// Kart: ESP32-C3 SuperMini, Arduino-ESP32 çekirdeği 3.x
//
// Bağlantı (GY-521 → ESP32-C3):
//   VCC → 3V3, GND → GND, SDA → GPIO8, SCL → GPIO9, INT → GPIO10, AD0 → GND (adres 0x68)
//
// Açılışta kartı masada hareketsiz bırak.

#include <Wire.h>

const int PIN_SDA = 8, PIN_SCL = 9;
const uint32_t I2C_HZ = 400000;      // hızlı mod
const uint8_t ADR = 0x68;            // AD0 = GND

// ---------------- Kayıtlar ----------------
const uint8_t R_SMPLRT_DIV   = 0x19;
const uint8_t R_CONFIG       = 0x1A;
const uint8_t R_GYRO_CONFIG  = 0x1B;
const uint8_t R_ACCEL_CONFIG = 0x1C;
const uint8_t R_INT_ENABLE   = 0x38;
const uint8_t R_INT_STATUS   = 0x3A;
const uint8_t R_ACCEL_XOUT_H = 0x3B;  // 14 bayt: ivme X,Y,Z, sıcaklık, jiroskop X,Y,Z (büyük uç önce)
const uint8_t R_PWR_MGMT_1   = 0x6B;
const uint8_t R_WHO_AM_I     = 0x75;

// Örnekleme hızı = Jiroskop çıkış hızı / (1 + SMPLRT_DIV); DLPF açıkken jiroskop çıkış hızı 1 kHz
const uint8_t V_CONFIG       = 0x01;  // DLPF_CFG = 1
const uint8_t V_SMPLRT_DIV   = 0x00;  // 1 kHz / (1 + 0) = 1 kHz
const uint8_t V_GYRO_CONFIG  = 0x18;  // FS_SEL = 3 → ±2000 °/s
const uint8_t V_ACCEL_CONFIG = 0x18;  // AFS_SEL = 3 → ±16 g
const float LSB_PER_G   = 2048.0;     // AFS_SEL = 3
const float LSB_PER_DPS = 16.4;       // FS_SEL = 3

void yaz(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(ADR);
  Wire.write(reg); Wire.write(v);
  Wire.endTransmission();
}

uint8_t oku(uint8_t reg) {
  Wire.beginTransmission(ADR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ADR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

bool okuHam(int16_t v[7]) {          // ax, ay, az, sicaklik, gx, gy, gz
  Wire.beginTransmission(ADR);
  Wire.write(R_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADR, (uint8_t)14) != 14) return false;
  for (int i = 0; i < 7; i++) {
    uint8_t h = Wire.read(), l = Wire.read();
    v[i] = (int16_t)((h << 8) | l);
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Wire.begin(PIN_SDA, PIN_SCL, I2C_HZ);

  // 1) I2C taraması
  Serial.println("I2C taramasi:");
  int bulunan = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf("  cihaz bulundu: 0x%02X\n", a); bulunan++; }
  }
  if (bulunan == 0) Serial.println("  hic cihaz yok");

  // 2) Kimlik
  uint8_t id = oku(R_WHO_AM_I);
  Serial.printf("WHO_AM_I = 0x%02X (beklenen 0x68)\n", id);
  if (id != 0x68) {
    Serial.println("HATA: WHO_AM_I yanlis. Baglantilari kontrol et.");
    while (true) delay(1000);
  }

  // 3) Ayarlar
  yaz(R_PWR_MGMT_1, 0x01);           // uykudan çık, saat kaynağı: X jiroskobu PLL
  delay(100);
  yaz(R_CONFIG, V_CONFIG);
  yaz(R_SMPLRT_DIV, V_SMPLRT_DIV);
  yaz(R_GYRO_CONFIG, V_GYRO_CONFIG);
  yaz(R_ACCEL_CONFIG, V_ACCEL_CONFIG);
  yaz(R_INT_ENABLE, 0x01);           // veri hazır kesmesi (INT_STATUS bit0 ile de izlenir)
  delay(50);
  Serial.printf("PWR_MGMT_1=0x%02X CONFIG=0x%02X SMPLRT_DIV=0x%02X GYRO_CONFIG=0x%02X ACCEL_CONFIG=0x%02X\n",
                oku(R_PWR_MGMT_1), oku(R_CONFIG), oku(R_SMPLRT_DIV), oku(R_GYRO_CONFIG), oku(R_ACCEL_CONFIG));
  Serial.printf("I2C = %lu Hz\n", (unsigned long)I2C_HZ);

  // 4) İlk ham değerler: önce 20 örnek okunur, sonra yazdırılır
  const int ILK = 20;
  int16_t d[ILK][7];
  int al = 0;
  while (al < ILK) {
    if (oku(R_INT_STATUS) & 0x01) { if (okuHam(d[al])) al++; }
  }
  Serial.println("# Ilk ham degerler (ardisik ornekler)");
  Serial.println("ornek,ax_lsb,ay_lsb,az_lsb,gx_lsb,gy_lsb,gz_lsb,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,sicaklik_C");
  for (int k = 0; k < ILK; k++) {
    int16_t *v = d[k];
    Serial.printf("%d,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.2f\n", k,
                  v[0], v[1], v[2], v[4], v[5], v[6],
                  v[0] / LSB_PER_G, v[1] / LSB_PER_G, v[2] / LSB_PER_G,
                  v[4] / LSB_PER_DPS, v[5] / LSB_PER_DPS, v[6] / LSB_PER_DPS,
                  v[3] / 340.0 + 36.53);      // sıcaklık formülü: register map, TEMP_OUT bölümü
  }
  Serial.println("Hazir. Olcum basladi.");
}

void loop() {
  static uint32_t sonRapor = millis(), sayac = 0, hata = 0;
  int16_t v[7];
  if (oku(R_INT_STATUS) & 0x01) {
    if (okuHam(v)) sayac++; else hata++;
  }
  if (millis() - sonRapor >= 5000) {
    sonRapor = millis();
    Serial.printf("# ornek/s: %.1f, okuma_hatasi: %lu\n", sayac / 5.0, (unsigned long)hata);
    sayac = 0; hata = 0;
  }
}
