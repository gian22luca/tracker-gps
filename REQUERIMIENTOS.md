# Requerimientos Funcionales y Alcance del Proyecto

**Sistema de rastreo y cartelería inteligente para colectivos — Línea 102 (Vixel)**

## 1. Resumen ejecutivo

Vixel está desarrollando un sistema a bordo para colectivos urbanos que rastrea
la posición del vehículo en tiempo real, anuncia automáticamente por voz y en
un cartel la próxima parada, y está pensado para crecer hacia una plataforma
más amplia de comunicación y monitoreo para empresas de transporte.

## 2. Alcance actual (piloto)

- Línea 102, Ramal A.
- Prueba en un colectivo real, con el hardware de rastreo y anuncio de
  paradas ya armado.
- Objetivo del piloto: validar en condiciones reales el rastreo de posición,
  el anuncio automático de paradas, y que el volumen del audio alcance para
  escucharse dentro del colectivo en movimiento.

## 3. Actores del sistema

- **Pasajero**: recibe la información de la próxima parada (audio y cartel),
  y a futuro tendrá acceso a conexión a internet a bordo.
- **Chofer**: a futuro, recibe avisos operativos de la central en una
  pantalla y puede confirmar que los leyó.
- **Central / despachante**: monitorea la ubicación de cada colectivo y,
  a futuro, puede enviarle avisos al chofer.
- **Administrador de flota (Vixel)**: ajusta parámetros de funcionamiento de
  forma remota, sin necesidad de intervenir cada unidad físicamente.

## 4. Requerimientos funcionales por etapa

### Etapa 1 — Rastreo y anuncio inteligente de paradas
*(desarrollada, en prueba sobre un colectivo real)*

- Reportar la posición del colectivo en tiempo real a una plataforma central.
- Reconocer automáticamente cuándo el colectivo se aproxima a una parada
  configurada.
- Anunciar por voz la parada próxima, con volumen suficiente para
  escucharse dentro del colectivo en movimiento.
- Permitir ajustar el volumen y otros parámetros de forma remota, sin
  reprogramar el dispositivo.
- Registrar métricas básicas de funcionamiento (consumo de datos, tiempo
  activo) para control operativo.

### Etapa 2 — Cartel visual de parada
*(desarrollada)*

- Mostrar en un cartel visible el nombre de la próxima parada al momento
  del anuncio.
- Volver a mostrar un texto por defecto (por ejemplo, el número de línea)
  cuando no hay un anuncio activo.

### Etapa 3 — Conectividad a bordo
*(diseñada, pendiente de desarrollo)*

- Ofrecer conexión a internet inalámbrica a los pasajeros dentro del
  colectivo.
- Aprovechar esa misma conexión para el sistema de rastreo, evitando
  depender de más de una línea de datos por vehículo.
- Mantener separada la red de pasajeros de la red del sistema de rastreo,
  por seguridad y estabilidad.

### Etapa 4 — Comunicación con el chofer
*(diseñada, pendiente de desarrollo)*

- Permitir que la central envíe avisos breves (por ejemplo, desvíos o
  novedades de tránsito) que el chofer vea en una pantalla a bordo.
- Permitir que el chofer confirme la recepción de un aviso con una sola
  acción simple.
- Alertar sonoramente al chofer cuando llega un aviso nuevo.
- Darle a la central un panel para ver el historial de avisos enviados y
  las confirmaciones recibidas, con acceso por usuario.

### Etapa 5 — Monitoreo de controles del colectivo
*(documentada, desarrollo pospuesto a propósito)*

- Informar el estado de los controles eléctricos del colectivo — por
  ejemplo, si un control fue accionado y si efectivamente respondió.
- Alertar cuando un control no responde como se esperaba, para facilitar
  el mantenimiento preventivo.
- Esta etapa depende de que la Etapa 4 esté implementada, por eso su
  desarrollo queda pospuesto hasta entonces.

## 5. Requerimientos generales del sistema

- **Autonomía**: debe encender y empezar a funcionar solo al arrancar el
  colectivo, sin que nadie tenga que intervenir manualmente.
- **Disponibilidad**: debe seguir funcionando de forma confiable durante
  toda la jornada del colectivo.
- **Escalabilidad**: el diseño debe permitir sumar más colectivos, más
  líneas y más empresas de transporte clientas sin rehacer el sistema
  desde cero.
- **Facilidad de mantenimiento**: los parámetros de funcionamiento
  (volumen, textos, tiempos) deben poder ajustarse de forma remota.
- **Costo por unidad**: las decisiones de diseño deben tener en cuenta el
  costo al momento de escalar a una flota completa.

## 6. Fuera de alcance (por ahora)

- Cobro de pasajes o integración con sistemas de boletería.
- Videovigilancia o cámaras a bordo.
- Publicidad o contenido multimedia en el cartel, más allá del nombre de
  la parada.
- Cualquier funcionalidad no descripta en este documento.

## 7. Estado actual

| Etapa | Estado |
|---|---|
| 1 — Rastreo y anuncio de paradas | Desarrollada, en prueba real |
| 2 — Cartel visual | Desarrollada |
| 3 — Conectividad a bordo | Diseñada, pendiente |
| 4 — Comunicación con el chofer | Diseñada, pendiente |
| 5 — Monitoreo de controles | Documentada, pospuesta |
