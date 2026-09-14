from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response

from .serializers import PerfilSerializer


@api_view(["GET"])
@permission_classes([IsAuthenticated])
def perfil_actual(request):
    """Devuelve el usuario logueado (con su rol) para que el frontend sepa
    que mostrar/ocultar sin tener que decodificar el JWT a mano."""
    return Response(PerfilSerializer(request.user).data)
