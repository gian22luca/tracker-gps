# Backend — Vixel (Django REST Framework)

Reemplaza dos cosas que hoy viven sin login ni base de datos real:

- El historial de viajes de `index.html`, que hoy solo existe en
  `localStorage` del navegador (se pierde si cambiás de PC o borrás caché,
  y no se registra nada si no hay una pestaña abierta en ese momento).
- La configuración remota de `control_remoto.html`, que hoy manda la
  Write API Key de ThingSpeak directo desde el navegador sin ninguna
  autenticación.

## Apps

- **`accounts`** — usuario custom con rol (`admin` / `despachante` /
  `chofer`, ver `REQUERIMIENTOS.md` sección 3) y login vía JWT.
- **`flota`** — dispositivos (colectivos), su configuración remota,
  paradas, posiciones GPS y el historial de viajes con sus desvíos.

Roles: cualquier usuario logueado puede **leer** todo. Solo `admin` puede
crear/editar dispositivos, paradas y configuración (`flota/permissions.py`).

## Setup local

```bash
cd backend
python3 -m venv venv
source venv/bin/activate          # Windows: venv\Scripts\activate
pip install -r requirements.txt

python manage.py migrate
python manage.py createsuperuser  # para entrar a /admin/ y probar la API
python manage.py runserver
```

Servidor en `http://127.0.0.1:8000/`.

## Endpoints principales

| Método | Ruta | Rol mínimo |
|---|---|---|
| POST | `/api/auth/login/` | — (usuario/contraseña → JWT) |
| POST | `/api/auth/refresh/` | — |
| GET | `/api/auth/perfil/` | logueado |
| GET/POST | `/api/dispositivos/` | lectura: logueado · escritura: admin |
| GET/PATCH | `/api/dispositivos/{id}/configuracion/` | lectura: logueado · escritura: admin |
| GET | `/api/dispositivos/{id}/posicion-actual/` | logueado |
| GET/POST | `/api/posiciones/?dispositivo=<id>` | logueado |
| GET/POST | `/api/viajes/?dispositivo=<id>` | logueado |
| GET/POST | `/api/paradas/` | lectura: logueado · escritura: admin |

`/admin/` tiene el panel de administración de Django (gestión de flota,
usuarios y paradas sin tocar código).

## Deploy

**Frontend (`index.html`, `control_remoto.html`) en Vercel** — es justo lo
que Vercel hace mejor: sitio estático, CDN, deploy en cada push. Import
Project apuntando a la raíz del repo (no a `backend/`).

**Backend (esta carpeta) en Railway o Render**, no en Vercel — Django es un
proceso persistente con base de datos, no encaja bien en el modelo
serverless de Vercel (filesystem efímero, cold starts, migraciones que no
tienen un buen lugar donde correrse). Railway/Render te dan el proceso
persistente + Postgres pegado con un click.

Pasos (Railway o Render, son casi idénticos):

1. Crear el servicio apuntando a la carpeta `backend/` del repo.
2. Agregar una base Postgres (add-on de un click en ambos) — la plataforma
   inyecta `DATABASE_URL` sola, `settings.py` ya la lee automáticamente.
3. Variables de entorno a setear: `DJANGO_SECRET_KEY` (generar una nueva,
   no usar la de desarrollo), `DJANGO_DEBUG=false`, `DJANGO_ALLOWED_HOSTS`
   (el dominio que te asigne la plataforma), `DJANGO_CORS_ALLOWED_ORIGINS`
   (el dominio de Vercel del frontend).
4. Build command: `pip install -r requirements.txt && python manage.py collectstatic --noinput && python manage.py migrate`
5. Start command: `gunicorn vixel_backend.wsgi` (agregar `gunicorn` a
   `requirements.txt` si la plataforma no lo incluye por defecto).
6. `python manage.py createsuperuser` una vez, vía la consola/shell que
   ofrezca la plataforma, para tener el primer usuario `admin`.

`vercel.json` queda en el repo por si en algún momento se prueba Vercel
para el backend igual (soporte oficial de Django, ver comentarios en
`settings.py`), pero no es el camino recomendado.

## Qué falta (próximos pasos, no implementado todavía)

- **Poller de ThingSpeak → `Posicion`**: un job periódico (management
  command o Celery/cron) que lea `feeds/last.json` de cada canal de
  ThingSpeak y cree un `Posicion` — así el firmware (`tracker_gps.ino`)
  no se toca todavía (sigue hablando con ThingSpeak como hoy).
- **Cálculo server-side de `Viaje`/`Desvio`**: portar la máquina de
  estados que hoy vive en el JS de `index.html` (detección de
  terminal/ida/vuelta/desvíos) a este backend, para que corra sola sin
  depender de una pestaña de navegador abierta.
- **Migrar `index.html` y `control_remoto.html`** para que hablen contra
  esta API en vez de `localStorage`/ThingSpeak directo.
- **App `mensajeria`** (avisos al chofer + confirmaciones, Fase 4 de
  `ROADMAP.md`) — todavía no se creó, se suma cuando esa fase arranque.
