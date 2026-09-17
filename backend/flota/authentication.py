from django.contrib.auth.models import AnonymousUser
from rest_framework.authentication import BaseAuthentication
from rest_framework.exceptions import AuthenticationFailed

from .models import Dispositivo


class DeviceKeyAuthentication(BaseAuthentication):
    """Autentica un dispositivo fisico (no una persona) via su device_key
    propia, mandada como 'Authorization: DeviceKey <key>'.

    request.user queda anonimo a proposito (un dispositivo no es un
    Usuario) y request.auth queda con la instancia de Dispositivo — asi
    la vista sabe para cual unidad reportar sin que el dispositivo pueda
    declarar "soy otro" en el body del request."""

    keyword = "DeviceKey"

    def authenticate(self, request):
        header = request.META.get("HTTP_AUTHORIZATION", "")
        if not header.startswith(self.keyword + " "):
            return None

        key = header[len(self.keyword) + 1:].strip()
        if not key:
            raise AuthenticationFailed("Device key vacia.")

        try:
            dispositivo = Dispositivo.objects.get(device_key=key, activo=True)
        except Dispositivo.DoesNotExist:
            raise AuthenticationFailed("Device key invalida o dispositivo inactivo.")

        return (AnonymousUser(), dispositivo)
