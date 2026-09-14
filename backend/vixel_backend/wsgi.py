"""
WSGI config for vixel_backend project.

It exposes the WSGI callable as a module-level variable named ``application``.

For more information on this file, see
https://docs.djangoproject.com/en/5.2/howto/deployment/wsgi/
"""

import os

from django.core.wsgi import get_wsgi_application

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "vixel_backend.settings")

application = get_wsgi_application()

# El runtime Python de Vercel busca una variable llamada "app" como punto
# de entrada del handler WSGI.
app = application
