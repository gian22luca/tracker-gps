#include <HardwareSerial.h>

// Configuración de pines
// ESP32 GPIO 16 (RX2) <--- SIM808 TX
// ESP32 GPIO 17 (TX2) ---> SIM808 RX
#define SIM808_RX_PIN 16
#define SIM808_TX_PIN 17

HardwareSerial sim808(2); // UART2

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

// ThingSpeak
String tsApiKey  = "YZG43OQW28F0SML7";

// --- CONTADOR DE CONSUMO DE DATOS SIM808 ---
unsigned long totalBytesSent     = 0;
unsigned long totalBytesReceived = 0;
unsigned long totalRequests      = 0;
unsigned long dataStartMs        = 0;

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

void setup() {
  Serial.begin(115200);
  // Esperar Serial con timeout (evita bloqueo sin USB)
  unsigned long sw = millis();
  while (!Serial && millis() - sw < 3000);
  delay(500);

  dataStartMs = millis();
  Serial.println("--- GPS CLOUD TRACKER SIM808 ---");
  Serial.println("Inicializando...");

  sim808.begin(9600, SERIAL_8N1, SIM808_RX_PIN, SIM808_TX_PIN);
  delay(2000); // dejar que el SIM808 termine su boot y sus dots

  // Limpiar todo lo que el SIM808 envio durante el boot
  while (sim808.available()) sim808.read();

  Serial.println("Encendiendo GPS (AT+CGNSPWR=1)...");
  sim808.println("AT+CGNSPWR=1");
  delay(2000);
  while (sim808.available()) sim808.read(); // descartar respuesta

  configureGPRS();

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

  delay(15000);
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
// Enviar ubicacion a dweet.cc
// Retorna true si el servidor respondio 200
// -------------------------------------------------------
bool doHTTPGet(const String& url) {
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
  sendAT("AT+HTTPREAD", 2000);

  httpClose();
  return success;
}

void sendLocation(String lat, String lon, String hdop) {
  Serial.println("Enviando datos a la nube (dweet.cc)...");

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
