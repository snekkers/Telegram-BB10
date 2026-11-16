#!/usr/bin/env python3
"""Exercise the real native Qt bridge and loopback UI without a Telegram account."""
import importlib.util
import json
import os
import tempfile
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("diagnostic_server", ROOT / "diagnostic/server.py")
server_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server_module)
CORE = ROOT / "build/macos-development/client/berrygram-diagnostic-core"
A = "-1001234567890"


def until(bridge, predicate, timeout=10):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        state = bridge.snapshot()
        if predicate(state):
            return state
        if bridge.process.poll() is not None:
            raise AssertionError("Native bridge exited: " + str(bridge.process.returncode))
        time.sleep(0.03)
    raise AssertionError("Native bridge did not reach the expected state: " + bridge.snapshot().get("phase", "unknown"))


def fixture_test():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        library = root / "libtdjson.so"
        library.write_text("fixture")
        bridge = server_module.Bridge(CORE, ROOT / "tests/fake-chat-engine.py", library, root / "private", synthetic=True)
        token = "synthetic-local-token"
        server = server_module.create_server(bridge, token)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        origin = f"http://127.0.0.1:{server.server_port}"

        def request(path="/api/command", value=None, *, authorization=True, origin_header=None, host=None, content_type="application/json"):
            headers = {}
            if authorization:
                headers["Authorization"] = "Bearer " + token
            if host:
                headers["Host"] = host
            data = None if value is None else json.dumps(value).encode()
            if value is not None:
                headers["Content-Type"] = content_type
                headers["Origin"] = origin if origin_header is None else origin_header
            req = urllib.request.Request(origin + path, data=data, headers=headers)
            try:
                with urllib.request.urlopen(req, timeout=15) as response:
                    return response.status, response.read()
            except urllib.error.HTTPError as error:
                return error.code, error.read()

        try:
            assert request("/api/state", authorization=False)[0] == 401
            bridge.synthetic = False
            assert request(value={"action": "demoLogin"})[0] == 403
            bridge.synthetic = True
            assert request("/api/state", host="evil.example")[0] == 403
            assert request(value={"action": "start"}, origin_header="https://evil.example")[0] == 403
            assert request(value={"action": []})[0] == 400
            assert request(value={"action": "unknown"})[0] == 400
            assert request(value={"action": "start"}, content_type="text/plain")[0] == 415
            assert request(value={"action": "draft", "text": "x" * 70000})[0] == 413
            assert request("/../../etc/passwd")[0] == 404
            status, body = request(value={"action": "start"})
            assert status == 200, (status, body)
            until(bridge, lambda s: s.get("phase") == "parameters")
            assert request(value={"action": "configure", "apiId": "12345", "apiHash": "a" * 32, "remember": True})[0] == 200
            state = until(bridge, lambda s: s.get("ready") and len(s.get("chats", [])) == 3)
            assert "a" * 32 not in json.dumps(state)
            assert state["hasSavedApi"] is True
            assert request(value={"action": "openChat", "chatId": A})[0] == 200
            until(bridge, lambda s: len(s.get("messages", [])) == 3)
            assert request(value={"action": "draft", "chatId": A, "text": "Черновик 😀"})[0] == 200
            until(bridge, lambda s: s.get("draft") == "Черновик 😀")
            assert request(value={"action": "sendText", "chatId": "9007199254740991", "text": "never-send-to-wrong-chat"})[0] == 409
            assert request(value={"action": "sendText", "chatId": A, "text": "Тест 😀"})[0] == 200
            until(bridge, lambda s: any(m.get("messageId") == "1000" for m in s.get("messages", [])))
            assert request(value={"action": "download", "chatId": A, "messageId": "20"})[0] == 200
            state = until(bridge, lambda s: any(m.get("media", {}).get("path") for m in s.get("messages", [])))
            code, image = request("/api/media/" + A + "/20")
            assert code == 200 and image.startswith(b"\x89PNG")
            assert request("/api/media/12345/20")[0] == 409
            assert request("/api/media/" + A + "/20", authorization=False)[0] == 401
            path = root / "private/files/photo.png"
            path.unlink()
            path.symlink_to(root / "libtdjson.so")
            assert request("/api/media/" + A + "/20")[0] == 404
            assert request(value={"action": "stop"})[0] == 200
            until(bridge, lambda s: s.get("phase") == "stopped")
            mode = (root / "private/api.json").stat().st_mode & 0o777
            assert mode == 0o600
        finally:
            server.shutdown()
            server.server_close()
            bridge.close()
        assert bridge.process.returncode == 0, bridge.process.returncode
        return {"name": "native_bridge_and_http_fixture", "passed": True,
                "checks": ["native Qt core lifecycle", "HTTP token, origin and host guards", "payload limits",
                           "API persistence permissions and snapshot redaction", "wrong-chat send rejected",
                           "drafts and synthetic messages", "media download and path/symlink restrictions"]}


def real_test():
    library = ROOT / "build/macos-development/tdlib/libtdjson.1.8.69.dylib"
    if not library.is_file():
        raise AssertionError("Build the pinned native TDLib first")
    with tempfile.TemporaryDirectory() as directory:
        data = Path(directory) / "private"
        bridge = server_module.Bridge(CORE, ROOT / "build/telegram/macos/berrygram-tdjson", library, data)
        try:
            assert bridge.command({"action": "start"}) is True
            state = until(bridge, lambda s: s.get("phase") == "parameters" and s.get("version"))
            assert state["version"] == "1.8.69"
            assert not data.exists()
            # EOF also tests destruction while a child is active, without explicit stop.
        finally:
            bridge.close()
        assert bridge.process.returncode == 0, bridge.process.returncode
    return {"name": "locally_built_tdlib_offline", "passed": True, "version": "1.8.69",
            "account_login_tested": False, "network_connection_tested": False}


def main():
    destination = ROOT / "build/macos-development/diagnostic-tests.json"
    destination.unlink(missing_ok=True)
    results = [fixture_test(), real_test()]
    destination.write_text(json.dumps({"platform": "macOS ARM64", "tests": results}, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(results, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
