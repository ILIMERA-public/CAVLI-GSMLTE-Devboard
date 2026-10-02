/*
 * CAVLI GSM/LTE Devboard — 02 LTE üzerinden HTTP POST (JSON gövde)
 *
 * Modemi açar, SIM ve şebekeyi bekler, veri bağlamını (PDP) etkinleştirir ve
 * her 60 saniyede bir sunucuya JSON gövdeli POST gönderir; durum kodunu ve
 * cevabı Seri Monitöre basar. Modemin kendi HTTP istemcisi kullanılır, ESP32
 * tarafında TCP/IP yığını gerekmez.
 *
 * Basit GET örneği için deponun kökündeki src/main.cpp'ye bakın.
 * Varsayılan adres httpbin.org/post: gönderdiğiniz JSON'u geri döndürür.
 * HTTPS için önce sertifika yüklenmelidir (AT+HTTPSLOAD, bkz. docs/AT_KOMUTLARI.md).
 *
 * Bağlantı: 01_AT_Komut_Terminali ile aynı (TX→16, RX←17, PKY←4, GND, 3V3 ≥ 2 A).
 *
 * EN: Brings up LTE and POSTs a JSON body every 60 s using the modem's built-in
 *     HTTP client, printing the status code and response.
 */

#include <Arduino.h>

// ─────────── KULLANICI AYARLARI ───────────
#define MODEM_RX_PIN   16
#define MODEM_TX_PIN   17
#define MODEM_PKY_PIN  4
#define PKY_BASILI     HIGH       // modem açılmıyorsa HIGH ↔ LOW değiştirin
#define PKY_SERBEST    LOW
#define APN            "internet" // operatörünüzün APN'i
#define POST_URL       "http://httpbin.org/post"
const uint32_t GONDERIM_ARALIGI_MS = 60000;
// ──────────────────────────────────────────

HardwareSerial modem(2);

// ═══════════ AT yardımcıları ═══════════
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
  Serial.printf(">> %-34s | %s\n", komut.c_str(), c.c_str());
  return c;
}

bool bekle(const String& komut, const char* aranan, uint32_t toplam, uint32_t aralik = 1000) {
  uint32_t t = millis();
  while (millis() - t < toplam) {
    if (atGonder(komut).indexOf(aranan) >= 0) return true;
    delay(aralik);
  }
  return false;
}

void pkyDarbesi() {
  pinMode(MODEM_PKY_PIN, OUTPUT);
  digitalWrite(MODEM_PKY_PIN, PKY_SERBEST); delay(100);
  digitalWrite(MODEM_PKY_PIN, PKY_BASILI);  delay(500);
  digitalWrite(MODEM_PKY_PIN, PKY_SERBEST);
}

bool kayitli() {
  String r = atGonder("AT+CEREG?");
  return r.indexOf(",1") >= 0 || r.indexOf(",5") >= 0;   // 1 = ev şebekesi, 5 = dolaşım
}

bool lteBaslat() {
  modem.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(200);
  if (!bekle("AT", "OK", 3000, 300)) { pkyDarbesi(); if (!bekle("AT", "OK", 20000, 500)) return false; }
  atGonder("ATE0");
  atGonder("AT+CMEE=2");
  if (atGonder("AT^SIMSWAP?").indexOf("1") < 0) { atGonder("AT^SIMSWAP=1", 5000); delay(1000); }  // 1 = harici SIM yuvası
  if (!bekle("AT+CPIN?", "READY", 15000)) { Serial.println("[HATA] SIM hazir degil"); return false; }
  uint32_t t = millis();
  while (!kayitli()) { if (millis() - t > 60000) { Serial.println("[HATA] Sebeke kaydi yok"); return false; } delay(2000); }
  atGonder("AT+CSQ");
  atGonder(String("AT+CGDCONT=1,\"IP\",\"") + APN + "\"", 5000);
  atGonder("AT+CGACT=1,1", 30000);
  if (atGonder("AT+CGACT?", 5000).indexOf("1,1") < 0) { Serial.println("[HATA] Veri baglami acilamadi"); return false; }
  atGonder("AT+CGPADDR=1");
  return true;
}

// ═══════════ HTTP POST ═══════════
void httpPost(const String& json) {
  atGonder("AT+HTTPCLEAN?");                                  // önceki URL, başlık ve içeriği temizle
  atGonder(String("AT+HTTPURL=") + POST_URL, 5000);
  atGonder("AT+HTTPADDHEAD=Content-Type: application/json");

  // Gövde: uzunluk bildirilir, ardından tam o kadar bayt gönderilir
  atGonder(String("AT+HTTPCONTENT=") + json.length(), 1000, ">");
  modem.print(json);
  String r; uint32_t t = millis();
  while (millis() - t < 5000) { while (modem.available()) r += (char)modem.read(); if (r.indexOf("OK") >= 0) break; }

  modem.print("AT+HTTPREQUEST=POST\r\n");
  r = ""; t = millis();
  while (millis() - t < 30000) { while (modem.available()) r += (char)modem.read(); if (r.indexOf("HTTPSEND") >= 0 && r.indexOf('\n', r.indexOf("HTTPSEND")) > 0) break; }
  Serial.println("Gonderim: " + r);
  if (r.indexOf("SUCCESS") < 0) return;

  atGonder("AT+HTTPGETSTAT?", 10000);                         // STATUS_RESPONSE:200
  String clen = atGonder("AT+HTTPGETCLEN?", 10000);           // CONTENT_LENGTH:<n>
  int p = clen.indexOf("CONTENT_LENGTH:");
  int uzunluk = p >= 0 ? clen.substring(p + 15).toInt() : 0;
  if (uzunluk > 0) atGonder(String("AT+HTTPGETCONT=0,") + min(uzunluk, 1024), 20000);
}

bool hazir = false;
uint32_t sonGonderim = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== CAVLI LTE HTTP POST ===");
  hazir = lteBaslat();
  if (!hazir) Serial.println("[HATA] Besleme (3V3 >= 2 A), anten, SIM ve APN'i kontrol edin.");
}

void loop() {
  if (!hazir) return;
  if (sonGonderim == 0 || millis() - sonGonderim >= GONDERIM_ARALIGI_MS) {
    sonGonderim = millis();
    String json = String("{\"cihaz\":\"cavli-devboard\",\"calisma_suresi_s\":") + (millis() / 1000) +
                  ",\"sicaklik\":" + String(temperatureRead(), 1) + "}";
    httpPost(json);
    if (!kayitli()) { Serial.println("Sebeke koptu, yeniden baglaniliyor..."); hazir = lteBaslat(); }
  }
}
