"""URL wiring for API Contract v1 — identical route surface to app01/app02/app03."""
from django.urls import path

from ledger import views

urlpatterns = [
    path("api/node", views.node),
    path("api/p2p/connect", views.p2p_connect),
    path("api/p2p/sync", views.p2p_sync),
    path("api/chain", views.chain),
    path("api/chain/validate", views.chain_validate),
    path("api/chain/mine", views.chain_mine),
    path("api/transactions/pending", views.transactions_pending),
    path("api/transactions", views.transactions_submit),
    path("api/wallet", views.wallet),
    path("api/wallet/balances", views.wallet_balances),
    path("api/consensus", views.consensus),
    path("api/nft/mint", views.nft_mint),
    path("api/nft", views.nft_all),
    path("api/contracts/medical/consent", views.medical_consent),
    path("api/contracts/medical/<str:patient_id>", views.medical_history),
    path("api/contracts/agri/event", views.agri_event),
    path("api/contracts/agri/<str:batch_id>", views.agri_trail),
    path("api/i18n/<str:lang>", views.i18n_messages),
]
