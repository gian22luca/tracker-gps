from django.contrib.auth.models import AbstractUser
from django.db import models


class User(AbstractUser):
    """Usuario del sistema. El rol determina qué puede ver/hacer en la API
    (ver flota/permissions.py) — se corresponde con los actores descriptos
    en REQUERIMIENTOS.md seccion 3."""

    class Rol(models.TextChoices):
        ADMIN = "admin", "Administrador de flota"
        DESPACHANTE = "despachante", "Central / despachante"
        CHOFER = "chofer", "Chofer"

    rol = models.CharField(max_length=20, choices=Rol.choices, default=Rol.DESPACHANTE)

    def __str__(self):
        return f"{self.username} ({self.get_rol_display()})"
