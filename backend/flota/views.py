from django.shortcuts import get_object_or_404
from rest_framework import generics, viewsets
from rest_framework.decorators import action
from rest_framework.response import Response

from .authentication import DeviceKeyAuthentication
from .models import ConfiguracionDispositivo, Dispositivo, Linea, Parada, Posicion, Viaje
from .permissions import EsAdminOSoloLectura, EsDispositivoAutenticado
from .serializers import (
    ConfiguracionDispositivoSerializer,
    DispositivoSerializer,
    LineaSerializer,
    ParadaSerializer,
    PosicionReporteSerializer,
    PosicionSerializer,
    ViajeSerializer,
)


class DispositivoViewSet(viewsets.ModelViewSet):
    queryset = Dispositivo.objects.all()
    serializer_class = DispositivoSerializer
    permission_classes = [EsAdminOSoloLectura]

    def perform_create(self, serializer):
        dispositivo = serializer.save()
        ConfiguracionDispositivo.objects.create(dispositivo=dispositivo)

    @action(detail=True, methods=["get", "patch"])
    def configuracion(self, request, pk=None):
        dispositivo = self.get_object()
        config = get_object_or_404(ConfiguracionDispositivo, dispositivo=dispositivo)

        if request.method == "GET":
            return Response(ConfiguracionDispositivoSerializer(config).data)

        serializer = ConfiguracionDispositivoSerializer(config, data=request.data, partial=True)
        serializer.is_valid(raise_exception=True)
        serializer.save(actualizado_por=request.user)
        return Response(serializer.data)

    @action(detail=True, methods=["get"], url_path="posicion-actual")
    def posicion_actual(self, request, pk=None):
        dispositivo = self.get_object()
        ultima = dispositivo.posiciones.first()  # Posicion.Meta.ordering = -timestamp
        if ultima is None:
            return Response({"detail": "Sin posiciones registradas todavia."}, status=404)
        return Response(PosicionSerializer(ultima).data)


class ParadaViewSet(viewsets.ModelViewSet):
    queryset = Parada.objects.all()
    serializer_class = ParadaSerializer
    permission_classes = [EsAdminOSoloLectura]


class LineaViewSet(viewsets.ModelViewSet):
    queryset = Linea.objects.all()
    serializer_class = LineaSerializer
    permission_classes = [EsAdminOSoloLectura]


class PosicionViewSet(viewsets.ModelViewSet):
    """Alta y consulta de posiciones GPS. Pensado para que lo alimente un
    poller server-side (o mas adelante el firmware directo) — ver la Etapa
    A del plan de migracion. Filtra por ?dispositivo=<id>."""

    serializer_class = PosicionSerializer
    permission_classes = [EsAdminOSoloLectura]

    def get_queryset(self):
        qs = Posicion.objects.all()
        dispositivo_id = self.request.query_params.get("dispositivo")
        if dispositivo_id:
            qs = qs.filter(dispositivo_id=dispositivo_id)
        return qs[:500]


class ReportarPosicionView(generics.CreateAPIView):
    """POST /api/reportar/ — usado por el dispositivo fisico, no por
    usuarios humanos. Autenticado con su device_key (no JWT); el
    dispositivo queda fijado por la key, nunca por un campo del body."""

    serializer_class = PosicionReporteSerializer
    authentication_classes = [DeviceKeyAuthentication]
    permission_classes = [EsDispositivoAutenticado]

    def perform_create(self, serializer):
        serializer.save(dispositivo=self.request.auth)


class ViajeViewSet(viewsets.ModelViewSet):
    """Historial de viajes. Reemplaza a gps_trip_history en localStorage.
    Filtra por ?dispositivo=<id>."""

    serializer_class = ViajeSerializer
    permission_classes = [EsAdminOSoloLectura]

    def get_queryset(self):
        qs = Viaje.objects.all()
        dispositivo_id = self.request.query_params.get("dispositivo")
        if dispositivo_id:
            qs = qs.filter(dispositivo_id=dispositivo_id)
        return qs
