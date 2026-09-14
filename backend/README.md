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
