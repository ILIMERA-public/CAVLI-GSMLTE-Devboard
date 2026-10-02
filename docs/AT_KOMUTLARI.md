# Cavli C16QS AT Komutları — Hızlı Başvuru

CAVLI GSM/LTE Devboard ve CAVLI GSM/LTE Devboard Dahili GPS üzerindeki **Cavli C16QS** (LTE Cat 1.bis)
modülü için en sık kullanılan komutlar. Komutları
[`01_AT_Komut_Terminali`](../examples/01_AT_Komut_Terminali) örneğiyle Seri Monitörden (115200 baud,
**Both NL & CR**) doğrudan deneyebilirsiniz.

Tüm komutlar ve parametreleri için üreticinin **C16QS AT Command Manual** belgesine bakın
([Cavli ürün ve çözüm kılavuzları](https://www.cavliwireless.com/resources/product-and-solution-guides)).
Bu sayfa o kılavuzun özeti değil, kart için hazırlanmış kısa bir çalışma rehberidir.

> Modülün UART hızı fabrika ayarında **115200 baud**, 8N1'dir. Kart **3.3 V lojik** çalışır, pinleri 5 V
> toleranslı değildir. 3V3 beslemesi **anlık en az 2 A** verebilmelidir.

## 1. Temel kontrol

| Komut | Ne yapar | Örnek cevap |
| --- | --- | --- |
| `AT` | Modem cevap veriyor mu | `OK` |
| `ATE0` / `ATE1` | Komut yankısını kapat / aç | `OK` |
| `AT+CMEE=2` | Hataları metin olarak göster | `OK` |
| `ATI` | Modül bilgisi | |
| `AT+CGMR` | Modem yazılım sürümü | |
| `AT+CGSN` | IMEI | `86xxxxxxxxxxxxx` |
| `AT+CCLK?` | Modemin saati (şebekeden) | `+CCLK: "26/10/02,10:00:00+12"` |
| `AT+CFUN?` / `AT+CFUN=1` | Radyo durumu / radyoyu aç | `+CFUN: 1` |

## 2. SIM ve şebeke

| Komut | Ne yapar | Örnek cevap |
| --- | --- | --- |
| `AT^SIMSWAP?` | Hangi SIM kullanılıyor | `1` = harici SIM yuvası, `0` = dahili eSIM |
| `AT^SIMSWAP=1` | Karttaki Nano SIM yuvasını seç | `OK` |
| `AT+CPIN?` | SIM durumu | `+CPIN: READY` |
| `AT+ICCID` | SIM kart numarası | |
| `AT+CSQ` | Sinyal gücü (0–31, 99 = bilinmiyor) | `+CSQ: 24,99` |
| `AT+CESQ` | Ayrıntılı LTE sinyal ölçümü (RSRQ, RSRP) | |
| `AT+CEREG?` | LTE şebeke kaydı | `+CEREG: 0,1` (1 = ev, 5 = dolaşım, 2 = aranıyor) |
| `AT+COPS?` | Bağlı operatör | |

> **Kart Nano SIM'i görmüyorsa** önce `AT^SIMSWAP?` sorun. `0` dönüyorsa modül dahili eSIM'e bakıyordur;
> `AT^SIMSWAP=1` ile karttaki yuvaya geçin.

`+CSQ` değeri 10'un altındaysa anteni ve konumu kontrol edin. LTE anteni bağlı değilken modem şebekeye
kaydolamaz.

## 3. Veri bağlantısı (PDP)

```text
AT+CGDCONT=1,"IP","internet"     ← operatörünüzün APN'i
AT+CGACT=1,1                     ← veri bağlamını etkinleştir
AT+CGACT?                        → +CGACT: 1,1   (1,1 = etkin)
AT+CGPADDR=1                     → modemin aldığı IP adresi
AT+PING="8.8.8.8"                ← internet erişimini sına
```

## 4. HTTP (modemin dahili istemcisi)

```text
AT+HTTPCLEAN?                    ← önceki URL, başlık ve içeriği temizle
AT+HTTPURL=http://example.com    ← adres
AT+HTTPADDHEAD=Content-Type: application/json   ← (isteğe bağlı) başlık ekle
AT+HTTPCONTENT=27                ← (POST için) gövde uzunluğu, ardından tam 27 bayt gönderilir
AT+HTTPREQUEST=GET               → HTTPSEND: SUCCESS   (GET, POST veya HEAD)
AT+HTTPGETSTAT?                  → STATUS_RESPONSE:200
AT+HTTPGETCLEN?                  → CONTENT_LENGTH:1256
AT+HTTPGETCONT=0,1256            ← cevap gövdesi (başlangıç, bitiş)
AT+HTTPGETHEAD?                  ← cevap başlıkları
```

**HTTPS:** Sunucu sertifikası önce `AT+HTTPSLOAD` ile modeme yüklenir, istekte sertifika kimliği verilir:
`AT+HTTPREQUEST=GET,0,<ca_cert_id>`. Ayrıntı için üretici kılavuzunun HTTP bölümüne bakın.

## 5. MQTT (modemin dahili istemcisi)

```text
AT+MQTTCREATE=test.mosquitto.org,1883,istemci-adi,60,1
                                 → +MQTTCREATE: <kimlik> : CREATED
AT+MQTTCONN=<kimlik>,1,10        → +MQTTCONN: <kimlik>: CONNECTED   (1 = otomatik yeniden bağlan, 10 sn)
AT+MQTTSUBUNSUB=<kimlik>,konu/komut,1,1
                                 → SUBSCRIBE SUCCESS   (1 = abone ol, QoS 1)
AT+MQTTPUB=<kimlik>,konu/veri,merhaba,0,0,0
                                 → PUBLISH SUCCESS     (QoS, duplicate, retain)
AT+MQTTSTATUS=<kimlik>           → +MQTTSTATUS: 1      (1 = bağlı)
AT+MQTTDISCONN=<kimlik>
AT+MQTTDELETE=<kimlik>
```

Gelen mesajlar `+MQTTPUBLISH: <kimlik>,<qos>,<konu>,<uzunluk>,<mesaj>` satırı olarak düşer.
Kullanıcı adı/şifre, son vasiyet (LWT) ve MQTT sürümü `AT+MQTTCREATE`'in isteğe bağlı parametreleridir.
Büyük mesajlar için `AT+MQTTPUBLM`, TLS için `AT+MQTTSLOAD` ve `AT+MQTTSCONN` kullanılır.

## 6. SMS

| Komut | Ne yapar |
| --- | --- |
| `AT+CMGF=1` | Metin modu (önce bunu verin) |
| `AT+CSCS="GSM"` | Karakter kümesi |
| `AT+CMGS="+905XXXXXXXXX"` | SMS yaz: `>` gelince metni yazın, **Ctrl+Z** (0x1A) ile gönderin → `+CMGS: <no>` |
| `AT+CNMI=2,2,0,0,0` | Gelen SMS'i saklamadan seri hatta ver: `+CMT: "+905...",...` ve alt satırda metin |
| `AT+CMGL="ALL"` | Kayıtlı mesajları listele |
| `AT+CMGR=<no>` | Mesaj oku |
| `AT+CMGD=<no>` | Mesaj sil |

LTE şebekede SMS, operatörün desteğine bağlıdır. C16QS bir veri modülüdür; sesli arama örnekleri bu depoda yoktur.

## 7. GNSS / GPS (yalnız **Dahili GPS** sürümü)

| Komut | Ne yapar |
| --- | --- |
| `AT+CGPS=1` / `AT+CGPS=0` | GNSS oturumunu başlat / durdur |
| `AT+CGPSGPOS=1` | GGA: konum, uydu sayısı, yükseklik |
| `AT+CGPSGPOS=2` | GSA: kullanılan uydular ve DOP değerleri |
| `AT+CGPSGPOS=3` / `=4` | GPS / BeiDou görünen uydular ve sinyal gücü |
| `AT+CGPSGPOS=5` | RMC: konum, hız, rota, saat ve tarih (en pratik olanı) |
| `AT+GPSPORT=1` | NMEA akışını AT portuna yönlendir |
| `AT+CGPSHOT` / `AT+CGPSWARM` / `AT+CGPSCOLD` | Sıcak / ılık / soğuk başlangıç |
| `AT+CGPSRST` | GNSS'i sıfırla |
| `AT+CGPSAGNSS=1` | Destekli GNSS (A-GNSS): veri bağlantısı varken ilk konumu hızlandırır |

RMC cevabındaki ikinci alan `A` ise konum geçerlidir, `V` ise henüz bulunamamıştır. Enlem ve boylam NMEA
`ddmm.mmmm` biçimindedir; ondalık dereceye çevirme kodu
[`05_GPS_Konum_Okuma`](../examples/05_GPS_Konum_Okuma) örneğindedir.

## 8. Sık karşılaşılan durumlar

| Belirti | Olası neden |
| --- | --- |
| `AT`'ye cevap yok | Modem kapalı (PKY darbesi), TX/RX ters bağlı, 3V3 beslemesi yetersiz ya da baud hızı 115200 değil |
| `+CPIN: SIM PIN` | SIM'in PIN kodu açık: `AT+CPIN="1234"` ya da PIN'i telefonda kapatın |
| SIM hiç görünmüyor | `AT^SIMSWAP=1` ile harici yuvayı seçin; SIM'in kesik köşesini kontrol edin |
| `+CEREG: 0,2` uzun sürüyor | Şebeke aranıyor: LTE anteni takılı mı, bölgede kapsama var mı |
| `+CEREG: 0,3` | Kayıt reddedildi: SIM aktif mi, veri paketi tanımlı mı |
| Veri gönderirken modem yeniden başlıyor | 3V3 kaynağı anlık 2 A veremiyor: güçlü bir regülatör ve kısa kablo kullanın |
| `HTTPSEND: FAIL` | APN yanlış, veri bağlamı açık değil (`AT+CGACT?`) ya da HTTPS sertifikası yüklenmemiş |
| `AT+CGPS=1` → `ERROR` | Kart GPS'siz sürüm ya da GNSS anteni bağlı değil |
