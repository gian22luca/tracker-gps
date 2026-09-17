from django.contrib.admin import AdminSite


class VixelAdminSite(AdminSite):
    """El /admin/ de Django es exclusivamente para el superusuario "root"
    de Vixel — ni un usuario con rol=admin de una empresa cliente puede
    entrar, aunque tenga is_staff. Paradas/Posiciones/Viajes se
    administran desde vixel.com.ar (API + JWT), no desde aca."""

    site_header = "Vixel — Administración"
    site_title = "Vixel Admin"

    def has_permission(self, request):
        return bool(
            request.user
            and request.user.is_active
            and request.user.is_superuser
        )
