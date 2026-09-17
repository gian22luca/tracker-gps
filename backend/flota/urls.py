from django.urls import path
from rest_framework.routers import DefaultRouter

from .views import (
    DispositivoViewSet,
    ParadaViewSet,
    PosicionViewSet,
    ReportarPosicionView,
    ViajeViewSet,
)

router = DefaultRouter()
router.register("dispositivos", DispositivoViewSet, basename="dispositivo")
router.register("paradas", ParadaViewSet, basename="parada")
router.register("posiciones", PosicionViewSet, basename="posicion")
router.register("viajes", ViajeViewSet, basename="viaje")

urlpatterns = router.urls + [
    path("reportar/", ReportarPosicionView.as_view(), name="reportar-posicion"),
]
