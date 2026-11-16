#!/usr/bin/env python3
"""Exercise the host with real TDLib without parameters, network login or a session."""

import json
import os
import selectors
import signal
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
HOST = ROOT / "build/telegram/macos/berrygram-tdjson"
LIBRARY = ROOT / "build/tdlib/macos/lib/libtdjson.1.8.69.dylib"
RESULTS = []


class Client:
    def __init__(self):
        self.process = subprocess.Popen(
            [str(HOST), "--library", str(LIBRARY)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)
        self.pending = b""
        self.events = []

    def send(self, request):
        self.process.stdin.write((json.dumps(request, ensure_ascii=False) + "\n").encode())
        self.process.stdin.flush()

    def until(self, predicate, timeout=15):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for event in self.events:
                if predicate(event):
                    return event
            if not self.selector.select(max(0, deadline - time.monotonic())):
                break
            data = os.read(self.process.stdout.fileno(), 65536)
            if not data:
                break
            self.pending += data
            while b"\n" in self.pending:
                line, self.pending = self.pending.split(b"\n", 1)
                self.events.append(json.loads(line))
        raise AssertionError("Expected TDLib event did not arrive: " + repr(self.events))

    def finish(self, expected=0):
        code = self.process.wait(timeout=15)
        stderr = self.process.stderr.read().decode()
        assert code == expected, (code, stderr)
        if expected == 0:
            assert not stderr, stderr

    def dispose(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait()
        self.selector.close()
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            stream.close()


def closed(event):
    return event.get("@type") == "updateAuthorizationState" and event.get(
        "authorization_state", {}
    ).get("@type") == "authorizationStateClosed"


def waiting(event):
    return event.get("@type") == "updateAuthorizationState" and event.get(
        "authorization_state", {}
    ).get("@type") == "authorizationStateWaitTdlibParameters"


def main():
    client = Client()
    try:
        client.until(waiting)
        client.send({"@type": "getOption", "name": "version", "@extra": "version"})
        version = client.until(lambda event: event.get("@extra") == "version")
        assert version["@type"] == "optionValueString", version
        assert version["value"] == "1.8.69", version

        prefix = "😀 Привет "
        request = {"@type": "getTextEntities", "text": prefix + "@telegram", "@extra": "unicode"}
        wire = (json.dumps(request, ensure_ascii=False) + "\r\n").encode()
        split = wire.index("😀".encode()) + 2
        client.process.stdin.write(wire[:split])
        client.process.stdin.flush()
        client.process.stdin.write(wire[split:])
        client.process.stdin.flush()
        entities = client.until(lambda event: event.get("@extra") == "unicode")
        assert entities["@type"] == "textEntities", entities
        mention = next(e for e in entities["entities"] if e["type"]["@type"] == "textEntityTypeMention")
        assert mention["offset"] == len(prefix.encode("utf-16-le")) // 2, mention
        assert mention["length"] == len("@telegram"), mention

        # A state name in a string is ordinary data, not a close notification.
        marker = {"@type": "getTextEntities", "text": "authorizationStateClosed", "@extra": "still-open"}
        client.send(marker)
        client.until(lambda event: event.get("@extra") == "still-open")
        assert client.process.poll() is None
        client.send({"@type": "berrygramUnknownRequest", "@extra": "invalid"})
        error = client.until(lambda event: event.get("@extra") == "invalid")
        assert error["@type"] == "error", error
        client.send({"@type": "close"})
        client.until(closed)
        client.finish()
        RESULTS.append({"test": "real_tdlib_round_trip", "passed": True,
                        "version": version["value"], "authorization": "WaitTdlibParameters",
                        "checks": ["request correlation", "split UTF-8 and CRLF", "UTF-16 entity offsets",
                                   "state name inside text", "TDLib errors", "explicit close"]})
    finally:
        client.dispose()

    for name, payload, expected, signum in [
        ("eof_closes_client", b'{"@type":"getOption","name":"version"}', 0, None),
        ("nul_rejected", b'{"@type":"x"}\x00\n', 65, None),
        ("oversized_request_rejected", b"x" * (1024 * 1024 + 1) + b"\n", 65, None),
        ("sigterm_closes_client", None, 128 + signal.SIGTERM, signal.SIGTERM),
    ]:
        client = Client()
        try:
            client.until(waiting)
            if payload is not None:
                client.process.stdin.write(payload)
                client.process.stdin.flush()
                client.process.stdin.close()
            if signum is not None:
                client.process.send_signal(signum)
            client.until(closed)
            client.finish(expected)
            if name == "eof_closes_client":
                assert any(e.get("@type") == "optionValueString" for e in client.events)
            RESULTS.append({"test": name, "passed": True})
        finally:
            client.dispose()

    for name, library, expected in [
        ("missing_library", str(ROOT / "build/tdlib/no-library.dylib"), 69),
        ("incomplete_interface", "/usr/lib/libSystem.B.dylib", 69),
        ("relative_library_path", "libtdjson.dylib", 64),
    ]:
        result = subprocess.run([str(HOST), "--library", library], capture_output=True, timeout=5)
        assert result.returncode == expected, (name, result.returncode, result.stderr)
        assert not result.stdout, (name, result.stdout)
        RESULTS.append({"test": name, "passed": True})

    report = {"platform": "macOS arm64", "library": str(LIBRARY), "tests": RESULTS,
              "account_login_tested": False, "qnx_runtime_tested": False}
    destination = ROOT / "build/telegram/macos/integration-results.json"
    destination.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
