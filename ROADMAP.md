# Roadmap — Tracker GPS Línea 102 (Vixel)

Proyecto dividido en fases incrementales, cada una con su hardware y su
software. Cada fase se apoya en la anterior sin romperla.

## Fase 1 — Tracker GPS + Paradas inteligentes
**Estado: implementado**, con 2 pendientes reales antes de poder flashear en
un colectivo.

- **Hardware:** ESP32 + SIM808 (GPS/GPRS/GSM) + DFPlayer Mini + amplificador
  PAM8610 + parlantes.
- **Software:** [`tracker_gps/tracker_gps.ino`](tracker_gps/tracker_gps.ino)
  — encendido automático por PWRKEY, telemetría GPS a ThingSpeak, detección
  de proximidad a paradas (Haversine), anuncio de audio por DFPlayer,
  configuración remota (volumen/texto/radio/intervalo) vía un segundo canal
  de ThingSpeak.
- **Pendiente real:** coordenadas verdaderas de las 4 paradas del Ramal A
  (siguen en placeholder), y crear el canal de configuración remota en
  ThingSpeak (`configChannelId` / `configReadApiKey` sin completar).

## Fase 2 — Pantalla LED con anuncio de parada
**Estado: implementado.**

- **Hardware:** segundo ESP32 dedicado + 2 paneles HUB75 64×32 (128×32) +
  fuente 5V de alta corriente.
- **Software:** [`pantalla_led/pantalla_led.ino`](pantalla_led/pantalla_led.ino)
  — recibe el texto a mostrar por un link serial simple (`TXT:<texto>`) y lo
  dibuja; el tracker (Fase 1) le manda el texto por defecto y el de cada
  parada detectada.
- **Pendiente técnico (no bloquea probar en banco, sí antes de instalar en
  un colectivo real):** el link entre tracker y pantalla sigue siendo serial
  directo (TTL). La decisión ya tomada es migrarlo a **RS-485** (con
  MAX485 en cada punta) porque un colectivo tiene metros de distancia entre
  placas — pero ese cambio todavía no está en el firmware, solo en el plan
  de hardware.

## Fase 3 — WiFi (conectividad unificada)
**Estado: diseñado, sin implementar.**

- **Hardware:** router 4G/LTE para vehículos con puerto LAN (WiFi para
  pasajeros) + módulo Ethernet W5500 en la placa del tracker.
- **Software pendiente:** en `tracker_gps.ino`, agregar la capa de red por
  Ethernet (W5500) como salida a internet — a definir si reemplaza al
  GPRS del SIM808 para datos (dejando el SIM808 solo para GPS) o convive
  como respaldo.
- **Nada implementado todavía**, ni el código de red ni la migración del
  cliente HTTP de ThingSpeak a esa capa.

## Fase 4 — Display al conductor
**Estado: diseñado, sin implementar.**

- **Hardware:** tercer ESP32 (consola) + pantalla Nextion o LCD+botón +
  buzzer + MAX485 (nodo del mismo bus RS-485 que la pantalla).
- **Software pendiente:**
  - Firmware de la consola (no existe todavía, iría en `consola_chofer/`).
  - Cliente MQTT en el tracker (TinyGSM + PubSubClient) — hoy el tracker
    solo sabe hablar con ThingSpeak vía AT+HTTP, no existe ningún cliente
    MQTT en el código.
  - Backend de mensajería (Vercel + Supabase) para que la central pueda
    mandar avisos y ver los "recibido" del chofer.
- **Nada implementado todavía.**

## Fase 5 — Botonera de relés (monitoreo)
**Estado: documentado, implementación pospuesta a pedido explícito.**

- **Software:** [`botonera/README.md`](botonera/README.md) — diseño
  completo (3× MCP23017, optoacopladores, divisores de tensión, mapeo de
  pulsadores/relés), sin firmware.
- **Pendiente antes de poder implementar:** esquema de sondeo en el bus
  RS-485 (recién con la Fase 4 hay más de un nodo hablando) y que exista el
  cliente MQTT del tracker (Fase 4) para tener adónde mandar los datos.

## Dependencias entre fases

```
Fase 1 (tracker) ──┬── Fase 2 (pantalla)         [implementadas]
                    ├── Fase 3 (wifi/conectividad) [siguiente candidata]
                    └── Fase 4 (consola chofer) ── requiere Fase 3 (o SIM808)
                                                 └── Fase 5 (botonera) requiere Fase 4
```

La Fase 5 depende de que la Fase 4 exista primero (necesita el cliente MQTT
y el esquema de sondeo del bus que introduce la consola), por eso quedó
pospuesta.
