"""
Django settings for the LegalChain node (app04, Python/Django port).

The ledger itself is deliberately *not* stored in a database: like the Java, C++ and C#
siblings, all chain state lives in an in-memory singleton graph (see ledger/context.py)
so that balances are always derived by replaying the chain and nothing can be edited
behind the ledger's back. The sqlite database below exists only because Django's test
runner expects one.
"""
import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent

# Educational project: this key protects nothing (no sessions, no auth, no cookies).
SECRET_KEY = "dev-only-educational-ledger-not-a-secret"
DEBUG = True
ALLOWED_HOSTS = ["*"]

INSTALLED_APPS = [
    "daphne",  # first, so `manage.py runserver` serves ASGI incl. WebSockets
    "channels",
    "ledger",
]

MIDDLEWARE = [
    "ledger.middleware.CorsMiddleware",
]

ROOT_URLCONF = "legalchain.urls"
ASGI_APPLICATION = "legalchain.asgi.application"

DATABASES = {
    "default": {
        "ENGINE": "django.db.backends.sqlite3",
        "NAME": BASE_DIR / "db.sqlite3",
    }
}

USE_TZ = True

# Node configuration, shared shape with the sibling nodes' application.yml/appsettings.json.
# Override per node via environment: LEGALCHAIN_NODE_NAME=node-B ... runserver 8111
LEGALCHAIN = {
    "NODE_NAME": os.environ.get("LEGALCHAIN_NODE_NAME", "node-A"),
    "CONSENSUS_DEFAULT": os.environ.get("LEGALCHAIN_CONSENSUS", "POS"),
    "TOKENOMICS": {
        # Educational tokenomics: fixed block reward, halved every HALVING_INTERVAL
        # blocks, under a hard supply cap (Bitcoin issuance model, classroom scale).
        "BLOCK_REWARD": "50",
        "HALVING_INTERVAL": 100,
        "MAX_SUPPLY": "21000",
    },
}
