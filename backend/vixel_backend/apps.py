from django.contrib.admin.apps import AdminConfig


class VixelAdminConfig(AdminConfig):
    default_site = "vixel_backend.admin_site.VixelAdminSite"
