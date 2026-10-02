/*
 * CAVLI GSM/LTE Devboard Dahili GPS — 05 GPS ile Konum Okuma
 *
 * YALNIZ "Dahili GPS" sürümü içindir (GPS'siz kartta AT+CGPS komutu ERROR döner).
 *
 * GNSS'i açar (AT+CGPS=1) ve her 5 saniyede bir RMC cümlesini (AT+CGPSGPOS=5)
 * okuyup enlem, boylam, hız ve UTC saatini Seri Monitöre yazar; konum
 * bulunduğunda Google Haritalar bağlantısı da verir.
 *
 * İlk konum açık gökyüzü altında, aktif GNSS anteniyle birkaç dakika sürebilir
 * (soğuk başlangıç). Kapalı alanda konum bulunmayabilir.
 * SIM ve şebeke gerekmez, ama modemin beslemesi (3V3 ≥ 2 A) gerekir.
 *
 * Bağlantı: 01_AT_Komut_Terminali ile aynı (TX→16, RX←17, PKY←4, GND, 3V3).
 * GNSS anteni kartın GNSS yazan U.FL konnektörüne takılmalıdır.
 *
 * EN: GPS version only. Starts GNSS and prints latitude, longitude, speed and UTC
 *     time from the RMC sentence every 5 s, with a Google Maps link once fixed.
 */

#include <Arduino.h>

// ─────────── KULLANICI AYARLARI ───────────
#define MODEM_RX_PIN   16
#define MODEM_TX_PIN   17
#define MODEM_PKY_PIN  4
#define PKY_BASILI     HIGH
#define PKY_SERBEST    LOW
const uint32_t OKUMA_ARALIGI_MS = 5000;
// ──────────────────────────────────────────

HardwareSerial modem(2);

String atGonder(const String& komut, uint32_t sure = 3000, const char* bitis = "OK") {
  while (modem.available()) modem.read();
  modem.print(komut); modem.print("\r\n");
  String c;
  uint32_t t = millis();
  while (millis() - t < sure) {
    while (modem.available()) c += (char)modem.read();
    if (c.indexOf(bitis) >= 0 || c.indexOf("ERROR") >= 0) break;
    delay(5);
  }
  c.trim();
  return c;
}

void pkyDarbesi() {
  pinMode(MODEM_PKY_PIN, OUTPUT);
  digitalWrite(MODEM_PKY_PIN, PKY_SERBEST); delay(100);
  digitalWrite(MODEM_PKY_PIN, PKY_BASILI);  delay(500);
  digitalWrite(MODEM_PKY_PIN, PKY_SERBEST);
}

// NMEA "ddmm.mmmm" + yön → ondalık derece
double nmeaDerece(const String& deger, const String& yon) {
  if (deger.length() < 4) return 0;
  double v = deger.toDouble();
  int derece = (int)(v / 100);
  double sonuc = derece + (v - derece * 100) / 60.0;
  return (yon == "S" || yon == "W") ? -sonuc : sonuc;
}

// RMC alanları: saat, durum(A/V), enlem, K/G, boylam, D/B, hız(knot), rota, tarih, ...
void rmcIsle(const String& cevap) {
  int p = cevap.indexOf("RMC");
  if (p < 0) { Serial.println("RMC cevabi yok: " + cevap); return; }
  int bas = p + 3;
  while (bas < (int)cevap.length() && (cevap[bas] == ':' || cevap[bas] == ',' || cevap[bas] == ' ')) bas++;
  String govde = cevap.substring(bas, cevap.indexOf('\n', bas) > 0 ? cevap.indexOf('\n', bas) : cevap.length());

  String alan[12];
  int n = 0, onceki = 0;
  for (int i = 0; i <= (int)govde.length() && n < 12; i++) {
    if (i == (int)govde.length() || govde[i] == ',' || govde[i] == '*') {
      alan[n++] = govde.substring(onceki, i);
      onceki = i + 1;
    }
  }

  String saat = alan[0].length() >= 6 ? alan[0].substring(0, 2) + ":" + alan[0].substring(2, 4) + ":" + alan[0].substring(4, 6) : "--:--:--";
  if (alan[1] != "A") {
    Serial.println("UTC " + saat + "  |  konum araniyor... (anten acik gokyuzune bakmali)");
    return;
  }
  double enlem = nmeaDerece(alan[2], alan[3]);
  double boylam = nmeaDerece(alan[4], alan[5]);
  double hiz = alan[6].toDouble() * 1.852;    // knot → km/sa
  Serial.printf("UTC %s  |  enlem %.6f  boylam %.6f  |  %.1f km/sa\n", saat.c_str(), enlem, boylam, hiz);
  Serial.printf("  https://maps.google.com/?q=%.6f,%.6f\n", enlem, boylam);
}

void setup() {
  Serial.begin(115200);
  modem.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(300);
  Serial.println("\n=== CAVLI GPS konum okuma ===");

  if (atGonder("AT", 1000).indexOf("OK") < 0) {
    Serial.println("Modem kapali, PKY ile aciliyor...");
    pkyDarbesi();
    uint32_t t = millis();
    while (atGonder("AT", 1000).indexOf("OK") < 0) {
      if (millis() - t > 20000) { Serial.println("[HATA] Modem cevap vermiyor."); return; }
    }
  }
  atGonder("ATE0");
  String r = atGonder("AT+CGPS=1", 5000);              // GNSS oturumunu başlat
  Serial.println("AT+CGPS=1 -> " + r);
  if (r.indexOf("ERROR") >= 0) Serial.println("[UYARI] GNSS baslatilamadi. Kart Dahili GPS surumu mu?");
}

void loop() {
  static uint32_t son = 0;
  if (millis() - son < OKUMA_ARALIGI_MS) return;
  son = millis();
  rmcIsle(atGonder("AT+CGPSGPOS=5", 3000));            // 5 = GNRMC (konum, hız, saat)
}
