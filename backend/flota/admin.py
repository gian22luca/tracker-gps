from django.contrib import admin

from .models import (
    ConfiguracionDispositivo,
    Desvio,
    Dispositivo,
    Parada,
    Posicion,
    Viaje,
    generar_device_key,
)


class ConfiguracionInline(admin.StackedInline):
    model = ConfiguracionDispositivo
    can_delete = False


@admin.register(Dispositivo)
class DispositivoAdmin(admin.ModelAdmin):
    """La device_key es exclusiva del superusuario "root" (Vixel) — ni
    siquiera un usuario con rol=admin de una empresa cliente la ve, aunque
    llegue a tener acceso a /admin/ por otro motivo. Un cliente/despachante
    solo ve el nombre del dispositivo, nunca su credencial."""

    list_display = ("nombre", "activo", "thingspeak_channel_id", "creado")
    list_filter = ("activo",)
    actions = ["regenerar_device_key"]
    inlines = [ConfiguracionInline]

    def get_fields(self, request, obj=None):
        fields = list(super().get_fields(request, obj))
        if not request.user.is_superuser and "device_key" in fields:
            fields.remove("device_key")
        return fields

    def get_readonly_fields(self, request, obj=None):
        return ("device_key",) if request.user.is_superuser else ()

    def get_actions(self, request):
        actions = super().get_actions(request)
        if not request.user.is_superuser:
            actions.pop("regenerar_device_key", None)
        return actions

    @admin.action(description="Regenerar device key (invalida la anterior)")
    def regenerar_device_key(self, request, queryset):
        for dispositivo in queryset:
            dispositivo.device_key = generar_device_key()
            dispositivo.save(update_fields=["device_key"])
        self.message_user(request, f"Device key regenerada para {queryset.count()} dispositivo(s).")


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
