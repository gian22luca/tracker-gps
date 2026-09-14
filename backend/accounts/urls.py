from django.urls import path

from .views import perfil_actual

urlpatterns = [
    path("perfil/", perfil_actual, name="perfil-actual"),
]
