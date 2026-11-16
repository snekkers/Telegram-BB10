#!/usr/bin/env python3
"""Check a staged ARM runtime on Classic without API keys or account access."""
import argparse
import hashlib
import json
import shlex
import sys
import time
import urllib.request
import uuid
from pathlib import Path
from xml.etree import ElementTree


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="169.254.0.1")
    parser.add_argument("--identity", type=Path, required=True)
    parser.add_argument("--known-hosts", type=Path, required=True)
    parser.add_argument("--paramiko-path", type=Path)
    parser.add_argument("--runtime", type=Path, default=root / "build/device/telegram-runtime")
    parser.add_argument("--report", type=Path, default=root / "build/porting/device-tests.json")
    args = parser.parse_args()
    if args.paramiko_path:
        sys.path.insert(0, str(args.paramiko_path))
    import paramiko

    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    with opener.open("http://" + args.host + "/cgi-bin/discovery.cgi", timeout=5) as response:
        discovery = ElementTree.fromstring(response.read(65536))
    values = {item.tag: item.text for item in discovery.iter()
              if item.tag in {"ModelName", "PlatformVersion"}}
    assert values.get("ModelName") == "SQC100-4", "Unexpected phone model"
    manifest = json.loads((args.runtime / "runtime-manifest.json").read_text())
    files = {}
    for entry in manifest["files"]:
        name = entry["path"]
        assert not Path(name).is_absolute() and ".." not in Path(name).parts
        digest = hashlib.sha256((args.runtime / name).read_bytes()).hexdigest()
        assert digest == entry["sha256"], name
        files[name] = digest

    client = paramiko.SSHClient()
    client.load_host_keys(str(args.known_hosts))
    client.set_missing_host_key_policy(paramiko.RejectPolicy())
    client.connect(args.host, username="root",
                   pkey=paramiko.RSAKey.from_private_key_file(str(args.identity)),
                   look_for_keys=False, allow_agent=False, timeout=8,
                   auth_timeout=15, banner_timeout=10,
                   disabled_algorithms={"pubkeys": ["rsa-sha2-512", "rsa-sha2-256"]})
    remote = "/var/tmp/berrygram-runtime-check-" + uuid.uuid4().hex
    cases = {}
    details = {}

    def command(binary):
        parts = ["on", "-u", "devuser", "env", "LD_LIBRARY_PATH=" + remote + "/lib",
                 remote + "/bin/" + binary]
        if binary == "berrygram-tdjson":
            parts += ["--library", remote + "/lib/libtdjson.so"]
        return " ".join(map(shlex.quote, parts))

    try:
        with client.open_sftp() as sftp:
            sftp.mkdir(remote, 0o755)
            sftp.mkdir(remote + "/bin", 0o755)
            sftp.mkdir(remote + "/lib", 0o755)
            for name in files:
                sftp.put(str(args.runtime / name), remote + "/" + name)
                sftp.chmod(remote + "/" + name, 0o755)
        for binary, case in [("cxx17-probe", "cxx17_and_libc"),
                             ("openssl-probe", "openssl_rng_sha256_tls_context")]:
            stdin, stdout, stderr = client.exec_command(command(binary), timeout=25)
            output = stdout.read(65536).decode("utf-8", "replace")
            errors = stderr.read(65536).decode("utf-8", "replace")
            code = stdout.channel.recv_exit_status()
            stdin.close(); stdout.close(); stderr.close()
            assert code == 0, (binary, code, errors)
            if binary == "openssl-probe":
                assert "entropy, SHA-256 and TLS context OK" in output
            cases[case] = "passed"
            details[case] = {"exit_code": code, "stdout": output, "stderr": errors}
            print(case + ": passed", flush=True)

        channel = client.get_transport().open_session(timeout=15)
        channel.exec_command(command("berrygram-tdjson"))
        requests = [
            {"@type": "getOption", "name": "version", "@extra": "classic-version"},
            {"@type": "getTextEntities", "text": "Привет 😀 https://example.org",
             "@extra": "classic-unicode"},
        ]
        payload = ("\n".join(json.dumps(item, ensure_ascii=False) for item in requests) + "\n").encode()
        for block in [payload[:7], payload[7:94], payload[94:]]:
            channel.sendall(block)
        output = b""
        errors = b""
        replies = []
        consumed = 0
        closed_input = False
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if channel.recv_ready():
                output += channel.recv(65536)
            if channel.recv_stderr_ready():
                errors += channel.recv_stderr(65536)
            lines = output.split(b"\n")
            for line in lines[consumed:-1]:
                if line:
                    replies.append(json.loads(line))
            consumed = len(lines) - 1
            extra = {item.get("@extra") for item in replies}
            if {"classic-version", "classic-unicode"} <= extra and not closed_input:
                channel.shutdown_write()
                closed_input = True
            if channel.exit_status_ready() and not channel.recv_ready() and not channel.recv_stderr_ready():
                break
            time.sleep(0.05)
        assert channel.exit_status_ready(), "TDLib process did not finish"
        code = channel.recv_exit_status()
        channel.close()
        assert code == 0, (code, errors.decode("utf-8", "replace"))
        version = next(item for item in replies if item.get("@extra") == "classic-version")
        assert version["@type"] == "optionValueString" and version["value"] == "1.8.69"
        entities = next(item for item in replies if item.get("@extra") == "classic-unicode")
        assert entities["@type"] == "textEntities"
        assert len(entities["entities"]) == 1
        entity = entities["entities"][0]
        assert entity["offset"] == 10 and entity["length"] == 19
        assert entity["type"]["@type"] == "textEntityTypeUrl"
        states = {item["authorization_state"]["@type"] for item in replies
                  if item.get("@type") == "updateAuthorizationState"}
        assert {"authorizationStateWaitTdlibParameters", "authorizationStateClosed"} <= states
        assert not any(item.get("@type") == "error" for item in replies)
        cases["tdlib_json_unicode_close"] = "passed"
        details["tdlib_json_unicode_close"] = {
            "exit_code": code, "replies": replies, "stderr": errors.decode("utf-8", "replace"),
        }
        print("tdlib_json_unicode_close: passed", flush=True)
        report = {
            "device": {"model": values["ModelName"], "os_version": values["PlatformVersion"]},
            "files": files, "cases": cases, "details": details,
            "telegram_login_tested": False,
            "tested_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        }
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        args.report.chmod(0o600)
    finally:
        try:
            with client.open_sftp() as sftp:
                for name in files:
                    try:
                        sftp.remove(remote + "/" + name)
                    except OSError:
                        pass
                for name in ["bin", "lib", ""]:
                    try:
                        sftp.rmdir(remote + ("/" + name if name else ""))
                    except OSError:
                        pass
        finally:
            client.close()


if __name__ == "__main__":
    main()
