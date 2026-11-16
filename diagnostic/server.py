#!/usr/bin/env python3
"""Loopback diagnostic UI. No account data or request logging; native shared core."""
import argparse
import hmac
import json
import os
import secrets
import signal
import re
import shutil
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import quote

ROOT = Path(__file__).resolve().parent.parent
WEB = Path(__file__).parent / "web"
MAX_BODY = 65536
ACTIONS = {"start", "stop", "configure", "useSaved", "forgetApi", "submit", "openChat", "closeChat",
           "sendText", "draft", "search", "nextUnread", "recoverDraft", "loadChats", "loadHistory", "download", "markRead", "demoLogin"}


class Bridge:
    def __init__(self, core, engine, library, data, synthetic=False):
        self.data = Path(data)
        self.synthetic = synthetic
        environment = os.environ.copy()
        if Path("/etc/ssl/cert.pem").is_file():
            environment.setdefault("SSL_CERT_FILE", "/etc/ssl/cert.pem")
        self.process = subprocess.Popen([str(core), str(engine), str(library), str(data)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding="utf-8", bufsize=1, env=environment)
        self.lock = threading.Lock()
        self.write_lock = threading.Lock()
        self.pending = {}
        self.initialized = threading.Event()
        self.state = {"phase": "starting", "status": "Запуск диагностического клиента…"}
        threading.Thread(target=self.read, daemon=True).start()

    def read(self):
        try:
            for line in self.process.stdout:
                value = json.loads(line)
                if value.get("@type") == "snapshot":
                    with self.lock:
                        self.state = value
                    self.initialized.set()
                elif value.get("@type") == "commandResult":
                    with self.lock:
                        pending = self.pending.get(value.get("id"))
                        if pending is not None:
                            pending["result"] = bool(value.get("ok"))
                            pending["event"].set()
        finally:
            self.initialized.set()
            with self.lock:
                self.state = {**self.state, "phase": "failed", "ready": False, "canSend": False,
                              "status": "Диагностический процесс завершился. Перезапустите стенд."}
                for pending in self.pending.values():
                    pending["event"].set()

    def command(self, value):
        if not self.initialized.wait(10):
            return None
        identifier = secrets.token_hex(12)
        pending = {"event": threading.Event(), "result": None}
        with self.lock:
            if self.process.poll() is not None:
                return None
            self.pending[identifier] = pending
        try:
            with self.write_lock:
                self.process.stdin.write(json.dumps({**value, "id": identifier}, ensure_ascii=False) + "\n")
                self.process.stdin.flush()
            pending["event"].wait(3)
            return pending["result"]
        finally:
            with self.lock:
                self.pending.pop(identifier, None)

    def snapshot(self):
        with self.lock:
            return {**self.state, "testFixture": self.synthetic}

    def close(self):
        if self.process.poll() is None:
            self.process.stdin.close()
            try:
                self.process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=2)


def create_server(bridge, token, port=0):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def reply(self, status, content=b"", content_type="application/json; charset=utf-8"):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Referrer-Policy", "no-referrer")
            self.send_header("Content-Security-Policy", "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self' blob:; base-uri 'none'; frame-ancestors 'none'")
            self.end_headers()
            self.wfile.write(content)

        def valid_host(self):
            return self.headers.get("Host") == f"127.0.0.1:{self.server.server_port}"

        def authorized(self):
            return hmac.compare_digest(self.headers.get("Authorization", ""), "Bearer " + token)

        def do_GET(self):
            if not self.valid_host():
                return self.reply(403)
            static = {"/": ("index.html", "text/html; charset=utf-8"),
                      "/app.js": ("app.js", "text/javascript; charset=utf-8"),
                      "/style.css": ("style.css", "text/css; charset=utf-8")}
            if self.path in static:
                name, mime = static[self.path]
                return self.reply(200, (WEB / name).read_bytes(), mime)
            if self.path == "/api/state":
                if not self.authorized():
                    return self.reply(401)
                return self.reply(200, json.dumps(bridge.snapshot(), ensure_ascii=False).encode())
            media = re.fullmatch(r"/api/media/(-?\d+)/(-?\d+)", self.path)
            if media:
                if not self.authorized():
                    return self.reply(401)
                state = bridge.snapshot()
                if state.get("chatId") != media.group(1):
                    return self.reply(409)
                selected = next((m.get("media", {}) for m in state.get("messages", []) if m.get("messageId") == media.group(2)), {})
                path = Path(selected.get("path", ""))
                boundary = (bridge.data / "files").resolve()
                try:
                    if not path.is_file() or path.is_symlink() or not path.resolve().is_relative_to(boundary) or path.stat().st_size > 50 * 1024 * 1024:
                        return self.reply(404)
                    with path.open("rb") as source:
                        self.send_response(200)
                        self.send_header("Content-Type", "application/octet-stream")
                        self.send_header("Content-Length", str(path.stat().st_size))
                        self.send_header("Cache-Control", "no-store")
                        self.send_header("X-Content-Type-Options", "nosniff")
                        self.send_header("Content-Disposition", "attachment; filename*=UTF-8''" + quote(selected.get("name", "attachment"), safe=""))
                        self.end_headers()
                        shutil.copyfileobj(source, self.wfile, 65536)
                except OSError:
                    return
                return
            self.reply(404)

        def do_POST(self):
            origin = f"http://127.0.0.1:{self.server.server_port}"
            if not self.valid_host() or self.headers.get("Origin") != origin or not self.authorized():
                return self.reply(403)
            if self.path != "/api/command":
                return self.reply(404)
            if self.headers.get("Content-Type") != "application/json":
                return self.reply(415)
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= MAX_BODY:
                    return self.reply(413)
                command = json.loads(self.rfile.read(length))
                if not isinstance(command, dict) or command.get("action") not in ACTIONS:
                    return self.reply(400)
                if command.get("action") == "demoLogin":
                    if not bridge.synthetic:
                        return self.reply(403)
                    accepted = demo_login(bridge)
                else:
                    accepted = bridge.command(command)
            except (ValueError, TypeError, UnicodeError, BrokenPipeError, OSError):
                return self.reply(400)
            self.reply(200 if accepted is True else 409 if accepted is False else 503,
                       b'{"ok":true}' if accepted is True else b'{"ok":false}')

    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    server.daemon_threads = True
    return server


def demo_login(bridge):
    if not bridge.synthetic:
        return False
    phase = bridge.snapshot().get("phase")
    if phase in ("stopped", "failed", "starting"):
        bridge.command({"action": "start"})
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        phase = bridge.snapshot().get("phase")
        if phase == "ready":
            return True
        if phase == "parameters":
            return bridge.command({"action": "configure", "apiId": "12345", "apiHash": "a" * 32, "remember": False})
        time.sleep(0.03)
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--core", type=Path, default=ROOT / "build/macos-development/client/berrygram-diagnostic-core")
    parser.add_argument("--engine", type=Path, default=ROOT / "build/telegram/macos/berrygram-tdjson")
    parser.add_argument("--library", type=Path, default=ROOT / "build/macos-development/tdlib/libtdjson.1.8.69.dylib")
    parser.add_argument("--data", type=Path, default=Path.home() / "Library/Application Support/Berrygram Diagnostic")
    parser.add_argument("--synthetic", action="store_true", help="Clearly label a test-process UI; never use with a real account")
    parser.add_argument("--access-key-file", type=Path, help="Reuse a private local UI access key when restarting the server")
    parser.add_argument("--start", action="store_true", help="Start TDLib up to the parameters step; does not submit credentials")
    args = parser.parse_args()
    if args.synthetic and args.engine.resolve() != (ROOT / "tests/fake-chat-engine.py").resolve():
        parser.error("Synthetic mode accepts only the bundled fake-chat-engine.py")
    if any(not path.is_file() for path in (args.core, args.engine, args.library)):
        parser.error("Сначала выполните scripts/build-macos-diagnostic.sh")
    os.umask(0o077)
    token = secrets.token_urlsafe(32)
    if args.access_key_file:
        path = args.access_key_file
        if path.is_symlink() or not path.is_file() or path.stat().st_uid != os.getuid() or path.stat().st_mode & 0o077:
            parser.error("Access key file must be a private regular file owned by the current user")
        token = path.read_text().strip()
        if not re.fullmatch(r"[A-Za-z0-9_-]{32,128}", token):
            parser.error("Invalid local access key file")
    bridge = Bridge(args.core.resolve(), args.engine.resolve(), args.library.resolve(), args.data.resolve(), args.synthetic)
    if args.synthetic:
        demo_login(bridge)
    elif args.start:
        bridge.command({"action": "start"})
    server = create_server(bridge, token, args.port)
    url = f"http://127.0.0.1:{server.server_port}/#{token}"
    print(url, flush=True)
    # Private capability URL, useful for opening the local browser without logs.
    metadata = ROOT / "build/macos-development" / ("demo-url.txt" if args.synthetic else "diagnostic-url.txt")
    metadata.write_text(url + "\n")
    metadata.chmod(0o600)
    def stop(*_):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        bridge.close()
        if metadata.exists() and metadata.read_text().strip() == url:
            metadata.unlink()


if __name__ == "__main__":
    main()
