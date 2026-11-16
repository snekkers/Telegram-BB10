#!/usr/bin/python3
"""Deterministic pipe peer for Qt tests. Never contacts Telegram."""
import base64
import json
import os
import sys
import time
from pathlib import Path

scenario_path = Path(sys.argv[sys.argv.index("--library") + 1])
scenario = scenario_path.read_text()
assert os.environ["LD_LIBRARY_PATH"].split(":")[0] == str(scenario_path.parent)


def send(value, split=False):
    wire = (json.dumps(value, ensure_ascii=False) + "\r\n").encode()
    if split:
        for i in range(0, len(wire), 7):
            os.write(1, wire[i:i + 7])
            time.sleep(0.001)
    else:
        os.write(1, wire)


def state(name):
    send({"@type": "updateAuthorizationState", "authorization_state": {"@type": "authorizationState" + name}})


def reply(request, kind="ok", **fields):
    send({"@type": kind, "@extra": request["@extra"], **fields})


if scenario == "malformed":
    os.write(1, b"{not-json}\n")
elif scenario == "oversized":
    os.write(1, b"x" * (8 * 1024 * 1024 + 1))
elif scenario == "crash":
    sys.exit(23)
else:
    state("WaitTdlibParameters")

for line in sys.stdin:
    query = json.loads(line)
    kind = query["@type"]
    if kind == "getOption":
        reply(query, "optionValueString", value="1.8.69 · Тест 😀")
    elif kind == "setTdlibParameters":
        assert query["api_id"] == 12345
        assert query["api_hash"] == "a" * 32
        assert len(base64.b64decode(query["database_encryption_key"])) == 32
        assert Path(query["database_directory"]).parent == Path(query["files_directory"]).parent
        assert query["use_test_dc"] is False
        assert query["use_message_database"] is False
        assert query["use_secret_chats"] is False
        if scenario == "email":
            state("WaitEmailAddress")
        elif scenario == "registration":
            state("WaitRegistration")
        elif scenario == "premium":
            state("WaitPremiumPurchase")
        else:
            state("WaitPhoneNumber")
        # Deliberately stale response after the new state. Must not change it.
        reply(query, "error", code=400, message="stale_api_hash_must_not_appear")
    elif kind == "setAuthenticationPhoneNumber":
        assert query["phone_number"] == "+79990000000"
        # Success is not the next auth state. Input must stay disabled in between.
        reply(query)
        time.sleep(0.15)
        state("WaitPhoneNumber")
        time.sleep(0.15)
        state("WaitCode")
    elif kind == "checkAuthenticationCode":
        if query["code"] == "bad":
            reply(query, "error", code=400, message="PHONE_CODE_INVALID")
        elif query["code"] == "flood":
            reply(query, "error", code=429, message="FLOOD_WAIT_42")
        elif query["code"] == "unknown":
            reply(query, "error", code=500, message="secret diagnostic that must never reach QML")
        else:
            assert query["code"] == "12345"
            state("WaitPassword")
            reply(query)
    elif kind == "checkAuthenticationPassword":
        assert query["password"] == '  Пароль " \\ \n😀  '
        # Includes a split UTF-8 message with an irrelevant close-state string.
        send({"@type": "unrelated", "value": "Привет 😀 authorizationStateClosed"}, split=True)
        state("Ready")
        reply(query)
    elif kind == "setAuthenticationEmailAddress":
        assert query["email_address"] == "test@example.com"
        state("WaitEmailCode")
        reply(query)
    elif kind == "checkAuthenticationEmailCode":
        assert query["code"] == {"@type": "emailAddressAuthenticationCode", "code": "12345"}
        state("Ready")
        reply(query)
    else:
        raise AssertionError(kind)

state("Closing")
state("Closed")
