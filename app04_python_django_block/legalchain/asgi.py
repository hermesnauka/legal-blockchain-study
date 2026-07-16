"""
ASGI entry point: HTTP (REST API) plus the two WebSocket endpoints shared by every
LegalChain node — /ws/p2p (node-to-node, PQC-encrypted protocol) and /ws/events
(frontend live feed, public ledger data only).
"""
import os

from django.core.asgi import get_asgi_application

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "legalchain.settings")
django_asgi_app = get_asgi_application()

from channels.routing import ProtocolTypeRouter, URLRouter  # noqa: E402
from django.urls import path  # noqa: E402

from ledger import consumers  # noqa: E402

application = ProtocolTypeRouter({
    "http": django_asgi_app,
    "websocket": URLRouter([
        path("ws/p2p", consumers.P2pConsumer.as_asgi()),
        path("ws/events", consumers.EventsConsumer.as_asgi()),
    ]),
})
