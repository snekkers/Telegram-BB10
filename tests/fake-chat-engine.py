#!/usr/bin/python3
"""Synthetic TDLib peer: list/history races and send outcomes, no network."""
import json
import sys
import base64
import shutil
from pathlib import Path

assert Path(sys.argv[sys.argv.index("--library") + 1]).is_file()
A, B, C = -1001234567890, 9007199254740991, -1001234567891
USER = 9007199254740990
loaded = False
hold_next_history = False
held_history = []
held_sends = []
held_media = []
sent_count = 0
last_only_local = False
last_send = None
library_path = Path(sys.argv[sys.argv.index("--library") + 1])

def media_file(completed=False, path=""):
    return {"@type": "file", "id": 7, "size": 68, "expected_size": 68,
            "local": {"path": path, "is_downloading_completed": completed, "is_downloading_active": False,
                      "downloaded_size": 68 if completed else 0}}


def send(value):
    print(json.dumps(value, ensure_ascii=False), flush=True)


def reply(query, kind="ok", **values):
    send({"@type": kind, "@extra": query["@extra"], **values})


def content(text):
    return {"@type": "messageText", "text": {"@type": "formattedText", "text": text, "entities": []}}


def message(mid, chat=A, text=None, outgoing=False, state=None):
    return {"@type": "message", "id": mid, "chat_id": chat,
            "sender_id": {"@type": "messageSenderUser", "user_id": USER},
            "date": 1700000000 + mid, "edit_date": 0, "is_outgoing": outgoing,
            "sending_state": state, "content": content(text if text is not None else f"Текст {mid} 😀")}


def positions(order, name="chatListMain"):
    return [{"@type": "chatPosition", "list": {"@type": name}, "order": str(order), "is_pinned": False}]


def chat(cid, title, order, can_send=True):
    return {"@type": "chat", "id": cid, "title": title, "positions": positions(order),
            "type": {"@type": "chatTypePrivate" if cid == B else "chatTypeSupergroup"},
            "permissions": {"@type": "chatPermissions", "can_send_basic_messages": can_send, "can_send_documents": can_send, "can_send_photos": can_send, "can_send_voice_notes": can_send},
            "unread_count": 3, "last_read_outbox_message_id": 0, "view_as_topics": False,
            "last_message": message(30, cid)}


def history(query):
    global last_only_local
    cid, start = query["chat_id"], query["from_message_id"]
    assert isinstance(cid, int) and isinstance(start, int), "int53 requests must be JSON numbers"
    assert query["limit"] == 50 and isinstance(query["only_local"], bool)
    last_only_local = query["only_local"]
    if cid == A:
        if start == 0:
            photo = message(20)
            photo["content"] = {"@type": "messagePhoto", "caption": {"text": "Подпись фото"},
                "photo": {"sizes": [{"type": "m", "width": 1, "height": 1, "photo": media_file()}]}}
            rows = [message(30), photo, message(10)]
        elif start == 10:
            rows = [message(10), message(5)]
        else:
            rows = [message(5)]
    else:
        rows = [message(90, cid, "Другая переписка")]
    reply(query, "messages", total_count=900, messages=rows)


send({"@type": "updateAuthorizationState", "authorization_state": {"@type": "authorizationStateWaitTdlibParameters"}})
for line in sys.stdin:
    query = json.loads(line)
    kind = query["@type"]
    if kind == "getOption":
        reply(query, "optionValueString", value="1.8.69")
    elif kind == "getMe":
        reply(query, "user", id=USER, first_name="Fixture", last_name="")
    elif kind == "getMessage":
        reply(query, "error", code=404, message="Fixture no longer has the temporary message")
    elif kind == "setTdlibParameters":
        assert query["api_id"] == 12345 and query["api_hash"] == "a" * 32
        send({"@type": "updateAuthorizationState", "authorization_state": {"@type": "authorizationStateReady"}})
        send({"@type": "updateConnectionState", "state": {"@type": "connectionStateReady"}})
        reply(query)
    elif kind == "setOption":
        assert query["name"] == "use_quick_ack"
        assert query["value"] == {"@type": "optionValueBoolean", "value": True}
        reply(query)
    elif kind == "loadChats":
        assert query["chat_list"] == {"@type": "chatListMain"}
        if loaded:
            reply(query, "error", code=404, message="All chats loaded")
        else:
            loaded = True
            send({"@type": "updateUser", "user": {"id": USER, "first_name": "Анна", "last_name": "😀"}})
            for row in [chat(C, "Только чтение", 9223372036854775805, False),
                        chat(B, "Личное", 9223372036854775805),
                        chat(A, "Группа 😀", 9223372036854775806), chat(44, "Архив", 0)]:
                send({"@type": "updateNewChat", "chat": row})
            reply(query)
    elif kind in ("openChat", "closeChat"):
        assert query["chat_id"] in (A, B, C) and isinstance(query["chat_id"], int)
        reply(query)
    elif kind == "setChatDraftMessage":
        assert query["chat_id"] in (A, B, C)
        draft = query["draft_message"]
        if draft is not None:
            assert draft["content"]["@type"] == "draftMessageContentText"
            assert draft["content"]["text"]["@type"] == "formattedText"
        send({"@type": "updateChatDraftMessage", "chat_id": query["chat_id"],
              "draft_message": draft, "positions": positions(9223372036854775806)})
        reply(query)
    elif kind == "viewMessages":
        assert query["source"] == {"@type": "messageSourceChatHistory"} and query["force_read"] is True
        assert query["message_ids"]
        send({"@type": "updateChatReadInbox", "chat_id": query["chat_id"],
              "last_read_inbox_message_id": query["message_ids"][-1], "unread_count": 0})
        reply(query)
    elif kind == "downloadFile":
        assert query["file_id"] == 7 and query["limit"] == 50 * 1024 * 1024 and query["synchronous"] is False
        directory = library_path.parent / "private/files"
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / "photo.png"
        path.write_bytes(base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aS1sAAAAASUVORK5CYII="))
        value = media_file(True, str(path))
        send({"@type": "updateFile", "file": value})
        reply(query, kind="file", **{k: v for k, v in value.items() if k != "@type"})
    elif kind == "getChatHistory":
        if hold_next_history:
            hold_next_history = False
            held_history.append(query)
        else:
            history(query)
    elif kind == "sendMessage":
        sent_count += 1
        assert query["chat_id"] in (A, B) and isinstance(query["chat_id"], int)
        assert query["options"]["paid_message_star_count"] == 0
        body = query["input_message_content"]
        if body["@type"] != "inputMessageText":
            types = {"inputMessageDocument": ("document", "inputDocument", "messageDocument"),
                     "inputMessagePhoto": ("photo", "inputPhoto", "messagePhoto"),
                     "inputMessageVoiceNote": ("voice_note", "inputVoiceNote", "messageVoiceNote")}
            field, nested_type, result_type = types[body["@type"]]
            nested = body[field]
            assert nested["@type"] == nested_type and nested[field]["@type"] == "inputFileLocal"
            path = Path(nested[field]["path"])
            assert path.is_file() and path.resolve().is_relative_to((library_path.parent / "private/uploads").resolve())
            assert body["caption"]["@type"] == "formattedText" and body["caption"]["entities"] == []
            caption = body["caption"]["text"]
            if field == "voice_note":
                assert 0 < nested["duration"] <= 300 and nested["waveform"] == ""
            if field == "document":
                assert nested["disable_content_type_detection"] is True
            if caption == "deny":
                reply(query, "error", code=403, message="fixture attachment rejected")
                continue
            saved = library_path.parent / "private/files" / (str(sent_count) + "-" + path.name)
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, saved)
            file = {"@type": "file", "id": 100 + sent_count, "size": saved.stat().st_size,
                    "local": {"path": str(saved), "is_downloading_completed": True},
                    "remote": {"uploaded_size": saved.stat().st_size, "is_uploading_active": False}}
            if field == "document":
                media = {"document": file, "file_name": path.name, "mime_type": "application/octet-stream"}
            elif field == "photo":
                assert nested["width"] > 0 and nested["height"] > 0
                media = {"sizes": [{"width": nested["width"], "height": nested["height"], "photo": file}]}
            else:
                media = {"voice": file, "duration": nested["duration"], "mime_type": "audio/mp4" if path.suffix == ".m4a" else "audio/ogg"}
            mid = 6000 + sent_count
            outgoing = message(mid, query["chat_id"], "", True, {"@type": "messageSendingStatePending"})
            outgoing["content"] = {"@type": result_type, field: media, "caption": body["caption"]}
            if caption == "timeout-media":
                held_media.append((query, outgoing))
                continue
            if caption == "fail-before-response":
                failed = {**outgoing, "sending_state": {"@type": "messageSendingStateFailed", "error": {"code": 403}}}
                send({"@type": "updateMessageSendFailed", "old_message_id": mid, "message": failed, "error": {"code": 403}})
                reply(query, kind="message", **{k: v for k, v in outgoing.items() if k != "@type"})
                continue
            reply(query, kind="message", **{k: v for k, v in outgoing.items() if k != "@type"})
            if caption == "fail-later":
                outgoing["sending_state"] = {"@type": "messageSendingStateFailed", "error": {"code": 403}}
                send({"@type": "updateMessageSendFailed", "old_message_id": mid, "message": outgoing, "error": {"code": 403}})
            else:
                send({"@type": "updateMessageSendAcknowledged", "chat_id": query["chat_id"], "message_id": mid})
                outgoing["sending_state"] = None
                send({"@type": "updateMessageSendSucceeded", "old_message_id": mid, "message": outgoing})
            continue
        assert body["@type"] == "inputMessageText"
        assert body["text"]["@type"] == "formattedText" and body["text"]["entities"] == []
        assert body["clear_draft"] is False
        text = body["text"]["text"]
        last_send = (query, text)
        if text == "deny":
            reply(query, "error", code=403, message="private server diagnostic")
        elif text == "timeout":
            held_sends.append(query)
        elif text == "failed-race":
            send({"@type": "updateMessageSendFailed", "old_message_id": 4000,
                  "message": message(4000, query["chat_id"], text, True,
                  {"@type": "messageSendingStateFailed", "error": {"code": 403}}), "error": {"code": 403}})
            pending = message(4000, query["chat_id"], text, True, {"@type": "messageSendingStatePending"})
            reply(query, kind="message", **{k: v for k, v in pending.items() if k != "@type"})
        elif text == "ack-race":
            send({"@type": "updateMessageSendAcknowledged", "chat_id": query["chat_id"], "message_id": 5000})
            pending = message(5000, query["chat_id"], text, True, {"@type": "messageSendingStatePending"})
            reply(query, kind="message", **{k: v for k, v in pending.items() if k != "@type"})
        elif text == "race":
            send({"@type": "updateMessageSendSucceeded", "old_message_id": 2000,
                  "message": message(2001, query["chat_id"], text, True)})
            reply(query, **{k: v for k, v in message(2000, query["chat_id"], text, True,
                  {"@type": "messageSendingStatePending"}).items() if k != "@type"}, kind="message")
        else:
            pending = message(1000, query["chat_id"], text, True, {"@type": "messageSendingStatePending"})
            reply(query, kind="message", **{k: v for k, v in pending.items() if k != "@type"})
            send({"@type": "updateMessageSendAcknowledged", "chat_id": query["chat_id"], "message_id": 1000})
    elif kind == "fixtureControl":
        op = query["op"]
        if op == "holdHistory":
            hold_next_history = True
        elif op == "releaseHistory":
            for request in held_history:
                history(request)
            held_history.clear()
        elif op == "emit":
            send(query["event"])
        elif op == "finishSend":
            request, text = last_send
            send({"@type": "updateMessageSendSucceeded", "old_message_id": 1000,
                  "message": message(1001, request["chat_id"], text, True)})
        elif op == "failSend":
            request, text = last_send
            send({"@type": "updateMessageSendFailed", "old_message_id": 1000,
                  "message": message(1000, request["chat_id"], text, True,
                  {"@type": "messageSendingStateFailed", "error": {"code": 403}}),
                  "error": {"code": 403, "message": "private diagnostic"}})
        elif op == "crash":
            reply(query, "fixtureStats", send_count=sent_count)
            sys.exit(0)
        elif op == "releaseMedia":
            for request, outgoing in held_media:
                outgoing["sending_state"] = None
                reply(request, kind="message", **{k:v for k,v in outgoing.items() if k != "@type"})
            held_media.clear()
        elif op == "releaseSend":
            for request in held_sends:
                final = message(3001, request["chat_id"], "timeout", True)
                reply(request, kind="message", **{k: v for k, v in final.items() if k != "@type"})
            held_sends.clear()
        else:
            assert op == "stats", op
        reply(query, "fixtureStats", send_count=sent_count, last_only_local=last_only_local)
    else:
        raise AssertionError(kind)

send({"@type": "updateAuthorizationState", "authorization_state": {"@type": "authorizationStateClosed"}})
