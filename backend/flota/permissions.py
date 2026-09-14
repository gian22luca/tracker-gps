from rest_framework.permissions import SAFE_METHODS, BasePermission

from accounts.models import User


class EsAdminOSoloLectura(BasePermission):
    """Cualquier usuario autenticado puede leer (despachante, chofer, admin).
    Solo el rol admin puede crear/editar/borrar — pensado para Dispositivo,
    ConfiguracionDispositivo y Parada, que son datos de flota, no telemetria."""

    def has_permission(self, request, view):
        if not (request.user and request.user.is_authenticated):
            return False
        if request.method in SAFE_METHODS:
            return True
        return request.user.rol == User.Rol.ADMIN
