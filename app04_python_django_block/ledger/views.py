"""
REST view layer for API Contract v1 — one thin view function per endpoint; all ledger
logic lives in ledger.core/consensus/contracts/nft/p2p. Mirrors the sibling nodes'
controllers (app01 Spring @RestController, app02/app03 equivalents): views only parse
the request, delegate to the domain layer, and shape the JSON response.
"""
import json
import os
import uuid
from dataclasses import replace
from decimal import Decimal, InvalidOperation

from django.conf import settings
from django.http import HttpResponse, JsonResponse
from django.views.decorators.http import require_http_methods

from . import p2p as p2p_module
from .context import get_context
from .core import LedgerEvent, LedgerEventType, Transaction, TransactionType, json_number, now_millis


def _body(request) -> dict:
    if not request.body:
        return {}
    try:
        return json.loads(request.body)
    except json.JSONDecodeError:
        raise ValueError("Malformed JSON body")


def _require_str(body: dict, key: str) -> str:
    value = body.get(key)
    if value is None or not str(value).strip():
        raise ValueError(f"Missing required field: {key}")
    return str(value)


def _decimal(value) -> Decimal:
    try:
        amount = Decimal(str(value))
    except (InvalidOperation, TypeError):
        raise ValueError("Amount must be a number")
    if amount < 0:
        raise ValueError("Amount must not be negative")
    return amount


def api_view(handler):
    """
    Uniform JSON error bodies for rule violations — the Django analogue of the sibling
    nodes' ApiExceptionMiddleware/@RestControllerAdvice: ValueError (broken ledger/
    contract rules or bad input) maps to 400, PermissionError (failed authenticity
    checks) to 403.
    """
    def wrapped(request, *args, **kwargs):
        try:
            result = handler(request, *args, **kwargs)
        except ValueError as e:
            return JsonResponse({"error": str(e)}, status=400)
        except PermissionError as e:
            return JsonResponse({"error": str(e)}, status=403)
        if isinstance(result, HttpResponse):
            return result
        return JsonResponse(result, safe=isinstance(result, dict))
    return wrapped


# ------------------------------------------------------------------ node & P2P

@api_view
@require_http_methods(["GET"])
def node(request):
    ctx = get_context()
    return {
        "nodeId": ctx.wallet.address,
        "name": ctx.node_name,
        "port": settings.LEGALCHAIN["PORT"],
        "consensus": ctx.consensus.active().name,
        "peers": ctx.p2p.connected_peers(),
        "chainLength": ctx.blockchain.length(),
    }


@api_view
@require_http_methods(["POST"])
def p2p_connect(request):
    ctx = get_context()
    url = _require_str(_body(request), "url")
    p2p_module.connect_to_peer(url, ctx.p2p)
    return {"connected": True, "peer": url}


@api_view
@require_http_methods(["POST"])
def p2p_sync(request):
    ctx = get_context()
    asked = ctx.p2p.sync_with_peers()
    return {
        "result": (f"Sync requested from {asked} peer(s)" if asked > 0
                   else "No established peers to sync with"),
        "chainLength": ctx.blockchain.length(),
    }


# ------------------------------------------------------------------------- chain

@api_view
@require_http_methods(["GET"])
def chain(request):
    ctx = get_context()
    blocks = ctx.blockchain.blocks()
    return {
        "length": len(blocks),
        "valid": ctx.blockchain.is_valid(),
        "blocks": [b.to_dict() for b in blocks],
    }


@api_view
@require_http_methods(["GET"])
def chain_validate(request):
    ctx = get_context()
    valid = ctx.blockchain.is_valid()
    return {
        "valid": valid,
        "message": ("Chain audit passed: all hashes, links, ML-DSA signatures and "
                    "consensus proofs verified.") if valid else
                   "Chain audit FAILED: at least one block or signature does not verify.",
    }


@api_view
@require_http_methods(["POST"])
def chain_mine(request):
    ctx = get_context()
    body = _body(request)
    validator = str(body.get("validatorId") or "").strip() or ctx.wallet.address
    block = ctx.blockchain.mine_pending_transactions(validator)
    return block.to_dict()


# ------------------------------------------------------------------- transactions

@api_view
@require_http_methods(["GET"])
def transactions_pending(request):
    ctx = get_context()
    return [tx.to_dict() for tx in ctx.blockchain.pending_transactions()]


@api_view
@require_http_methods(["POST"])
def transactions_submit(request):
    """
    In the MVP the browser has no key store, so the node wallet signs on the user's
    behalf (custodial model); the ML-DSA signature is created here and verified again
    by Blockchain.submit — the ledger never trusts this API layer.
    """
    ctx = get_context()
    body = _body(request)
    recipient = _require_str(body, "recipient")
    amount = _decimal(body.get("amount"))
    memo = body.get("memo")
    unsigned = Transaction(
        id=str(uuid.uuid4()), timestamp=now_millis(),
        sender=ctx.wallet.address, recipient=recipient, amount=amount,
        type=TransactionType.TRANSFER, payload={"memo": str(memo)} if memo else {},
        sender_public_key=ctx.wallet.public_key_b64, signature=None)
    signed = replace(unsigned, signature=ctx.wallet.sign(unsigned.content_to_sign()))
    submitted = ctx.blockchain.submit(signed)
    return submitted.to_dict()


# ------------------------------------------------------------------------- wallet

@api_view
@require_http_methods(["GET"])
def wallet(request):
    ctx = get_context()
    balance = ctx.blockchain.balances().get(ctx.wallet.address, Decimal(0))
    return {
        "address": ctx.wallet.address,
        "algorithm": ctx.wallet.algorithm(),
        "publicKey": ctx.wallet.public_key_b64,
        "fingerprint": ctx.wallet.address,
        "balance": json_number(balance),
    }


@api_view
@require_http_methods(["GET"])
def wallet_balances(request):
    ctx = get_context()
    return {address: json_number(amount) for address, amount in ctx.blockchain.balances().items()}


# ---------------------------------------------------------------------- consensus

@api_view
@require_http_methods(["GET", "POST"])
def consensus(request):
    ctx = get_context()
    if request.method == "POST":
        strategy = _require_str(_body(request), "strategy")
        ctx.consensus.switch_to(strategy)
        ctx.events.publish(LedgerEvent(LedgerEventType.CONSENSUS_CHANGED,
                                       {"active": ctx.consensus.active().name}))
        return {"active": ctx.consensus.active().name}
    return {
        "active": ctx.consensus.active().name,
        "available": [{"name": s.name, "description": s.description}
                      for s in ctx.consensus.available()],
    }


# --------------------------------------------------------------------------- NFT

@api_view
@require_http_methods(["POST"])
def nft_mint(request):
    ctx = get_context()
    body = _body(request)
    title = _require_str(body, "title")
    description = _require_str(body, "description")
    metadata_uri = str(body.get("metadataUri") or "").strip() or "ipfs://placeholder"
    minted = ctx.nft.mint(title, description, metadata_uri)
    return minted.to_dict()


@api_view
@require_http_methods(["GET"])
def nft_all(request):
    ctx = get_context()
    return [n.to_dict() for n in ctx.nft.all()]


# --------------------------------------------------------------------- contracts

@api_view
@require_http_methods(["POST"])
def medical_consent(request):
    ctx = get_context()
    body = _body(request)
    result = ctx.contract_engine.execute(ctx.medical, TransactionType.CONTRACT_MEDICAL, {
        "patientId": _require_str(body, "patientId"),
        "granteeId": _require_str(body, "granteeId"),
        "scope": _require_str(body, "scope"),
        "granted": str(bool(body.get("granted"))).lower(),
    })
    return result.to_dict()


@api_view
@require_http_methods(["GET"])
def medical_history(request, patient_id):
    ctx = get_context()
    return ctx.medical.history_of(patient_id)


@api_view
@require_http_methods(["POST"])
def agri_event(request):
    ctx = get_context()
    body = _body(request)
    result = ctx.contract_engine.execute(ctx.agri, TransactionType.CONTRACT_AGRI, {
        "batchId": _require_str(body, "batchId"),
        "stage": _require_str(body, "stage"),
        "actor": _require_str(body, "actor"),
        "location": _require_str(body, "location"),
        "details": str(body.get("details") or ""),
    })
    return result.to_dict()


@api_view
@require_http_methods(["GET"])
def agri_trail(request, batch_id):
    ctx = get_context()
    return ctx.agri.trail_of(batch_id)


# ------------------------------------------------------------------------- i18n

_I18N_SUPPORTED = ("en", "pl")
_i18n_cache: dict = {}


@api_view
@require_http_methods(["GET"])
def i18n_messages(request, lang):
    normalized = lang.lower()
    if normalized not in _I18N_SUPPORTED:
        raise ValueError(f"Unsupported language: {lang} (supported: en, pl)")
    if normalized not in _i18n_cache:
        path = os.path.join(settings.BASE_DIR, "i18n", f"messages_{normalized}.json")
        with open(path, encoding="utf-8") as f:
            _i18n_cache[normalized] = json.load(f)
    return _i18n_cache[normalized]
