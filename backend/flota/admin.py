from django.contrib import admin

from .models import ConfiguracionDispositivo, Desvio, Dispositivo, Parada, Posicion, Viaje


class ConfiguracionInline(admin.StackedInline):
    model = ConfiguracionDispositivo
    can_delete = False


@admin.register(Dispositivo)
class DispositivoAdmin(admin.ModelAdmin):
    list_display = ("nombre", "activo", "thingspeak_channel_id", "creado")
    list_filter = ("activo",)
    inlines = [ConfiguracionInline]


@admin.register(Parada)
class ParadaAdmin(admin.ModelAdmin):
    list_display = ("orden", "nombre", "lat", "lon", "audio_track")
    ordering = ("orden",)


class DesvioInline(admin.TabularInline):
    model = Desvio
    extra = 0


@admin.register(Viaje)
class ViajeAdmin(admin.ModelAdmin):
    list_display = ("dispositivo", "direccion", "inicio", "fin", "distancia_km", "velocidad_media_kmh")
    list_filter = ("dispositivo", "direccion")
    inlines = [DesvioInline]


@admin.register(Posicion)
class PosicionAdmin(admin.ModelAdmin):
    list_display = ("dispositivo", "timestamp", "lat", "lon", "hdop")
    list_filter = ("dispositivo",)
    date_hierarchy = "timestamp"
