"""
Seri porttaki satırları hem ekrana yazar hem de bir dosyaya kaydeder.

Kurulum (bir kez):   pip install pyserial
Kullanım:            python seri_kaydet.py COM8 alici.csv
                     python seri_kaydet.py COM7 verici.csv
Durdurmak için:      Ctrl+C

Not: Aynı portu Arduino IDE'nin seri monitörü açıkken kullanamazsın; önce onu kapat.
Her satırın başına bilgisayarın saati (ms) eklenmez; satırlar kartın yazdığı gibi kaydedilir.
"""
import sys
import serial

if len(sys.argv) != 3:
    print("Kullanim: python seri_kaydet.py <PORT> <DOSYA>   ornek: python seri_kaydet.py COM8 alici.csv")
    sys.exit(1)

port, dosya = sys.argv[1], sys.argv[2]
satir_sayisi = 0

with serial.Serial(port, 115200, timeout=1) as s, open(dosya, "w", encoding="utf-8", newline="") as f:
    print(f"{port} dinleniyor, {dosya} dosyasina yaziliyor. Durdurmak icin Ctrl+C.")
    try:
        while True:
            ham = s.readline()
            if not ham:
                continue
            satir = ham.decode("utf-8", errors="replace").rstrip("\r\n")
            print(satir)
            f.write(satir + "\n")
            f.flush()
            satir_sayisi += 1
    except KeyboardInterrupt:
        pass

print(f"\nBitti: {satir_sayisi} satir {dosya} dosyasina kaydedildi.")
