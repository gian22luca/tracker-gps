from django.conf import settings
from django.db import models


class Dispositivo(models.Model):
    """Un colectivo/unidad de la flota. Reemplaza al array DEVICES
    hardcodeado en index.html."""

    nombre = models.CharField(max_length=100)
    color = models.CharField(
        max_length=7, default="#e74c3c",
        help_text="Color hex usado en el mapa (ej: #e74c3c)",
    )
    thingspeak_channel_id = models.CharField(max_length=20, blank=True)
    thingspeak_read_api_key = models.CharField(max_length=64, blank=True)
    thingspeak_write_api_key = models.CharField(max_length=64, blank=True)
    activo = models.BooleanField(default=True)
    creado = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ["nombre"]

    def __str__(self):
        return self.nombre


class ConfiguracionDispositivo(models.Model):
    """Parametros remotos del dispositivo. Reemplaza a los campos que hoy
    se mandan directo a ThingSpeak desde control_remoto.html sin login."""

    dispositivo = models.OneToOneField(
        Dispositivo, on_delete=models.CASCADE, related_name="configuracion"
    )
    volumen = models.PositiveSmallIntegerField(default=20)  # 0-30 (DFPlayer)
    texto_default = models.CharField(max_length=64, default="LINEA 102 - VIXEL")
    radio_deteccion_parada_m = models.PositiveIntegerField(default=30)
    intervalo_subida_s = models.PositiveIntegerField(default=15)
    actualizado = models.DateTimeField(auto_now=True)
    actualizado_por = models.ForeignKey(
        settings.AUTH_USER_MODEL, null=True, blank=True, on_delete=models.SET_NULL
    )

    def __str__(self):
        return f"Config de {self.dispositivo}"


class Parada(models.Model):
    """Parada de la Ramal A. Se corresponde con el struct Parada de
    tracker_gps.ino (coordenadas, texto en pantalla, pista de audio)."""

    nombre = models.CharField(max_length=100)
    orden = models.PositiveSmallIntegerField()
    lat = models.FloatField()
    lon = models.FloatField()
    texto_pantalla = models.CharField(max_length=64)
    audio_track = models.CharField(
        max_length=10, help_text="Pista en la SD del DFPlayer (ej: 001.mp3)"
    )

    class Meta:
        ordering = ["orden"]

    def __str__(self):
        return f"{self.orden}. {self.nombre}"


class Posicion(models.Model):
    """Un reporte de posicion GPS. Reemplaza al fetch directo a ThingSpeak
    desde el navegador en index.html — esto es lo que un poller server-side
    (o mas adelante el firmware directo) va a ir insertando."""

    dispositivo = models.ForeignKey(
        Dispositivo, on_delete=models.CASCADE, related_name="posiciones"
    )
    lat = models.FloatField()
    lon = models.FloatField()
    hdop = models.FloatField(null=True, blank=True)
    timestamp = models.DateTimeField(help_text="Momento del fix GPS")

    # Medidor de consumo de datos (fields 4-7 del canal de ThingSpeak hoy)
    bytes_enviados = models.PositiveBigIntegerField(null=True, blank=True)
    bytes_recibidos = models.PositiveBigIntegerField(null=True, blank=True)
    reportes_enviados = models.PositiveIntegerField(null=True, blank=True)
    uptime_s = models.PositiveIntegerField(null=True, blank=True)

    creado = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ["-timestamp"]
        indexes = [models.Index(fields=["dispositivo", "-timestamp"])]

    def __str__(self):
        return f"{self.dispositivo} @ {self.timestamp}"


class Viaje(models.Model):
    """Un viaje completo (ida o vuelta). Reemplaza al historial que hoy
    vive solo en localStorage del navegador (gps_trip_history en
    index.html) — la deteccion de ida/vuelta/desvios se recalcula
    server-side para no depender de que haya una pestaña abierta."""

    class Direccion(models.TextChoices):
        IDA = "ida", "Ida"
        VUELTA = "vuelta", "Vuelta"

    dispositivo = models.ForeignKey(
        Dispositivo, on_delete=models.CASCADE, related_name="viajes"
    )
    direccion = models.CharField(max_length=10, choices=Direccion.choices)
    inicio = models.DateTimeField()
    fin = models.DateTimeField(null=True, blank=True)
    distancia_km = models.FloatField(default=0)
    velocidad_media_kmh = models.FloatField(default=0)

    class Meta:
        ordering = ["-inicio"]

    def __str__(self):
        return f"{self.dispositivo} · {self.get_direccion_display()} · {self.inicio:%d/%m %H:%M}"


class Desvio(models.Model):
    """Un desvio de ruta detectado durante un viaje."""

    viaje = models.ForeignKey(Viaje, on_delete=models.CASCADE, related_name="desvios")
    momento = models.DateTimeField()
    distancia_m = models.FloatField()

    class Meta:
        ordering = ["momento"]

    def __str__(self):
        return f"Desvio {self.distancia_m}m @ {self.momento:%H:%M:%S}"
