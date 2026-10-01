/* ─────────────────────────────────────────────────────────────────────────────
 *  Cavli C16QS — AT komutlarıyla ilk bağlantı ve HTTP GET örneği
 *
 *  Akış:
 *    1) Modemi başlat (gerekiyorsa PWRKEY ile aç)
 *    2) SIM yuvası:  AT^SIMSWAP?   → 1 değilse  AT^SIMSWAP=1
 *    3) SIM hazır:   AT+CPIN?      → READY
 *    4) Şebeke:      AT+CEREG?     → kayıtlı
 *    5) İnternet:    AT+CGDCONT + AT+CGACT=1,1
 *    6) HTTP GET:    AT+HTTPURL / AT+HTTPREQUEST=GET → cevabı seriale bas
 *
 *  Donanım: ESP32-C3 + Cavli C16QS (modem UART1'de).
 * ───────────────────────────────────────────────────────────────────────────── */

#include <Arduino.h>

// ═════════════════════ KULLANICI AYARLARI (karta/SIM'e göre değiştirin) ══════════════════════
#define MODEM_TX_PIN      6                      // ESP TX → modem RX   (karta göre değişir)
#define MODEM_RX_PIN      5                      // ESP RX ← modem TX   (karta göre değişir)
#define MODEM_PWRKEY_PIN  0                      // PWRKEY GPIO         (karta göre değişir)
#define APN               "internet"             // operatör APN'i      (SIM'e göre değişir)
#define HTTP_URL          "http://example.com"   // istek atılacak adres
// ═════════════════════════════════════════════════════════════════════════════════════════════

// ── Sabit ayarlar (genelde değişmez) ─────────────────────────────────────────
#define MODEM_SERIAL      Serial1   // C16QS fabrika default UART hızı
#define MODEM_BAUD        115200
#define DEBUG_SERIAL      Serial     // USB/serial üzerinden log
#define DEBUG_BAUD        115200

// PWRKEY kartta invert transistörden geçiyor → GPIO HIGH = basılı.
// Transistörsüz bir kartta PRESS/RELEASE değerlerini ters çevirin.
#define MODEM_PWRKEY_PRESS    HIGH
#define MODEM_PWRKEY_RELEASE  LOW
#define MODEM_PWRKEY_ON_MS    500

// Zaman aşımları (ms)
#define TIMEOUT_AT_READY   20000
#define TIMEOUT_SIM        15000
#define TIMEOUT_NET_REG    30000
#define TIMEOUT_PDP_OPEN   30000
#define TIMEOUT_HTTP_SEND  30000

// ═════════════════════════════ AT yardımcıları ═══════════════════════════════

// Komutu gönderir, OK/ERROR gelene (ya da süre dolana) kadar cevabı okur ve döndürür.
String atSend(const char* cmd, uint32_t timeout_ms = 3000) {
    while (MODEM_SERIAL.available()) MODEM_SERIAL.read();
    MODEM_SERIAL.println(cmd);

    String resp;
    uint32_t t = millis();
    while (millis() - t < timeout_ms) {
        while (MODEM_SERIAL.available()) resp += (char)MODEM_SERIAL.read();
        if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) break;
        delay(10);
    }
    resp.trim();
    DEBUG_SERIAL.printf(">> %-28s | %s\n", cmd, resp.c_str());
    return resp;
}

// Belirli bir metin gelene kadar bekler (asenkron URC'ler için).
String atWaitFor(const char* token, uint32_t timeout_ms) {
    String resp;
    uint32_t t = millis();
    while (millis() - t < timeout_ms) {
        while (MODEM_SERIAL.available()) resp += (char)MODEM_SERIAL.read();
        if (resp.indexOf(token) >= 0) break;
        delay(10);
    }
    resp.trim();
    return resp;
}

// AT'ye OK gelene kadar yoklar.
bool waitReady(uint32_t timeout_ms) {
    uint32_t t = millis();
    while (millis() - t < timeout_ms) {
        if (atSend("AT", 1000).indexOf("OK") >= 0) return true;
        delay(500);
    }
    return false;
}

// PWRKEY açma darbesi (modem kapalıyken).
void pwrkeyPulse() {
    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    digitalWrite(MODEM_PWRKEY_PIN, MODEM_PWRKEY_RELEASE);
    delay(100);
    digitalWrite(MODEM_PWRKEY_PIN, MODEM_PWRKEY_PRESS);
    delay(MODEM_PWRKEY_ON_MS);
    digitalWrite(MODEM_PWRKEY_PIN, MODEM_PWRKEY_RELEASE);
}

// ═══════════════════════════════ Başlatma ════════════════════════════════════

// Modemi başlat: UART'ı aç, (gerekiyorsa) PWRKEY ile aç, AT'ye hazır olmasını bekle.
bool modemBegin() {
    MODEM_SERIAL.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    delay(100);

    if (!waitReady(3000)) {                 // modem cevap vermiyorsa kapalı → aç
        DEBUG_SERIAL.println("[MODEM] PWRKEY ile açılıyor...");
        pwrkeyPulse();
    }
    return waitReady(TIMEOUT_AT_READY);
}

// ═══════════════════════════ Bağlantı adımları ═══════════════════════════════

// SIM yuvasını sorgula; 1 değilse 1 yap.
void simSwapCheck() {
    if (atSend("AT^SIMSWAP?").indexOf("1") < 0) {
        DEBUG_SERIAL.println("[SIM] Yuva 1'e alınıyor...");
        atSend("AT^SIMSWAP=1", 5000);
        delay(1000);
    }
}

// SIM hazır (CPIN READY) olana kadar bekle.
bool simReady() {
    uint32_t t = millis();
    while (millis() - t < TIMEOUT_SIM) {
        if (atSend("AT+CPIN?").indexOf("READY") >= 0) return true;
        delay(1000);
    }
    return false;
}

// Şebekeye kayıt (CEREG: x,1 = home, x,5 = roaming) olana kadar bekle.
bool networkRegistered() {
    uint32_t t = millis();
    while (millis() - t < TIMEOUT_NET_REG) {
        String r = atSend("AT+CEREG?");
        if (r.indexOf(",1") >= 0 || r.indexOf(",5") >= 0) return true;
        delay(2000);
    }
    return false;
}

// APN'i ayarla ve veri bağlamını (PDP) aktive et.
bool pdpActivate() {
    atSend("AT+CGDCONT=1,\"IP\",\"" APN "\"", 5000);
    atSend("AT+CGACT=1,1", TIMEOUT_PDP_OPEN);
    return atSend("AT+CGACT?", 5000).indexOf("1,1") >= 0;
}

// HTTP GET isteği at ve cevabı seriale bas.
void httpGet(const char* url) {
    atSend((String("AT+HTTPURL=") + url).c_str(), 5000);

    DEBUG_SERIAL.println("[HTTP] GET gönderiliyor...");
    MODEM_SERIAL.println("AT+HTTPREQUEST=GET");
    atWaitFor("SUCCESS", TIMEOUT_HTTP_SEND);     // HTTPSEND:SUCCESS

    atSend("AT+HTTPGETSTAT?", 10000);            // STATUS_RESPONSE:<kod>
    String clen = atSend("AT+HTTPGETCLEN?", 10000);  // CONTENT_LENGTH:<n>

    int len = 0, p = clen.indexOf("CONTENT_LENGTH:");
    if (p >= 0) len = clen.substring(p + 15).toInt();

    DEBUG_SERIAL.println("\n──────── HTTP CEVAP ────────");
    if (len > 0) {
        String body = atSend((String("AT+HTTPGETCONT=0,") + len).c_str(), 30000);
        DEBUG_SERIAL.println(body);
    }
    DEBUG_SERIAL.println("────────────────────────────\n");
}

// ════════════════════════════════ setup / loop ═══════════════════════════════

void setup() {
    DEBUG_SERIAL.begin(DEBUG_BAUD);
    delay(300);
    DEBUG_SERIAL.println("\n=== Cavli C16QS AT örneği ===");

    if (!modemBegin())        { DEBUG_SERIAL.println("[HATA] Modem başlatılamadı.");   return; }
    atSend("ATE0");           // echo kapat
    atSend("AT+CMEE=2");      // hataları metin olarak ver

    simSwapCheck();
    if (!simReady())          { DEBUG_SERIAL.println("[HATA] SIM hazır değil.");       return; }
    if (!networkRegistered()) { DEBUG_SERIAL.println("[HATA] Şebekeye kaydolunamadı."); return; }
    if (!pdpActivate())       { DEBUG_SERIAL.println("[HATA] İnternet açılamadı.");     return; }

    httpGet(HTTP_URL);
    DEBUG_SERIAL.println("=== Bitti ===");
}

void loop() {
    // Örnek tek seferliktir.
}
