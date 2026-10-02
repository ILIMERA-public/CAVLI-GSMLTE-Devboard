/*
 * CAVLI GSM/LTE Devboard — 01 AT Komut Terminali (ESP32 ile)
 *
 * ESP32'yi USB ile Cavli C16QS arasında köprü yapar: Seri Monitöre yazdığınız
 * her AT komutu modeme gider, modemin cevabı ekrana gelir. Kartı ilk kez
 * denerken ve docs/AT_KOMUTLARI.md içindeki komutları sınarken kullanın.
 * Açılışta modem cevap vermiyorsa PKY pinine açma darbesi gönderir.
 *
 * Bağlantı (ESP32 DevKit örneği — pinleri kendi kartınıza göre değiştirin):
 *   CAVLI TX  → ESP32 GPIO16 (MODEM_RX_PIN)
 *   CAVLI RX  → ESP32 GPIO17 (MODEM_TX_PIN)
 *   CAVLI PKY → ESP32 GPIO4  (MODEM_PKY_PIN)
 *   CAVLI GND → ESP32 GND
 *   CAVLI 3V3 → anlık en az 2 A verebilen 3.3 V kaynak (ESP32'nin 3V3 pini YETMEZ)
 * Kart 3.3 V lojiktir; 5 V'luk bir denetleyicide TX/RX'e seviye dönüştürücü şarttır.
 *
 * Seri Monitör: 115200 baud, satır sonu "Both NL & CR".
 *
 * EN: USB ↔ Cavli C16QS AT terminal on an ESP32. Type AT commands in the Serial
 *     Monitor (115200, Both NL & CR). Sends a PKY pulse if the modem is silent.
 */

#include <Arduino.h>

// ─────────── KULLANICI AYARLARI ───────────
#define MODEM_RX_PIN   16     // ESP32 RX ← CAVLI TX
#define MODEM_TX_PIN   17     // ESP32 TX → CAVLI RX
#define MODEM_PKY_PIN  4      // ESP32 → CAVLI PKY
// PKY darbesinin etkin seviyesi. Modem açılmıyorsa HIGH ↔ LOW değiştirin.
#define PKY_BASILI     HIGH
#define PKY_SERBEST    LOW
// ──────────────────────────────────────────

#define MODEM_BAUD 115200     // C16QS fabrika ayarı
HardwareSerial modem(2);

bool modemCevapVeriyor() {
  for (int i = 0; i < 3; i++) {
    while (modem.available()) modem.read();
    modem.print("AT\r\n");
    uint32_t t = millis();
    String r;
    while (millis() - t < 1000) {
      while (modem.available()) r += (char)modem.read();
      if (r.indexOf("OK") >= 0) return true;
    }
  }
  return false;
}

void pkyDarbesi() {
  pinMode(MODEM_PKY_PIN, OUTPUT);
  digitalWrite(MODEM_PKY_PIN, PKY_SERBEST);
  delay(100);
  digitalWrite(MODEM_PKY_PIN, PKY_BASILI);
  delay(500);
  digitalWrite(MODEM_PKY_PIN, PKY_SERBEST);
}

void setup() {
  Serial.begin(115200);
  modem.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(500);
  Serial.println("\n=== CAVLI C16QS AT terminali ===");

  if (!modemCevapVeriyor()) {
    Serial.println("Modem cevap vermiyor, PKY ile aciliyor...");
    pkyDarbesi();
    uint32_t t = millis();
    while (!modemCevapVeriyor() && millis() - t < 20000) delay(500);
  }
  Serial.println("Hazir. Ornek: AT+CPIN?  AT+CSQ  AT+CEREG?  AT+CGMR");
}

void loop() {
  while (Serial.available()) modem.write(Serial.read());
  while (modem.available()) Serial.write(modem.read());
}
