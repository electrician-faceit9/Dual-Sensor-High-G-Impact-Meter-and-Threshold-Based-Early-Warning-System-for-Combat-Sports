"""
ESP-NOW testi: kayıp oranı ve gecikme istatistikleri.

Girdi dosyaları seri monitör çıktısıdır (seri_kaydet.py ile kaydedilmiş ya da
Arduino IDE'den kopyalanmış; "HH:MM:SS.mmm -> " önekleri otomatik atılır).

  alici.csv  : sira_no,t_tx_us,t_rx_us,fark_us,kayip_toplam   (07_espnow_test_alici)
  verici.csv : sira_no,t_tx_us,rtt_us,tek_yon_tahmini_us      (06_espnow_test_verici)

Kullanım:
  python gecikme_analiz.py --alici alici.csv --verici verici.csv --gonderilen 1000
  (--verici ve --gonderilen isteğe bağlıdır)

Notlar:
  * Alıcıdaki fark_us = t_rx - t_tx iki kartın saat farkını içerir; mutlak gecikme DEĞİLDİR.
    Betik bundan yalnızca göreli gecikmeyi (fark - en küçük fark) çıkarır: oynama (jitter).
  * Mutlak gecikme tahmini vericideki RTT'den gelir: tek yön ≈ RTT / 2.
  * --gonderilen verilmezse kayıp, alınan ilk ve son sıra numarası arasındaki boşluklardan
    hesaplanır; sondaki kayıpları göremez.
"""
import argparse
import math


def satirlari_oku(dosya, beklenen_alan):
    kayitlar = []
    with open(dosya, encoding="utf-8", errors="replace") as f:
        for ham in f:
            satir = ham.strip()
            if "->" in satir:                      # Arduino IDE zaman öneki
                satir = satir.split("->", 1)[1].strip()
            if not satir or satir.startswith("#") or satir.startswith("sira_no"):
                continue
            parcalar = satir.split(",")
            if len(parcalar) != beklenen_alan:
                continue
            try:
                kayitlar.append([int(p) for p in parcalar])
            except ValueError:
                continue
    return kayitlar


def yuzdelik(sirali, oran):
    """En yakın sıra yöntemi."""
    if not sirali:
        return float("nan")
    k = max(1, math.ceil(oran * len(sirali)))
    return sirali[k - 1]


def ozet(degerler):
    s = sorted(degerler)
    return {
        "n": len(s),
        "min": s[0],
        "ort": sum(s) / len(s),
        "maks": s[-1],
        "p50": yuzdelik(s, 0.50),
        "p95": yuzdelik(s, 0.95),
        "p99": yuzdelik(s, 0.99),
    }


def histogram(degerler, kutu):
    h = {}
    for d in degerler:
        b = int(d // kutu)
        h[b] = h.get(b, 0) + 1
    return [(b * kutu, (b + 1) * kutu, h[b]) for b in sorted(h)]


def sira_ac(siralar):
    """uint16 sıra numarasını sıfıra dönmeye karşı açar."""
    acik, onceki, ek = [], None, 0
    for s in siralar:
        if onceki is not None and s < onceki and onceki - s > 30000:
            ek += 65536
        acik.append(s + ek)
        onceki = s
    return acik


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--alici", required=True)
    ap.add_argument("--verici")
    ap.add_argument("--gonderilen", type=int, help="vericinin gönderdiği toplam paket")
    ap.add_argument("--kutu", type=int, default=250, help="histogram kutu genişliği (µs)")
    a = ap.parse_args()

    # ---------------- Alıcı: kayıp ve göreli gecikme ----------------
    al = satirlari_oku(a.alici, 5)
    if not al:
        raise SystemExit("Alici dosyasinda veri satiri bulunamadi.")
    siralar = sira_ac([r[0] for r in al])
    benzersiz = sorted(set(siralar))
    alinan = len(benzersiz)
    tekrar = len(siralar) - alinan
    if a.gonderilen:
        beklenen = a.gonderilen
        kaynak = "vericinin gonderdigi toplam"
    else:
        beklenen = benzersiz[-1] - benzersiz[0] + 1
        kaynak = "ilk ve son alinan sira arasi (sondaki kayiplar gorulmez)"
    kayip = beklenen - alinan
    oran = 100.0 * kayip / beklenen if beklenen else float("nan")

    farklar = [r[3] for r in al]
    en_kucuk = min(farklar)
    goreli = [f - en_kucuk for f in farklar]
    g = ozet(goreli)

    print("## Alici")
    print(f"Beklenen paket: {beklenen}  ({kaynak})")
    print(f"Alinan paket: {alinan}   Kayip: {kayip}   Kayip orani: %{oran:.2f}   Tekrar: {tekrar}")
    print(f"Goreli gecikme / oynama (us): min {g['min']}, ort {g['ort']:.0f}, maks {g['maks']}, "
          f"p50 {g['p50']}, p95 {g['p95']}, p99 {g['p99']}")

    # ---------------- Verici: RTT ve tek yön tahmini ----------------
    v = None
    if a.verici:
        vr = satirlari_oku(a.verici, 4)
        if vr:
            rtt = [r[2] for r in vr]
            v = ozet(rtt)
            print("\n## Verici (RTT)")
            print(f"Yanki sayisi: {v['n']}")
            print(f"RTT (us): min {v['min']}, ort {v['ort']:.0f}, maks {v['maks']}, "
                  f"p50 {v['p50']}, p95 {v['p95']}, p99 {v['p99']}")
            print(f"Tek yon tahmini (us): min {v['min']/2:.0f}, ort {v['ort']/2:.0f}, maks {v['maks']/2:.0f}")
            print(f"\nDagilim (RTT, {a.kutu} us kutular):")
            for alt, ust, n in histogram(rtt, a.kutu):
                print(f"  {alt}-{ust} us: {n}")

    # ---------------- README tablosu ----------------
    def us_ms(x):
        return f"{x/1000:.2f} ms"

    print("\n## README tablosu\n")
    print("| Ölçüm | Değer |")
    print("|---|---|")
    print(f"| Beklenen / alınan paket | {beklenen} / {alinan} |")
    print(f"| Kayıp oranı | %{oran:.2f} |")
    if v:
        print(f"| Gidiş-dönüş (RTT) min / ort / maks | {us_ms(v['min'])} / {us_ms(v['ort'])} / {us_ms(v['maks'])} |")
        print(f"| RTT p50 / p95 / p99 | {us_ms(v['p50'])} / {us_ms(v['p95'])} / {us_ms(v['p99'])} |")
        print(f"| Tek yön tahmini (RTT/2) min / ort / maks | {us_ms(v['min']/2)} / {us_ms(v['ort']/2)} / {us_ms(v['maks']/2)} |")
    else:
        print("| Gidiş-dönüş (RTT) | ___ |")
        print("| Tek yön tahmini | ___ |")
    print(f"| Göreli gecikme oynaması p95 / maks | {us_ms(g['p95'])} / {us_ms(g['maks'])} |")

    # İsteğe bağlı grafik
    if v:
        try:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
            fig, ax = plt.subplots(figsize=(7, 3.5))
            ax.hist([x / 1000 for x in rtt], bins=40, color="#2b6cb0")
            ax.set_xlabel("Gidiş-dönüş süresi (ms)")
            ax.set_ylabel("Paket sayısı")
            ax.set_title("ESP-NOW RTT dağılımı")
            fig.tight_layout()
            fig.savefig("rtt_dagilim.png", dpi=150)
            print("\nGrafik: rtt_dagilim.png")
        except ImportError:
            pass


if __name__ == "__main__":
    main()
