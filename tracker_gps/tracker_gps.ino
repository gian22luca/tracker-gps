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
String apn = "igprs.claro.com.ar";
String user = "clarogprs";
String pass = "clarogprs999";

// CLARO CHILE (Descomentar si es Chile)
// String apn = "bam.clarochile.cl";
// String user = "clarochile";
// String pass = "clarochile";

// CLARO URUGUAY (Descomentar si es Uruguay)
// String apn = "igprs.claro.com.uy";
// String user = "f";
// String pass = "f";

// ID UNICO para tu tracker (puedes cambiarlo si quieres)
String trackerID = "mi-gps-tracker-unico-12345";

void setup() {
  // Inicializar puerto serial de depuración (USB)
  Serial.begin(115200);
  while (!Serial);
  delay(1000);

  Serial.println("--- GPS CLOUD TRACKER SIM808 ---");
  Serial.println("Inicializando...");

  // Inicializar puerto serial del SIM808 a 9600 baudios
  sim808.begin(9600, SERIAL_8N1, SIM808_RX_PIN, SIM808_TX_PIN);
  delay(1000);

  // Enviar comando para encender GPS
  Serial.println("Enviando comando de ENCENDIDO GPS (AT+CGNSPWR=1)...");
  sim808.println("AT+CGNSPWR=1");
  delay(2000); 
  
  // Configurar GPRS
  configureGPRS();
  
  Serial.println("\nSistema listo. Esperando señal GPS para transmitir...");
  Serial.println("-----------------------------------------------------");
}

void loop() {
  // Pedir informacion GPS
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
    String fixStatus = getValue(response, ',', 1); // Fix status is the second value (index 1)
    
    if (fixStatus == "1") {
      String latitude = getValue(response, ',', 3);
      String longitude = getValue(response, ',', 4);
      String hdop = getValue(response, ',', 10);
      
      // Limpiar posibles comillas o espacios extra
      latitude.trim();
      longitude.trim();
      hdop.trim();

      Serial.println("\n>>> GPS FIX OBTENIDO <<<");
      Serial.print("Lat: "); Serial.print(latitude);
      Serial.print(" Lon: "); Serial.print(longitude);
      Serial.print(" HDOP: "); Serial.println(hdop);
      
      // ENVIAR A LA NUBE (Solo si precision es decente, HDOP < 5.0)
      if (hdop.toFloat() < 5.0 && latitude.length() > 5) {
        sendLocation(latitude, longitude, hdop);
      } else {
        Serial.println("Precision baja o datos invalidos. Ignorando envio.");
      }
      
    } else {
      String satView = getValue(response, ',', 14);
      Serial.print("\n... Buscando satelites ... Satelites visibles: ");
      Serial.println(satView);
    }
  } else {
      // Si no es respuesta de GPS, puede ser un eco o nada
      if (response.length() > 5) {
         Serial.print("R (Raw): ");
         Serial.println(response);
      }
  }

  // Esperar 15 segundos antes de la siguiente lectura/envio
  delay(15000);
}

void configureGPRS() {
  Serial.println("--- Configurando GPRS ---");
  // Cerrar bearer primero para asegurar
  sim808.println("AT+SAPBR=0,1"); 
  delay(2000);
  while(sim808.available()) sim808.read();
  
  sim808.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  delay(1000);
  sim808.println("AT+SAPBR=3,1,\"APN\",\"" + apn + "\"");
  delay(1000);
  sim808.println("AT+SAPBR=3,1,\"USER\",\"" + user + "\"");
  delay(1000);
  sim808.println("AT+SAPBR=3,1,\"PWD\",\"" + pass + "\"");
  delay(1000);
  
  Serial.println("Intentando conectar (AT+SAPBR=1,1)...");
  sim808.println("AT+SAPBR=1,1");
  delay(4000); // Dar mas tiempo
  while(sim808.available()) sim808.read(); // Limpiar respuesta
  Serial.println("\n--- Fin Config GPRS ---");
}

void sendLocation(String lat, String lon, String hdop) {
  Serial.println("Enviando datos a la nube (dweet.cc)...");
  
  // 1. Verificar GPRS antes de enviar
  // Limpiar buffer
  while(sim808.available()) sim808.read();
  
  sim808.println("AT+SAPBR=2,1");
  delay(1000); 
  
  bool gprsConnected = false;
  long checkStart = millis();
  while(millis() - checkStart < 2000) {
    while(sim808.available()) {
        String line = sim808.readStringUntil('\n');
        if (line.indexOf("+SAPBR: 1,1") != -1) {
          gprsConnected = true;
        }
    }
  }
  
  if (!gprsConnected) {
    Serial.println("GPRS desconectado. Intentando reconectar...");
    configureGPRS();
  }

  // Limpiar sesion anterior
  sim808.println("AT+HTTPTERM");
  delay(500);
  while(sim808.available()) sim808.read(); 
  
  // Iniciar HTTP
  sim808.println("AT+HTTPINIT");
  delay(1000);
  // Leemos si dio error o OK, pero en realidad no importa mucho
  // Si dio error es porque ya estaba init o fallo algo grave
  // Proseguimos igual a ver si PARA funciona
  while(sim808.available()) Serial.write(sim808.read());
  
  sim808.println("AT+HTTPPARA=\"CID\",1");
  delay(1000);
  while(sim808.available()) Serial.write(sim808.read());
  
  String url = "http://dweet.cc/dweet/for/" + trackerID + "?lat=" + lat + "&lon=" + lon + "&hdop=" + hdop;
  sim808.println("AT+HTTPPARA=\"URL\",\"" + url + "\"");
  delay(2000);
  while(sim808.available()) Serial.write(sim808.read());
  
  sim808.println("AT+HTTPACTION=0");
  
  long waitStart = millis();
  while (millis() - waitStart < 10000) {
    while(sim808.available()) {
      Serial.write(sim808.read()); 
    }
  }
  
  Serial.println("\nRespuesta del servidor:");
  sim808.println("AT+HTTPREAD");
  delay(2000);
  while(sim808.available()) {
    Serial.write(sim808.read());
  }
  sim808.println("AT+HTTPTERM"); // Cerrar al terminar siempre
  Serial.println("\n--- Fin intento envio ---");
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
