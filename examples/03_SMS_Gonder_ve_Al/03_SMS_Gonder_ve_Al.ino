/*
 * CAVLI GSM/LTE Devboard — 03 SMS Gönderme ve Alma
 *
 * Açılışta yetkili numaraya "hazir" SMS'i gönderir, ardından gelen SMS'leri
 * dinler. Yetkili numaradan gelen komutlar:
 *   CIKIS AC     → CIKIS_PIN HIGH (ör. röle modülü)
 *   CIKIS KAPAT  → CIKIS_PIN LOW
 *   DURUM        → sinyal gücü ve çıkış durumunu SMS ile yanıtlar
 *
 * SIM'inizde SMS paketi olmalıdır. 4G (LTE) şebekede SMS operatörün desteğine bağlıdır.
 * Bağlantı: 01_AT_Komut_Terminali ile aynı (TX→16, RX←17, PKY←4, GND, 3V3 ≥ 2 A).
 *
 * EN: Sends a "ready" SMS on boot and listens for SMS commands from an authorised
 *     number (CIKIS AC / CIKIS KAPAT / DURUM = output on / off / status).
 */

#include <Arduino.h>

// ─────────── KULLANICI AYARLARI ───────────
#define MODEM_RX_PIN   16
#define MODEM_TX_PIN   17
#define MODEM_PKY_PIN  4
#define PKY_BASILI     HIGH
#define PKY_SERBEST    LOW
#define YETKILI_NUMARA "+905XXXXXXXXX"
#define CIKIS_PIN      2
// ──────────────────────────────────────────

HardwareSerial modem(2);
String satir, bekleyenNumara;
bool metinBekleniyor = false;

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
  Serial.printf(">> %-30s | %s\n", komut.c_str(), c.c_str());
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

bool modemHazirla() {
  modem.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(200);
  if (!bekle("AT", "OK", 3000, 300)) { pkyDarbesi(); if (!bekle("AT", "OK", 20000, 500)) return false; }
  atGonder("ATE0");
  atGonder("AT+CMEE=2");
  if (atGonder("AT^SIMSWAP?").indexOf("1") < 0) { atGonder("AT^SIMSWAP=1", 5000); delay(1000); }
  if (!bekle("AT+CPIN?", "READY", 15000)) return false;
  uint32_t t = millis();
  for (;;) {
    String r = atGonder("AT+CEREG?");
    if (r.indexOf(",1") >= 0 || r.indexOf(",5") >= 0) break;
    if (millis() - t > 60000) return false;
    delay(2000);
  }
  atGonder("AT+CMGF=1");                 // metin modu
  atGonder("AT+CSCS=\"GSM\"");           // karakter kümesi
  atGonder("AT+CNMI=2,2,0,0,0");         // gelen SMS'i saklamadan seri hatta ver (+CMT)
  return true;
}

bool smsGonder(const char* numara, const String& metin) {
  atGonder(String("AT+CMGS=\"") + numara + "\"", 5000, ">");
  modem.print(metin);
  modem.write(0x1A);                     // Ctrl+Z: gönder
  String r; uint32_t t = millis();
  while (millis() - t < 30000) {
    while (modem.available()) r += (char)modem.read();
    if (r.indexOf("+CMGS:") >= 0 || r.indexOf("ERROR") >= 0) break;
  }
  bool ok = r.indexOf("+CMGS:") >= 0;
  Serial.println(ok ? "SMS gonderildi" : "SMS gonderilemedi: " + r);
  return ok;
}

String sinyal() {
  String r = atGonder("AT+CSQ");         // +CSQ: <rssi>,<ber>
  int p = r.indexOf("+CSQ:");
  return p >= 0 ? r.substring(p + 6, r.indexOf(',', p)) : String("?");
}

void komutIsle(const String& numara, String metin) {
  metin.trim();
  metin.toUpperCase();
  Serial.println("SMS [" + numara + "]: " + metin);
  if (numara != YETKILI_NUMARA) { Serial.println("Yetkisiz numara, yok sayildi."); return; }

  if (metin == "CIKIS AC")         digitalWrite(CIKIS_PIN, HIGH);
  else if (metin == "CIKIS KAPAT") digitalWrite(CIKIS_PIN, LOW);
  else if (metin != "DURUM")       { smsGonder(YETKILI_NUMARA, "Komutlar: CIKIS AC, CIKIS KAPAT, DURUM"); return; }

  smsGonder(YETKILI_NUMARA, String("Cikis: ") + (digitalRead(CIKIS_PIN) ? "ACIK" : "KAPALI") +
                            ", sinyal: " + sinyal() + "/31");
}

void setup() {
  Serial.begin(115200);
  pinMode(CIKIS_PIN, OUTPUT);
  digitalWrite(CIKIS_PIN, LOW);
  delay(300);
  Serial.println("\n=== CAVLI SMS gonder/al ===");
  if (!modemHazirla()) { Serial.println("[HATA] Modem hazir degil: besleme, anten, SIM."); return; }
  smsGonder(YETKILI_NUMARA, "CAVLI GSM/LTE Devboard hazir. Komutlar: CIKIS AC, CIKIS KAPAT, DURUM");
}

// Gelen SMS iki satırdır:  +CMT: "+905XXXXXXXXX","","26/10/02,10:00:00+12"   ve alt satırda metin
void loop() {
  while (modem.available()) {
    char ch = modem.read();
    if (ch == '\n') {
      satir.trim();
      if (metinBekleniyor && satir.length()) {
        metinBekleniyor = false;
        komutIsle(bekleyenNumara, satir);
      } else if (satir.startsWith("+CMT:")) {
        int a = satir.indexOf('"'), b = satir.indexOf('"', a + 1);
        bekleyenNumara = (a >= 0 && b > a) ? satir.substring(a + 1, b) : String();
        metinBekleniyor = true;
      }
      satir = "";
    } else if (ch != '\r' && satir.length() < 200) {
      satir += ch;
    }
  }
}
