#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// =========================================================================
// Librerias necesarias (Arduino IDE > Administrador de bibliotecas):
//   - DFRobotDFPlayerMini   (DFRobot)
//   - EspSoftwareSerial     (Peter Lerup) - link con el ESP32 de la pantalla
//   - ArduinoJson           (Benoit Blanchon)
//
// La pantalla HUB75 NO se maneja desde esta placa: vive en un segundo
// ESP32 dedicado (ver pantalla_led/pantalla_led.ino) para no competir por
// GPIOs ni tiempo de CPU con el GPRS/GPS/DFPlayer. Este tracker solo le
// manda el texto a mostrar por un link serial de un cable (ver
// LINK_TX_PIN / LINK_RX_PIN mas abajo).
// =========================================================================

// -------------------------------------------------------------------------
// Pines (ver tabla en README del proyecto)
// -------------------------------------------------------------------------
// SIM808 UART2: RX=GPIO34 (input-only, recibe TX del SIM808), TX=GPIO17
#define SIM808_RX_PIN 34
#define SIM808_TX_PIN 17

// PWRKEY del SIM808: cable soldado a la pata PWRKEY del boton fisico S2
#define SIM808_PWRKEY_PIN 2

// DFPlayer Mini UART1: RX=GPIO14, TX=GPIO13 (resistor 1k en serie hacia RX del DFPlayer)
#define DFPLAYER_RX_PIN 14
#define DFPLAYER_TX_PIN 13

// Link serial hacia el ESP32 de la pantalla (ver pantalla_led.ino).
// Pines libres ahora que el HUB75 se fue a la otra placa. UART0/1/2 de
// hardware ya estan ocupados (debug/DFPlayer/SIM808), por eso este es un
// SoftwareSerial.
#define LINK_RX_PIN 26 // sin uso real por ahora, queda para un futuro ACK
#define LINK_TX_PIN 25

HardwareSerial sim808(2);          // UART2 - SIM808
HardwareSerial dfSerial(1);        // UART1 - DFPlayer Mini
DFRobotDFPlayerMini dfPlayer;
SoftwareSerial pantallaLink(LINK_RX_PIN, LINK_TX_PIN); // hacia el ESP32 de la pantalla

// --- CONFIGURACION GPRS (INTERNET) ---
// Descomenta la linea de tu pais/APN correcto:

// CLARO ARGENTINA
String apn  = "igprs.claro.com.ar";
String user = "clarogprs";
String pass = "clarogprs999";

// CLARO CHILE (Descomentar si es Chile)
// String apn  = "bam.clarochile.cl";
// String user = "clarochile";
// String pass = "clarochile";

// CLARO URUGUAY (Descomentar si es Uruguay)
// String apn  = "igprs.claro.com.uy";
// String user = "f";
// String pass = "f";

// --- ThingSpeak: canal de telemetria (ya en uso) ---
String tsApiKey  = "YZG43OQW28F0SML7";

// --- ThingSpeak: canal de configuracion remota (pendiente de crear) ---
String configChannelId  = "COMPLETAR_CHANNEL_ID";
String configReadApiKey = "COMPLETAR_READ_API_KEY";

// --- CONTADOR DE CONSUMO DE DATOS SIM808 ---
unsigned long totalBytesSent     = 0;
unsigned long totalBytesReceived = 0;
unsigned long totalRequests      = 0;
unsigned long dataStartMs        = 0;

// -------------------------------------------------------------------------
// Parametros configurables remotamente (valores por defecto, se
// sobreescriben desde el canal de config de ThingSpeak, ver revisarConfigRemota())
// -------------------------------------------------------------------------
int volumenActual              = 20;      // 0-30 (DFPlayer)
String textoDefault            = "LINEA 102 - VIXEL";
int radioDeteccionParada       = 30;      // metros
unsigned long intervaloSubidaMs = 15000;  // ms entre lecturas GPS / envios

long ultimoEntryIdConfig = -1;
unsigned long ultimaRevisionConfigMs = 0;
const unsigned long INTERVALO_REVISION_CONFIG_MS = 60000; // cada 60s

// -------------------------------------------------------------------------
// Paradas del Ramal A: coordenadas placeholder, PENDIENTE reemplazar por
// las coordenadas reales de las 4 paradas donde debe sonar cada audio.
// track = numero de pista en la SD del DFPlayer (001.mp3 .. 004.mp3)
// -------------------------------------------------------------------------
struct Parada {
  double lat;
  double lon;
  int radioDeteccion; // metros (se sincroniza con radioDeteccionParada)
  int track;
  String texto;       // texto mostrado en la pantalla LED al anunciar
};

Parada paradas[] = {
  // TODO: coordenadas reales pendientes (placeholder cerca de -34.63)
  { -34.6300, -58.4300, 30, 1, "PROXIMA PARADA 1" },
  { -34.6310, -58.4310, 30, 2, "PROXIMA PARADA 2" },
  { -34.6320, -58.4320, 30, 3, "PROXIMA PARADA 3" },
  { -34.6330, -58.4330, 30, 4, "PROXIMA PARADA 4" },
};
const int NUM_PARADAS = sizeof(paradas) / sizeof(paradas[0]);
bool paradaAnunciada[NUM_PARADAS] = { false, false, false, false };

bool mostrandoTextoParada = false;
unsigned long mostrandoParadaHastaMs = 0;
const unsigned long DURACION_TEXTO_PARADA_MS = 20000;

// -------------------------------------------------------
// Enviar comando AT y retornar respuesta completa
// Usa timeout ABSOLUTO para no quedar atrapado si el SIM808
// envia datos continuos (boot dots, NMEA, etc.)
// -------------------------------------------------------
String sendAT(const String& cmd, unsigned long timeout_ms = 2000) {
  while (sim808.available()) sim808.read(); // limpiar buffer previo
  sim808.println(cmd);
  String resp = "";
  unsigned long start = millis();
  while (millis() - start < timeout_ms) {
    while (sim808.available() && resp.length() < 512) {
      resp += (char)sim808.read();
    }
    // Salir en cuanto la respuesta AT este completa
    if (resp.indexOf("OK\r\n")    != -1 ||
        resp.indexOf("OK\n")      != -1 ||
        resp.indexOf("ERROR\r\n") != -1 ||
        resp.indexOf("ERROR\n")   != -1) break;
    delay(1); // alimentar watchdog del ESP32
  }
  return resp;
}

// -------------------------------------------------------
// Verificar si el bearer GPRS esta activo y tiene IP
// Retorna true si "+SAPBR: 1,1," esta en la respuesta
// -------------------------------------------------------
bool checkBearer() {
  String resp = sendAT("AT+SAPBR=2,1", 3000);
  Serial.print("[GPRS] Estado bearer: "); Serial.println(resp);
  return resp.indexOf("+SAPBR: 1,1,") != -1;
}

// -------------------------------------------------------
// Configurar y abrir bearer GPRS
// Retorna true si logro conectar
// -------------------------------------------------------
bool configureGPRS() {
  Serial.println("--- Configurando GPRS ---");

  // Cerrar bearer si estaba abierto
  sendAT("AT+SAPBR=0,1", 5000);

  sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  sendAT("AT+SAPBR=3,1,\"APN\",\""  + apn  + "\"");
  sendAT("AT+SAPBR=3,1,\"USER\",\"" + user + "\"");
  sendAT("AT+SAPBR=3,1,\"PWD\",\""  + pass + "\"");

  Serial.println("Abriendo bearer (AT+SAPBR=1,1)...");
  String resp = sendAT("AT+SAPBR=1,1", 8000); // hasta 8s para conectar
  Serial.print("[GPRS] Resp apertura: "); Serial.println(resp);

  // Verificar que quedo conectado
  if (checkBearer()) {
    Serial.println("[GPRS] Bearer conectado con IP.");
    return true;
  }

  Serial.println("[GPRS] ERROR: bearer no obtuvo IP.");
  return false;
}

// -------------------------------------------------------
// Asegurar GPRS activo, reintentar si hace falta
// -------------------------------------------------------
bool ensureGPRS() {
  if (checkBearer()) return true;
  Serial.println("[GPRS] Desconectado. Reconectando...");
  return configureGPRS();
}

// -------------------------------------------------------
// Simular el apretón del boton fisico S2 (PWRKEY) para que
// el SIM808 arranque solo al energizar, sin intervencion manual.
// El SIM808 exige minimo ~1s de PWRKEY a GND para encender.
// -------------------------------------------------------
void encenderSIM808() {
  Serial.println("Encendiendo SIM808 (simulando boton PWRKEY)...");
  pinMode(SIM808_PWRKEY_PIN, OUTPUT);
  digitalWrite(SIM808_PWRKEY_PIN, LOW);
  delay(1500);
  pinMode(SIM808_PWRKEY_PIN, INPUT); // alta impedancia: no interfiere con el boton fisico
  delay(3000); // dar tiempo a que el modulo arranque
}

// -------------------------------------------------------
// Pedirle al ESP32 de la pantalla que muestre un texto, mandandolo por el
// link serial (protocolo de linea: "TXT:<texto>\n", ver pantalla_led.ino)
// -------------------------------------------------------
void mostrarTexto(const String& texto) {
  pantallaLink.print("TXT:");
  pantallaLink.print(texto);
  pantallaLink.print("\n");
}

// -------------------------------------------------------
// Distancia entre dos coordenadas GPS (formula de Haversine), en metros
// -------------------------------------------------------
double distanciaMetros(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0; // radio terrestre en metros
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(radians(lat1)) * cos(radians(lat2)) * sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

// -------------------------------------------------------
// Revisar si el colectivo entro al radio de alguna parada y,
// si es asi, reproducir el audio correspondiente y mostrarlo en pantalla.
// Usa un flag por parada + histeresis (3x el radio) para no repetir el
// anuncio mientras el bus sigue dentro del radio de deteccion.
// -------------------------------------------------------
void revisarParadas(double latActual, double lonActual) {
  for (int i = 0; i < NUM_PARADAS; i++) {
    double d = distanciaMetros(latActual, lonActual, paradas[i].lat, paradas[i].lon);

    if (d <= paradas[i].radioDeteccion) {
      if (!paradaAnunciada[i]) {
        paradaAnunciada[i] = true;
        Serial.print("[PARADA] Anunciando parada "); Serial.print(i + 1);
        Serial.print(" (dist="); Serial.print(d, 1); Serial.println("m)");

        dfPlayer.play(paradas[i].track);
        mostrarTexto(paradas[i].texto);
        mostrandoTextoParada = true;
        mostrandoParadaHastaMs = millis() + DURACION_TEXTO_PARADA_MS;
      }
    } else if (d > paradas[i].radioDeteccion * 3) {
      paradaAnunciada[i] = false; // se alejo lo suficiente, se puede reanunciar en la vuelta
    }
  }

  // Volver al texto por defecto una vez que paso el tiempo de anuncio
  if (mostrandoTextoParada && millis() > mostrandoParadaHastaMs) {
    mostrandoTextoParada = false;
    mostrarTexto(textoDefault);
  }
}

void setup() {
  Serial.begin(115200);
  // Esperar Serial con timeout (evita bloqueo sin USB)
  unsigned long sw = millis();
  while (!Serial && millis() - sw < 3000);
  delay(500);

  dataStartMs = millis();
  Serial.println("--- GPS CLOUD TRACKER SIM808 ---");
  Serial.println("Inicializando...");

  encenderSIM808();

  sim808.begin(9600, SERIAL_8N1, SIM808_RX_PIN, SIM808_TX_PIN);
  delay(2000); // dejar que el SIM808 termine su boot y sus dots

  // Limpiar todo lo que el SIM808 envio durante el boot
  while (sim808.available()) sim808.read();

  Serial.println("Encendiendo GPS (AT+CGNSPWR=1)...");
  sim808.println("AT+CGNSPWR=1");
  delay(2000);
  while (sim808.available()) sim808.read(); // descartar respuesta

  configureGPRS();

  Serial.println("Inicializando DFPlayer Mini...");
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (dfPlayer.begin(dfSerial)) {
    dfPlayer.volume(volumenActual);
    Serial.println("DFPlayer OK.");
  } else {
    Serial.println("[WARN] DFPlayer no respondio. Revisar cableado/SD.");
  }

  Serial.println("Inicializando link con ESP32 de la pantalla...");
  pantallaLink.begin(9600);
  delay(100);
  mostrarTexto(textoDefault);

  ultimaRevisionConfigMs = millis();

  Serial.println("\nSistema listo. Esperando señal GPS...");
  Serial.println("-----------------------------------------------------");
}

void loop() {
  sim808.println("AT+CGNSINF");
  delay(100);

  String response = "";
  long startTime = millis();
  while (millis() - startTime < 300) {
    while (sim808.available()) {
      char c = sim808.read();
      response += c;
    }
  }

  if (response.indexOf("+CGNSINF:") != -1) {
    String fixStatus = getValue(response, ',', 1);

    if (fixStatus == "1") {
      String latitude  = getValue(response, ',', 3);
      String longitude = getValue(response, ',', 4);
      String hdop      = getValue(response, ',', 10);
      latitude.trim();
      longitude.trim();
      hdop.trim();

      Serial.println("\n>>> GPS FIX OBTENIDO <<<");
      Serial.print("Lat: "); Serial.print(latitude);
      Serial.print(" Lon: "); Serial.print(longitude);
      Serial.print(" HDOP: "); Serial.println(hdop);

      if (hdop.toFloat() < 5.0 && latitude.length() > 5) {
        double latVal = atof(latitude.c_str());
        double lonVal = atof(longitude.c_str());
        revisarParadas(latVal, lonVal);
        sendLocation(latitude, longitude, hdop);
      } else {
        Serial.println("Precision baja o datos invalidos. Ignorando envio.");
      }

    } else {
      String satView = getValue(response, ',', 14);
      Serial.print("\n... Buscando satelites ... Visibles: ");
      Serial.println(satView);
    }
  } else {
    if (response.length() > 5) {
      Serial.print("R (Raw): ");
      Serial.println(response);
    }
  }

  if (millis() - ultimaRevisionConfigMs > INTERVALO_REVISION_CONFIG_MS) {
    revisarConfigRemota();
    ultimaRevisionConfigMs = millis();
  }

  delay(intervaloSubidaMs);
}

// -------------------------------------------------------
// Cerrar sesion HTTP de forma limpia
// -------------------------------------------------------
void httpClose() {
  String r = sendAT("AT+HTTPTERM", 3000);
  Serial.print("[HTTP] HTTPTERM: "); Serial.println(r);
  delay(1500); // pausa para que el modulo libere la sesion
}

// -------------------------------------------------------
// Intentar AT+HTTPINIT con un reintento si falla
// -------------------------------------------------------
bool httpInit() {
  String r = sendAT("AT+HTTPINIT", 3000);
  Serial.print("[HTTP] HTTPINIT: "); Serial.println(r);
  if (r.indexOf("OK") != -1) return true;

  // Fallo: forzar doble HTTPTERM con delay largo y reintentar
  Serial.println("[HTTP] HTTPINIT fallo. Forzando HTTPTERM doble...");
  sendAT("AT+HTTPTERM", 3000);
  delay(2000);
  sendAT("AT+HTTPTERM", 3000);
  delay(2000);

  r = sendAT("AT+HTTPINIT", 3000);
  Serial.print("[HTTP] HTTPINIT reintento: "); Serial.println(r);
  return r.indexOf("OK") != -1;
}

// -------------------------------------------------------
// Ejecutar un HTTP GET sobre la sesion del SIM808.
// Si se pasa outBody, ahi se copia el texto crudo de AT+HTTPREAD
// (usado por revisarConfigRemota() para parsear el JSON de ThingSpeak).
// Retorna true si el servidor respondio 200.
// -------------------------------------------------------
bool doHTTPGet(const String& url, String* outBody = nullptr) {
  // Cerrar sesion previa antes de empezar
  httpClose();

  // Iniciar HTTP (con reintento interno)
  if (!httpInit()) {
    Serial.println("[HTTP] HTTPINIT fallo definitivo. Abortando.");
    return false;
  }

  String r = sendAT("AT+HTTPPARA=\"CID\",1", 2000);
  Serial.print("[HTTP] CID: "); Serial.println(r);
  if (r.indexOf("ERROR") != -1) {
    Serial.println("[HTTP] HTTPPARA CID fallo.");
    httpClose();
    return false;
  }

  r = sendAT("AT+HTTPPARA=\"URL\",\"" + url + "\"", 3000);
  Serial.print("[HTTP] URL set: "); Serial.println(r);
  if (r.indexOf("ERROR") != -1) {
    Serial.println("[HTTP] HTTPPARA URL fallo.");
    httpClose();
    return false;
  }

  // Ejecutar GET
  while (sim808.available()) sim808.read(); // limpiar buffer antes de esperar URC
  sim808.println("AT+HTTPACTION=0");
  Serial.println("[HTTP] HTTPACTION enviado, esperando respuesta (60s max)...");

  // Esperar "+HTTPACTION: 0,STATUS,BYTES" hasta 60s
  String actionResp = "";
  long waitStart = millis();
  bool received = false;
  while (millis() - waitStart < 60000) {
    while (sim808.available()) {
      char c = sim808.read();
      Serial.write(c);
      actionResp += c;
    }
    delay(1); // alimentar watchdog del ESP32
    if (actionResp.indexOf("+HTTPACTION:") != -1) {
      received = true;
      delay(100);
      while (sim808.available()) {
        char c = sim808.read();
        Serial.write(c);
        actionResp += c;
      }
      break;
    }
  }

  if (!received) {
    Serial.println("[WARN] +HTTPACTION no recibido en 60s.");
    httpClose();
    return false;
  }

  // Parsear codigo de estado y bytes recibidos
  int idx = actionResp.indexOf("+HTTPACTION:");
  bool success = false;
  if (idx != -1) {
    String part = actionResp.substring(idx);
    int c1 = part.indexOf(',');
    int c2 = c1 != -1 ? part.indexOf(',', c1 + 1) : -1;
    if (c2 != -1) {
      String statusCode = part.substring(c1 + 1, c2);
      statusCode.trim();
      Serial.print("[HTTP] Status: "); Serial.println(statusCode);

      int lineEnd = part.indexOf('\n', c2 + 1);
      String bytesStr = part.substring(c2 + 1, lineEnd != -1 ? lineEnd : part.length());
      bytesStr.trim();
      unsigned long rxBytes = (unsigned long)bytesStr.toInt();
      totalBytesReceived += rxBytes;
      totalRequests++;

      if (statusCode == "200") {
        success = true;
      } else {
        Serial.print("[WARN] Error HTTP "); Serial.println(statusCode);
      }
    }
  }

  // Leer respuesta del servidor
  Serial.println("\nRespuesta del servidor:");
  String readResp = sendAT("AT+HTTPREAD", 3000);
  Serial.println(readResp);
  if (outBody != nullptr) *outBody = readResp;

  httpClose();
  return success;
}

void sendLocation(String lat, String lon, String hdop) {
  Serial.println("Enviando datos a la nube (ThingSpeak)...");

  // Verificar GPRS; reconectar si es necesario
  if (!ensureGPRS()) {
    Serial.println("[ERROR] Sin conexion GPRS. Saltando envio.");
    return;
  }

  unsigned long uptimeSec = (millis() - dataStartMs) / 1000;
  String url = "http://api.thingspeak.com/update?api_key=" + tsApiKey +
               "&field1=" + lat + "&field2=" + lon + "&field3=" + hdop +
               "&field4=" + String(totalBytesSent) +
               "&field5=" + String(totalBytesReceived) +
               "&field6=" + String(totalRequests) +
               "&field7=" + String(uptimeSec);

  // Estimar bytes enviados (linea GET + cabeceras fijas ~150 B)
  totalBytesSent += url.length() + 150;

  bool ok = doHTTPGet(url);

  if (!ok) {
    // Si fallo, reconectar GPRS y reintentar UNA vez
    Serial.println("[RETRY] Reconectando GPRS y reintentando...");
    configureGPRS();
    uptimeSec = (millis() - dataStartMs) / 1000;
    url = "http://api.thingspeak.com/update?api_key=" + tsApiKey +
          "&field1=" + lat + "&field2=" + lon + "&field3=" + hdop +
          "&field4=" + String(totalBytesSent) +
          "&field5=" + String(totalBytesReceived) +
          "&field6=" + String(totalRequests) +
          "&field7=" + String(uptimeSec);
    totalBytesSent += url.length() + 150;
    doHTTPGet(url);
  }

  // Resumen de consumo
  uptimeSec = (millis() - dataStartMs) / 1000;
  unsigned long totalBytes = totalBytesSent + totalBytesReceived;
  Serial.println("\n╔══ CONSUMO DE DATOS SIM808 ══╗");
  Serial.print(  "  Envios realizados : "); Serial.println(totalRequests);
  Serial.print(  "  Bytes enviados    : "); Serial.print(totalBytesSent);    Serial.println(" B");
  Serial.print(  "  Bytes recibidos   : "); Serial.print(totalBytesReceived); Serial.println(" B");
  Serial.print(  "  Total             : ");
  if (totalBytes < 1024) {
    Serial.print(totalBytes); Serial.println(" B");
  } else {
    Serial.print(totalBytes / 1024.0, 2); Serial.println(" KB");
  }
  unsigned long h = uptimeSec / 3600;
  unsigned long m = (uptimeSec % 3600) / 60;
  unsigned long s = uptimeSec % 60;
  Serial.print(  "  Tiempo activo     : ");
  if (h < 10) Serial.print("0"); Serial.print(h); Serial.print(":");
  if (m < 10) Serial.print("0"); Serial.print(m); Serial.print(":");
  if (s < 10) Serial.print("0"); Serial.println(s);
  if (uptimeSec > 0) {
    float rateKBh = (totalBytes / 1024.0) / (uptimeSec / 3600.0);
    Serial.print("  Tasa              : "); Serial.print(rateKBh, 1); Serial.println(" KB/h");
  }
  Serial.println("╚═════════════════════════════╝");
  Serial.println("--- Fin intento envio ---");
}

// -------------------------------------------------------
// Consultar el canal de configuracion remota de ThingSpeak (cada 60s) y
// aplicar en RAM los cambios de volumen / texto / radio de parada /
// intervalo de subida, si el entry_id cambio desde la ultima lectura.
// No hace nada mientras configChannelId/configReadApiKey no esten
// completados (ver constantes arriba).
// -------------------------------------------------------
void revisarConfigRemota() {
  if (configChannelId == "COMPLETAR_CHANNEL_ID" || configReadApiKey == "COMPLETAR_READ_API_KEY") {
    return; // canal de config todavia no creado
  }

  if (!ensureGPRS()) {
    Serial.println("[CONFIG] Sin conexion GPRS. Se reintenta en el proximo ciclo.");
    return;
  }

  String url = "http://api.thingspeak.com/channels/" + configChannelId +
               "/feeds/last.json?api_key=" + configReadApiKey;

  String body;
  if (!doHTTPGet(url, &body)) {
    Serial.println("[CONFIG] No se pudo leer el canal de configuracion.");
    return;
  }

  int start = body.indexOf('{');
  int end   = body.lastIndexOf('}');
  if (start == -1 || end == -1 || end <= start) {
    Serial.println("[CONFIG] Respuesta sin JSON valido.");
    return;
  }
  String json = body.substring(start, end + 1);

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.print("[CONFIG] Error parseando JSON: "); Serial.println(err.c_str());
    return;
  }

  long entryId = doc["entry_id"] | -1;
  if (entryId < 0 || entryId == ultimoEntryIdConfig) {
    Serial.println("[CONFIG] Sin cambios.");
    return;
  }
  ultimoEntryIdConfig = entryId;

  // field1=volumen, field2=texto default, field3=radio deteccion (m), field4=intervalo subida (s)
  int nuevoVolumen = doc["field1"] | volumenActual;
  const char* nuevoTexto = doc["field2"] | textoDefault.c_str();
  int nuevoRadio = doc["field3"] | radioDeteccionParada;
  long nuevoIntervaloSeg = doc["field4"] | (long)(intervaloSubidaMs / 1000);

  volumenActual = constrain(nuevoVolumen, 0, 30);
  dfPlayer.volume(volumenActual);

  textoDefault = String(nuevoTexto);
  if (!mostrandoTextoParada) mostrarTexto(textoDefault);

  radioDeteccionParada = nuevoRadio;
  for (int i = 0; i < NUM_PARADAS; i++) paradas[i].radioDeteccion = radioDeteccionParada;

  if (nuevoIntervaloSeg > 0) intervaloSubidaMs = (unsigned long)nuevoIntervaloSeg * 1000UL;

  Serial.println("[CONFIG] Configuracion remota aplicada:");
  Serial.print("  volumen="); Serial.print(volumenActual);
  Serial.print(" texto=\""); Serial.print(textoDefault);
  Serial.print("\" radio="); Serial.print(radioDeteccionParada);
  Serial.print("m intervalo="); Serial.print(intervaloSubidaMs / 1000);
  Serial.println("s");
}

// Funcion auxiliar para separar por comas
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
