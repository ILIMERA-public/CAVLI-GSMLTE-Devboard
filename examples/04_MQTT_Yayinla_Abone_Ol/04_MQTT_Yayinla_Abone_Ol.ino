/*
 * CAVLI GSM/LTE Devboard — 04 LTE üzerinden MQTT (modemin dahili istemcisi)
 *
 * Cavli C16QS'in kendi MQTT istemcisini kullanır; ESP32 tarafında MQTT
 * kütüphanesi gerekmez:
 *   - her 30 saniyede <KONU>/telemetri konusuna JSON yayınlar,
 *   - <KONU>/komut konusuna abone olur, "AC" / "KAPAT" ile CIKIS_PIN'i sürer.
 *
 * Komut akışı: AT+MQTTCREATE (istemci oluştur, kimlik numarası döner) →
 * AT+MQTTCONN (bağlan, otomatik yeniden bağlanma açık) → AT+MQTTSUBUNSUB (abone ol) →
 * AT+MQTTPUB (yayınla). Gelen mesajlar +MQTTPUBLISH satırı olarak düşer.
 *
 * Deneme için herkese açık test.mosquitto.org kullanılır; gizli veri göndermeyin.
 * Bilgisayardan izlemek için:
 *   mosquitto_sub -h test.mosquitto.org -t "ilimera/cavli/#" -v
 *   mosquitto_pub -h test.mosquitto.org -t "ilimera/cavli/<IMEI>/komut" -m AC
 *
 * Bağlantı: 01_AT_Komut_Terminali ile aynı (TX→16, RX←17, PKY←4, GND, 3V3 ≥ 2 A).
 *
 * EN: Uses the C16QS built-in MQTT client: publishes JSON telemetry every 30 s and
 *     subscribes to a command topic ("AC"/"KAPAT" = on/off).
 */

#include <Arduino.h>

// ─────────── KULLANICI AYARLARI ───────────
#define MODEM_RX_PIN   16
#define MODEM_TX_PIN   17
#define MODEM_PKY_PIN  4
#define PKY_BASILI     HIGH
#define PKY_SERBEST    LOW
#define APN            "internet"
#define MQTT_SUNUCU    "test.mosquitto.org"
#define MQTT_PORT      1883
#define CIKIS_PIN      2
const uint32_t YAYIN_ARALIGI_MS = 30000;
// ──────────────────────────────────────────

HardwareSerial modem(2);
String istemci;        // AT+MQTTCREATE'in döndürdüğü kimlik
String konu;           // ilimera/cavli/<IMEI>
String satir;
uint32_t sonYayin = 0;
bool bagli = false;

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
  Serial.printf(">> %-40s | %s\n", komut.c_str(), c.c_str());
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

bool lteBaslat() {
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
  atGonder(String("AT+CGDCONT=1,\"IP\",\"") + APN + "\"", 5000);
  atGonder("AT+CGACT=1,1", 30000);
  return atGonder("AT+CGACT?", 5000).indexOf("1,1") >= 0;
}

String imei() {
  String r = atGonder("AT+CGSN");
  String rakamlar;
  for (char c : r) if (isDigit(c)) rakamlar += c;
  return rakamlar.length() >= 15 ? rakamlar.substring(0, 15) : String((uint32_t)ESP.getEfuseMac(), HEX);
}

bool mqttBaglan() {
  // AT+MQTTCREATE=<sunucu>,<port>,<istemci_adı>,<keepalive_s>,<cleansession>
  String r = atGonder(String("AT+MQTTCREATE=") + MQTT_SUNUCU + "," + MQTT_PORT + ",ilimera-" +
                      konu.substring(konu.lastIndexOf('/') + 1) + ",60,1", 10000);
  int p = r.indexOf("+MQTTCREATE:");
  if (p < 0) return false;
  istemci = r.substring(p + 12, r.indexOf(':', p + 12));        // "+MQTTCREATE: 3 : CREATED" → "3"
  istemci.trim();

  // AT+MQTTCONN=<kimlik>,<yeniden_bağlan 1>,<bekleme_s>
  r = atGonder("AT+MQTTCONN=" + istemci + ",1,10", 30000, "CONNECTED");
  if (r.indexOf("CONNECTED") < 0 || r.indexOf("FAIL") >= 0) return false;

  // AT+MQTTSUBUNSUB=<kimlik>,<konu>,<1=abone ol>,<qos>
  atGonder("AT+MQTTSUBUNSUB=" + istemci + "," + konu + "/komut,1,1", 15000, "SUCCESS");
  Serial.println("Komut konusu: " + konu + "/komut");
  return true;
}

void yayinla(const String& altKonu, const String& mesaj) {
  // AT+MQTTPUB=<kimlik>,<konu>,<mesaj>,<qos>,<duplicate>,<retain>
  atGonder("AT+MQTTPUB=" + istemci + "," + konu + "/" + altKonu + "," + mesaj + ",0,0,0", 15000, "PUBLISH");
}

// Gelen mesaj: +MQTTPUBLISH: <kimlik>,<qos>,<konu>,<uzunluk>,<mesaj>
void satirIsle(const String& s) {
  if (s.indexOf("DISCONNECTED") >= 0) { bagli = false; return; }
  if (!s.startsWith("+MQTTPUBLISH:")) return;
  String mesaj = s.substring(s.lastIndexOf(',') + 1);
  mesaj.trim();
  mesaj.toUpperCase();
  Serial.println("Gelen komut: " + mesaj);
  if (mesaj == "AC" || mesaj == "1")         digitalWrite(CIKIS_PIN, HIGH);
  else if (mesaj == "KAPAT" || mesaj == "0") digitalWrite(CIKIS_PIN, LOW);
  yayinla("cikis", digitalRead(CIKIS_PIN) ? "ACIK" : "KAPALI");
}

void setup() {
  Serial.begin(115200);
  pinMode(CIKIS_PIN, OUTPUT);
  digitalWrite(CIKIS_PIN, LOW);
  delay(300);
  Serial.println("\n=== CAVLI LTE MQTT ===");
  if (!lteBaslat()) { Serial.println("[HATA] LTE baglantisi kurulamadi: besleme, anten, SIM, APN."); return; }
  konu = "ilimera/cavli/" + imei();
  bagli = mqttBaglan();
  if (!bagli) Serial.println("[HATA] MQTT sunucusuna baglanilamadi.");
}

void loop() {
  while (modem.available()) {
    char ch = modem.read();
    if (ch == '\n') { satir.trim(); if (satir.length()) satirIsle(satir); satir = ""; }
    else if (ch != '\r' && satir.length() < 300) satir += ch;
  }

  if (!bagli) {
    static uint32_t sonDeneme = 0;
    if (millis() - sonDeneme > 30000) {
      sonDeneme = millis();
      if (istemci.length()) atGonder("AT+MQTTDELETE=" + istemci);
      bagli = mqttBaglan();
    }
    return;
  }

  if (millis() - sonYayin >= YAYIN_ARALIGI_MS) {
    sonYayin = millis();
    // AT+MQTTPUB parametreleri virgülle ayrıldığından mesaj virgülsüz tutuldu.
    // Virgüllü ya da uzun JSON için AT+MQTTPUBLM (büyük mesaj) komutunu kullanın.
    String json = String("{\"calisma_suresi_s\":") + (millis() / 1000) + "}";
    yayinla("telemetri", json);
  }
}
