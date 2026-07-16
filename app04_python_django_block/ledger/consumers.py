"""
WebSocket adapters (Django Channels): thin transport glue — all protocol logic lives in
the transport-agnostic ledger.p2p module, mirroring the sibling apps' design.
"""
from channels.generic.websocket import WebsocketConsumer

from .context import get_context


class P2pConsumer(WebsocketConsumer):
    """Inbound /ws/p2p endpoint: the non-initiating side of the PQC handshake."""

    def connect(self):
        self.accept()
        self.peer = get_context().p2p.register(
            lambda text: self.send(text_data=text), initiator=False)

    def receive(self, text_data=None, bytes_data=None):
        if text_data is not None:
            get_context().p2p.on_message(self.peer, text_data)

    def disconnect(self, code):
        get_context().p2p.unregister(self.peer)


class EventsConsumer(WebsocketConsumer):
    """Browser live feed /ws/events: public ledger events, unencrypted by design."""

    def connect(self):
        self.accept()
        self.session_id = get_context().broadcaster.attach(
            lambda text: self.send(text_data=text))

    def disconnect(self, code):
        get_context().broadcaster.detach(self.session_id)
