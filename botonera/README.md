# Monitoreo de botonera de relés (diseñado, sin implementar)

Este documento mapea el diseño de un futuro cuarto nodo del proyecto: monitoreo
no invasivo de la botonera de relés existente (proyecto aparte del usuario,
ya en uso). **No hay firmware escrito todavía** — esto es la referencia para
cuando se decida implementarlo.

## Objetivo

Reportar, junto con el resto de la telemetría del colectivo:
- Tensión de los rieles de control (5V) y del sistema (24V nominal / 28V real).
- Si cada relé fue **comandado** a encender.
- Si cada relé **realmente conmutó** (verificación del lado de carga, no solo
  del comando — detecta bobina quemada o contacto pegado).
- Estado de cada pulsador físico.

## Hardware existente (del proyecto de botonera, ya armado por el usuario)

- **Placa de relés de 16 canales**, módulos `SRD-05VDC-SL-C`, control **5V,
  activo en bajo** (línea en 0V = relé encendido). Entradas de control ya
  opto-aisladas de fábrica en la propia placa.
- Los relés manejan cargas a **24V nominal (28V real)**: pantallas LED,
  calefacción, luces LED interiores.
- **8 pulsadores** controlan **16 relés físicos, 9 en uso**: 7 pulsadores
  manejan 1 relé cada uno, 1 pulsador maneja 2 relés en paralelo. Los 7
  relés restantes quedan de backup, sin usar.
- Panel de pulsadores con su propio circuito (ver esquemático
  `circuito_2_bond1.kicad_sch` del usuario, no versionado en este repo):
  - Redes resistivas RN1/RN2 (10k) + capacitores de 100nF por línea
    (antirrebote) en la señal de cada pulsador.
  - LEDs indicadores en los pulsadores alimentados a 28V real a través de
    redes RN3/RN5 (2.2k) como limitadoras de corriente.
  - Conector IDC de 20 pines (`J2`, `Conn_02x10_Odd_Even`) como bus entre el
    panel de pulsadores y la placa de relés.
- Se descartó a propósito la variante de placa de relés de 16 canales con
  ESP8266 integrado: ese módulo espera controlar los relés él mismo por
  WiFi, lo que competiría con el control físico de los pulsadores en vez de
  convivir con él.

## Principio de diseño: tap en paralelo, nunca en serie

El monitoreo no debe interponerse en el camino pulsador → relé → carga. Todas
las lecturas se toman **en paralelo** sobre señales ya existentes, sin cortar
ni redirigir ningún cable del circuito actual.

## Diseño de monitoreo propuesto

Un nodo ESP32 dedicado, con 3 expansores de I/O por I2C (`MCP23017`, 16
canales c/u, evita usar casi todos los GPIO del ESP32 para esto) más algunas
lecturas analógicas:

| Chip / entrada | Señal | Cómo se lee |
|---|---|---|
| MCP23017 #1 (0x20) | 16 líneas de comando de la placa de relés | Tap directo en paralelo sobre el header de control (ya es 5V lógico, ya aislado por la propia placa de relés). Activo en bajo: bit en 0 = relé comandado ON. |
| MCP23017 #2 (0x21) | 8 pulsadores | Tap en paralelo después de la red RN1/RN2 + capacitor de 100nF existente (señal ya limpia). |
| MCP23017 #3 (0x22) | 9 verificaciones de conmutación real | Un optoacoplador (PC817 + resistencia limitadora ~2.2k, mismo valor que ya usa el circuito de LEDs a 28V) por cada relé en uso, en el lado de **carga/salida** del relé — este es el único punto que sí necesita aislamiento propio, por ser 28V real en vez de 5V lógico. 7 canales libres por si se cablean relés de backup a futuro. |
| ADC del ESP32 (2 canales) | Tensión riel 5V y tensión riel 28V | Divisor resistivo simple en cada uno, a menos de 3.3V. |

**Detección de falla:** comparar por cada relé el bit de "comandado" (chip 1)
contra el bit de "conmutado" (chip 3). Un desacople sostenido (varias
lecturas seguidas, para no confundir con el delay mecánico normal de
conmutación) indica relé fallado.

## Pendiente de resolver antes de implementar

1. **Esquema de sondeo en el bus RS-485.** Hasta ahora el bus solo tiene
   tráfico en un sentido (tracker → pantalla). Este nodo necesita *hablar*
   hacia el tracker, y al ser un bus compartido, dos nodos transmitiendo a
   la vez se pisan. Hace falta que el tracker sondee a la botonera
   periódicamente (algo como mandar `POLL:BOTONERA\n` y esperar una
   respuesta `BOT:...`) en vez de dejar que transmita libremente.
2. **A dónde van los datos en la nube.** El cliente MQTT del tracker todavía
   no está implementado (quedó en diseño). Hasta que exista, los datos de
   este nodo no tienen un destino real en la nube — la opción intermedia es
   mostrarlos por el puerto serie del tracker como primer hito de prueba.

## Estado

**Diseñado, no implementado.** Nada de este documento afecta al firmware
actual (`tracker_gps/`, `pantalla_led/`).
