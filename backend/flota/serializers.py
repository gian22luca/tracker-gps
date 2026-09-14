from rest_framework import serializers

from .models import ConfiguracionDispositivo, Desvio, Dispositivo, Parada, Posicion, Viaje


class ConfiguracionDispositivoSerializer(serializers.ModelSerializer):
    class Meta:
        model = ConfiguracionDispositivo
        fields = [
            "volumen",
            "texto_default",
            "radio_deteccion_parada_m",
            "intervalo_subida_s",
            "actualizado",
            "actualizado_por",
        ]
        read_only_fields = ["actualizado", "actualizado_por"]

    def validate_volumen(self, value):
        if not 0 <= value <= 30:
            raise serializers.ValidationError("El volumen va de 0 a 30 (rango del DFPlayer Mini).")
        return value

    def validate_intervalo_subida_s(self, value):
        if value < 15:
            raise serializers.ValidationError(
                "ThingSpeak (plan gratuito) no acepta actualizaciones mas seguidas que cada 15s."
            )
        return value


class DispositivoSerializer(serializers.ModelSerializer):
    configuracion = ConfiguracionDispositivoSerializer(read_only=True)

    class Meta:
        model = Dispositivo
        fields = [
            "id",
            "nombre",
            "color",
            "thingspeak_channel_id",
            "activo",
            "creado",
            "configuracion",
        ]
        # Las API keys de ThingSpeak son write-only: se pueden cargar pero
        # nunca se devuelven en una respuesta (evita repetir el problema de
        # control_remoto.html, donde la write key queda expuesta en el
        # navegador de cualquiera que abra la pagina).
        extra_kwargs = {
            "thingspeak_read_api_key": {"write_only": True},
            "thingspeak_write_api_key": {"write_only": True},
        }


class ParadaSerializer(serializers.ModelSerializer):
    class Meta:
        model = Parada
        fields = ["id", "nombre", "orden", "lat", "lon", "texto_pantalla", "audio_track"]


class PosicionSerializer(serializers.ModelSerializer):
    class Meta:
        model = Posicion
        fields = [
            "id",
            "dispositivo",
            "lat",
            "lon",
            "hdop",
            "timestamp",
            "bytes_enviados",
            "bytes_recibidos",
            "reportes_enviados",
            "uptime_s",
            "creado",
        ]
        read_only_fields = ["creado"]


class DesvioSerializer(serializers.ModelSerializer):
    class Meta:
        model = Desvio
        fields = ["id", "momento", "distancia_m"]


class ViajeSerializer(serializers.ModelSerializer):
    desvios = DesvioSerializer(many=True, read_only=True)
    dispositivo_nombre = serializers.CharField(source="dispositivo.nombre", read_only=True)

    class Meta:
        model = Viaje
        fields = [
            "id",
            "dispositivo",
            "dispositivo_nombre",
            "direccion",
            "inicio",
            "fin",
            "distancia_km",
            "velocidad_media_kmh",
            "desvios",
        ]
