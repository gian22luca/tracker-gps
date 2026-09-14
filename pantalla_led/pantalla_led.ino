#include <HardwareSerial.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// =========================================================================
// Librerias necesarias (Arduino IDE > Administrador de bibliotecas):
//   - ESP32 HUB75 LED MATRIX PANEL DMA Display  (mrfaptastic)
//
// Esta placa es un segundo ESP32 dedicado UNICAMENTE a manejar la pantalla
// HUB75. No toca GPS/GPRS/SIM808/DFPlayer: eso vive en el otro ESP32
// (tracker_gps/tracker_gps.ino), que es el que decide que mostrar y
// cuando. Esta placa solo recibe el texto por UART y lo dibuja.
//
// Protocolo (lineas de texto terminadas en '\n'):
//   TXT:<texto a mostrar>
//
// Conexion entre placas: GND en comun + 1 cable desde LINK_TX_PIN del
// tracker hasta LINK_RX_PIN de esta placa.
// =========================================================================

// HUB75 (2 paneles 64x32 encadenados = 128x32)
#define HUB75_R1_PIN 25
#define HUB75_G1_PIN 26
#define HUB75_B1_PIN 27
#define HUB75_R2_PIN 32
#define HUB75_G2_PIN 33
#define HUB75_B2_PIN 4
#define HUB75_A_PIN  5
#define HUB75_B_PIN  18
#define HUB75_C_PIN  19
#define HUB75_D_PIN  21
#define HUB75_OE_PIN 22
#define HUB75_LAT_PIN 23
#define HUB75_CLK_PIN 16

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 2 // 2 paneles de 64x32 -> 128x32 total

// Link serial hacia el ESP32 tracker. UART de hardware libre (esta placa
// no usa SIM808 ni DFPlayer), pines fuera de la lista del HUB75 de arriba.
#define LINK_RX_PIN 13
#define LINK_TX_PIN 14 // sin uso real por ahora, queda para un futuro ACK

HardwareSerial trackerLink(2); // UART2

MatrixPanel_I2S_DMA *dma_display = nullptr;
String textoActual = "LINEA 102 - VIXEL";
String lineaEntrante = "";

// -------------------------------------------------------
// Inicializar pantalla LED HUB75
// -------------------------------------------------------
void inicializarPantalla() {
  HUB75_I2S_CFG::i2s_pins pines = {
    HUB75_R1_PIN, HUB75_G1_PIN, HUB75_B1_PIN,
    HUB75_R2_PIN, HUB75_G2_PIN, HUB75_B2_PIN,
    HUB75_A_PIN, HUB75_B_PIN, HUB75_C_PIN, HUB75_D_PIN,
    -1, // pin E: no se usa en paneles 1/16 scan (64x32)
    HUB75_LAT_PIN, HUB75_OE_PIN, HUB75_CLK_PIN
  };

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pines);
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);
}

// -------------------------------------------------------
// Mostrar un texto estatico en la pantalla LED
// -------------------------------------------------------
void mostrarTexto(const String& texto) {
  if (dma_display == nullptr) return;
  dma_display->clearScreen();
  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->setCursor(0, 8);
  dma_display->print(texto);
}

// -------------------------------------------------------
// Interpretar una linea recibida del tracker
// -------------------------------------------------------
void procesarLinea(const String& linea) {
  if (linea.startsWith("TXT:")) {
    textoActual = linea.substring(4);
    Serial.print("[LINK] Nuevo texto: "); Serial.println(textoActual);
    mostrarTexto(textoActual);
  } else if (linea.length() > 0) {
    Serial.print("[LINK] Comando desconocido: "); Serial.println(linea);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("--- ESP32 PANTALLA LED (esclava del tracker) ---");

  inicializarPantalla();
  mostrarTexto(textoActual);

  trackerLink.begin(9600, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  Serial.println("Esperando comandos del tracker...");
}

void loop() {
  while (trackerLink.available()) {
    char c = trackerLink.read();
    if (c == '\n') {
      procesarLinea(lineaEntrante);
      lineaEntrante = "";
    } else if (c != '\r') {
      lineaEntrante += c;
    }
  }
}
