#!/usr/bin/python3
"""Synthetic TDLib peer: list/history races and send outcomes, no network."""
import json
import sys
import base64
import shutil
import time
from pathlib import Path

assert Path(sys.argv[sys.argv.index("--library") + 1]).is_file()
A, B, C = -1001234567890, 9007199254740991, -1001234567891
USER = 9007199254740990
PUBLIC = -1001234567899
archive_loaded = False
actions = []
only_all_delete = False
pinned_enabled = False
reaction_chosen = False
forward_race = False
people_calls=[]
held_contact_open=[]
hold_contact=False
loaded = False
held_search = []
search_calls = []
hold_next_history = False
held_history = []
held_threads = []
hold_thread = False
thread_readonly = False
thread_unknown = False
thread_draft = None
held_sends = []
held_media = []
sent_count = 0
last_only_local = False
last_send = None
scheduled_text = "Later fixture"
scheduled_date = int(time.time())+3600
scheduled_cancelled = False
hold_download = False
held_downloads = []
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
            "type": {"@type": "chatTypePrivate", "user_id": USER} if cid == B else {"@type": "chatTypeSupergroup", "supergroup_id": 1234, "is_channel": False},
            "notification_settings": {"@type": "chatNotificationSettings", "use_default_mute_for": True, "mute_for": 0, "use_default_sound": False, "sound_id": "9223372036854775806", "show_preview": False, "use_default_show_preview": False},
            "permissions": {"@type": "chatPermissions", "can_send_basic_messages": can_send, "can_send_documents": can_send, "can_send_photos": can_send, "can_send_voice_notes": can_send},
            "unread_count": 3, "last_read_outbox_message_id": 0, "view_as_topics": False,
            "last_message": message(30, cid)}


def history(query):
    global last_only_local
    cid, start = query["chat_id"], query["from_message_id"]
    assert isinstance(cid, int) and isinstance(start, int), "int53 requests must be JSON numbers"
    assert query["limit"] == 50 and isinstance(query["only_local"], bool)
    last_only_local = query["only_local"]
    if start > 0 and query["offset"] == -20:
        rows = [message(start + 1, cid), message(start, cid), message(start - 1, cid)]
    elif cid == A:
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
    elif kind == "getContacts":
        people_calls.append(query)
        for uid,name,username,status in [(42,"Анна","anna",{"@type":"userStatusOnline","expires":int(time.time())+3600}),(84,"Борис","boris",{"@type":"userStatusRecently"})]:
            send({"@type":"updateUser","user":{"@type":"user","id":uid,"first_name":name,"last_name":"","is_contact":True,"usernames":{"editable_username":username},"status":status,"profile_photo":{"small":{"@type":"file","id":77 if uid==42 else 78,"size":68,"local":{"is_downloading_completed":False}}}}})
        reply(query,"users",total_count=3,user_ids=[42,84,85])
    elif kind == "getUser":
        people_calls.append(query);assert query["user_id"]==85
        reply(query,"user",id=85,first_name="Катя",last_name="",is_contact=True,status={"@type":"userStatusEmpty"},type={"@type":"userTypeRegular"})
    elif kind == "createPrivateChat":
        people_calls.append(query);assert query["user_id"] in (42,84,85) and query["force"] is False
        value=chat(420000+query["user_id"],"Новый диалог",0);value["type"]={"@type":"chatTypePrivate","user_id":query["user_id"]}
        if hold_contact:hold_contact=False;held_contact_open.append((query,value))
        else:reply(query,kind="chat",**{k:v for k,v in value.items() if k!="@type"})
    elif kind == "getMe":
        reply(query, "user", id=USER, first_name="Fixture", last_name="")
    elif kind in ("searchPublicChats", "searchChats", "searchChatsOnServer", "searchMessages"):
        search_calls.append(query)
        if query["query"] == "slow":
            held_search.append(query)
            continue
        if query["query"] == "deny":
            reply(query, "error", code=429, message="private diagnostic")
            continue
        if kind == "searchMessages":
            assert query["chat_list"] is None and query["chat_type_filter"] is None
            assert query["limit"] == 30 and query["min_date"] == 0 and query["max_date"] == 0
            assert query["filter"]["@type"] in ("searchMessagesFilterEmpty", "searchMessagesFilterPhoto", "searchMessagesFilterDocument", "searchMessagesFilterUrl")
            if query["offset"] == "":
                reply(query, "foundMessages", total_count=3, messages=[message(555, PUBLIC, "Needle public"), message(20, A, "Needle group")], next_offset="page2")
            else:
                assert query["offset"] == "page2"
                reply(query, "foundMessages", total_count=3, messages=[message(20, A), message(10, B, "Needle private")], next_offset="")
        else:
            assert query["type_filter"] is None
            ids = [PUBLIC] if kind == "searchPublicChats" else [B] if kind == "searchChats" else [B, A]
            reply(query, "chats", total_count=len(ids), chat_ids=ids)
    elif kind == "getChat":
        assert query["chat_id"] in (PUBLIC,-1001234567800)
        value = chat(query["chat_id"], "Обсуждение" if query["chat_id"] != PUBLIC else "Публичный канал", 0, query["chat_id"] != PUBLIC)
        value["type"] = {"@type": "chatTypeSupergroup", "is_channel": query["chat_id"] == PUBLIC}
        reply(query, kind="chat", **{k:v for k,v in value.items() if k != "@type"})
    elif kind == "getMessage":
        reply(query, "error", code=404, message="Fixture no longer has the temporary message")
    elif kind == "setTdlibParameters":
        assert query["api_id"] == 12345 and query["api_hash"] == "a" * 32
        send({"@type": "updateAuthorizationState", "authorization_state": {"@type": "authorizationStateReady"}})
        send({"@type": "updateConnectionState", "state": {"@type": "connectionStateReady"}})
        reply(query)
    elif kind == "setOption":
        assert query["name"] in ("use_quick_ack", "online")
        assert query["value"]["@type"] == "optionValueBoolean"
        reply(query)
    elif kind == "loadChats":
        if query["chat_list"] == {"@type": "chatListArchive"}:
            if archive_loaded:reply(query,"error",code=404,message="All archived chats loaded")
            else:
                archive_loaded=True
                value=chat(44,"Архив",0);value["positions"]=positions(20,"chatListArchive")
                send({"@type":"updateNewChat","chat":value});reply(query)
            continue
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
        assert query["chat_id"] in (A, B, C, PUBLIC,-1001234567800,420042,420084,420085) and isinstance(query["chat_id"], int)
        reply(query)
    elif kind == "setChatDraftMessage":
        assert query["chat_id"] in (A, B, C,-1001234567800)
        if query.get("topic_id"):actions.append(query)
        draft = query["draft_message"]
        if query.get("topic_id"):thread_draft=draft
        if draft is not None:
            assert draft["content"]["@type"] == "draftMessageContentText"
            assert draft["content"]["text"]["@type"] == "formattedText"
        if not query.get("topic_id"):send({"@type": "updateChatDraftMessage", "chat_id": query["chat_id"],
              "draft_message": draft, "positions": positions(9223372036854775806)})
        reply(query)
    elif kind == "viewMessages":
        assert query["source"]["@type"] in ("messageSourceChatHistory","messageSourceMessageThreadHistory") and query["force_read"] is True
        if query["source"]["@type"]=="messageSourceMessageThreadHistory":actions.append(query);reply(query);continue
        assert query["message_ids"]
        send({"@type": "updateChatReadInbox", "chat_id": query["chat_id"],
              "last_read_inbox_message_id": query["message_ids"][-1], "unread_count": 0})
        reply(query)
    elif kind == "downloadFile":
        if hold_download and query["file_id"]==7:
            held_downloads.append(query);continue
        if query["file_id"] in (77,78):
            people_calls.append(query);assert query["limit"]==1024*1024 and query["priority"]==8 and query["offset"]==0 and query["synchronous"] is False
            directory=library_path.parent/"private/files";directory.mkdir(parents=True,exist_ok=True)
            path=directory/("avatar-"+str(query["file_id"])+".png")
            path.write_bytes(base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aS1sAAAAASUVORK5CYII="))
            value={"id":query["file_id"],"size":68,"local":{"path":str(path),"is_downloading_completed":True,"is_downloading_active":False}}
            send({"@type":"updateFile","file":{"@type":"file",**value}});reply(query,"file",**value)
            continue
        assert query["file_id"] == 7 and query["limit"] == 50 * 1024 * 1024 and query["synchronous"] is False
        directory = library_path.parent / "private/files"
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / "photo.png"
        path.write_bytes(base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aS1sAAAAASUVORK5CYII="))
        value = media_file(True, str(path))
        send({"@type": "updateFile", "file": value})
        reply(query, kind="file", **{k: v for k, v in value.items() if k != "@type"})
    elif kind == "fixtureOnlyAllDelete":
        only_all_delete=True;reply(query)
    elif kind == "getMessageAvailableReactions":
        reaction={"@type":"availableReaction","type":{"@type":"reactionTypeEmoji","emoji":"👍"},"needs_premium":False}
        premium={"@type":"availableReaction","type":{"@type":"reactionTypeEmoji","emoji":"❤️"},"needs_premium":True}
        reply(query,"availableReactions",top_reactions=[reaction,premium],recent_reactions=[reaction],popular_reactions=[],allow_custom_emoji=False,are_tags=False)
    elif kind in ("addMessageReaction","removeMessageReaction"):
        assert query["chat_id"]==A and query["message_id"]==30 and query["reaction_type"]=={"@type":"reactionTypeEmoji","emoji":"👍"}
        reaction_chosen=kind=="addMessageReaction";actions.append(query)
        reaction={"@type":"messageReaction","type":{"@type":"reactionTypeEmoji","emoji":"👍"},"total_count":1 if reaction_chosen else 0,"is_chosen":reaction_chosen}
        info={"@type":"messageInteractionInfo","reactions":{"@type":"messageReactions","reactions":[reaction] if reaction_chosen else [],"can_get_added_reactions":True}}
        send({"@type":"updateMessageInteractionInfo","chat_id":A,"message_id":30,"interaction_info":info});reply(query)
    elif kind == "getMessageAddedReactions":
        assert query["reaction_type"] is None and query["limit"]==50
        sender={"@type":"messageSenderUser","user_id":42};reaction={"@type":"addedReaction","sender_id":sender,"type":{"@type":"reactionTypeEmoji","emoji":"👍"}}
        reply(query,"addedReactions",total_count=1,reactions=[reaction],next_offset="")
    elif kind == "getChatPinnedMessage":
        if pinned_enabled and query["chat_id"] == A:
            value=message(30,A,"Pinned fixture");reply(query,"message",**{k:v for k,v in value.items() if k!="@type"})
        else:reply(query,"error",code=404,message="No pinned message")
    elif kind == "getStorageStatistics":
        assert query["chat_limit"] == 0
        rows=[{"@type":"storageStatisticsByFileType","file_type":{"@type":"fileTypePhoto"},"size":2097152,"count":2},{"@type":"storageStatisticsByFileType","file_type":{"@type":"fileTypeDocument"},"size":1048576,"count":1}]
        reply(query,"storageStatistics",size=3145728,count=3,by_chat=[{"@type":"storageStatisticsByChat","chat_id":0,"size":3145728,"count":3,"by_file_type":rows}])
    elif kind == "getChatScheduledMessages":
        actions.append(query)
        value=message(7100,query["chat_id"],scheduled_text,True)
        value["scheduling_state"]={"@type":"messageSchedulingStateSendAtDate","send_date":scheduled_date,"repeat_period":0}
        reply(query,"messages",total_count=0 if scheduled_cancelled else 1,messages=[] if scheduled_cancelled else [value])
    elif kind == "editMessageSchedulingState":
        actions.append(query);assert query["message_id"]==7100
        scheduled_date=query["scheduling_state"]["send_date"];reply(query)
    elif kind == "cancelDownloadFile":
        actions.append(query);assert query["only_if_pending"] is False;reply(query)
    elif kind == "getMessageProperties":
        actions.append(query)
        reply(query,"messageProperties",can_get_message_thread=query["message_id"]!=999,can_be_replied=True,can_be_forwarded=query["message_id"]!=999,can_be_edited=query["message_id"]==50,can_be_saved=True,can_be_deleted_only_for_self=query["message_id"]!=999 and not only_all_delete,can_be_deleted_for_all_users=query["message_id"]==50)
    elif kind == "deleteMessages":
        actions.append(query)
        if query["message_ids"]==[7100]:
            assert query["revoke"] is True;scheduled_cancelled=True;reply(query);continue
        if only_all_delete:assert query["revoke"] is True
        assert query["chat_id"]==A and query["message_ids"] and isinstance(query["revoke"],bool)
        if query["revoke"]:assert query["message_ids"]==[50]
        send({"@type":"updateDeleteMessages","chat_id":A,"message_ids":query["message_ids"],"is_permanent":True,"from_cache":False})
        reply(query)
    elif kind == "editMessageText":
        actions.append(query)
        if query["message_id"]==7100:
            assert query["reply_markup"] is None
            scheduled_text=query["input_message_content"]["text"]["text"]
            value=message(7100,query["chat_id"],scheduled_text,True);value["scheduling_state"]={"@type":"messageSchedulingStateSendAtDate","send_date":scheduled_date,"repeat_period":0}
            reply(query,"message",**{k:v for k,v in value.items() if k!="@type"});continue
        assert query["message_id"]==50 and query["reply_markup"] is None
        body=query["input_message_content"];assert body["@type"]=="inputMessageText"
        value=message(50,query["chat_id"],body["text"]["text"],True);value["edit_date"]=1700009999
        reply(query,kind="message",**{k:v for k,v in value.items() if k!="@type"})
    elif kind == "forwardMessages":
        actions.append(query)
        assert query["message_ids"] in ([30],[30,50]) and query["from_chat_id"]==A and query["chat_id"]==B
        assert query["send_copy"] is False and query["remove_caption"] is False and query["options"]["paid_message_star_count"]==0
        value=message(8800,B,"Forwarded",True);value["forward_info"]={"origin":{"@type":"messageOriginUser","sender_user_id":USER}}
        if forward_race:
            forward_race=False
            final={**value,"id":8801}
            send({"@type":"updateMessageSendSucceeded","old_message_id":8800,"message":final})
            value["sending_state"]={"@type":"messageSendingStatePending"}
        reply(query,"messages",total_count=len(query["message_ids"]),messages=[value] if len(query["message_ids"])==1 else [value,message(8802,B,"Forwarded 2",True)])
    elif kind in ("toggleChatIsPinned","addChatToList"):
        actions.append(query)
        if kind=="toggleChatIsPinned":
            value=positions(9223372036854775807,query["chat_list"]["@type"])[0];value["is_pinned"]=query["is_pinned"]
            send({"@type":"updateChatPosition","chat_id":query["chat_id"],"position":value})
        else:
            target=query["chat_list"]["@type"];other="chatListMain" if target=="chatListArchive" else "chatListArchive"
            for name,order in [(other,0),(target,100)]:send({"@type":"updateChatPosition","chat_id":query["chat_id"],"position":positions(order,name)[0]})
        reply(query)
    elif kind == "getSupergroupFullInfo":
        actions.append(query);reply(query,"supergroupFullInfo",description="Synthetic group description",member_count=2,can_get_members=True)
    elif kind == "getUserFullInfo":
        reply(query,"userFullInfo",bio={"text":"Synthetic bio"})
    elif kind == "getScopeNotificationSettings":
        actions.append(query);reply(query,"scopeNotificationSettings",mute_for=0)
    elif kind == "getSupergroupMembers":
        actions.append(query);assert query["limit"]==50 and query["filter"] is None
        send({"@type":"updateUser","user":{"id":42,"first_name":"Участник","last_name":""}})
        members=[{"member_id":{"@type":"messageSenderUser","user_id":uid},"status":{"@type":"chatMemberStatusMember"}} for uid in [USER,42]] if query["offset"]==0 else []
        reply(query,"chatMembers",total_count=2,members=members)
    elif kind == "setChatNotificationSettings":
        actions.append(query);assert query["notification_settings"]["sound_id"]=="9223372036854775806"
        assert query["notification_settings"]["show_preview"] is False
        send({"@type":"updateChatNotificationSettings","chat_id":query["chat_id"],"notification_settings":query["notification_settings"]});reply(query)
    elif kind == "searchChatMessages":
        actions.append(query);assert query["query"]=="" and query["sender_id"] is None and query["topic_id"] is None
        photo=message(20,query["chat_id"]);photo["content"]={"@type":"messagePhoto","caption":{"text":"Общее фото"},"photo":{"sizes":[{"width":1,"height":1,"photo":media_file()}]}}
        document=message(25,query["chat_id"]);document["content"]={"@type":"messageDocument","caption":{"text":"Файл"},"document":{"file_name":"note.txt","document":media_file()}}
        rows=[photo] if query["filter"]["@type"]=="searchMessagesFilterPhoto" else [document]
        reply(query,"foundChatMessages",total_count=1,messages=rows,next_from_message_id=0)
    elif kind == "getMessageThread":
        actions.append(query)
        if hold_thread:
            held_threads.append(query)
        else:
            cid=-1001234567800 if thread_unknown else A
            permissions={"@type":"chatPermissions","can_send_basic_messages":not thread_readonly,"can_send_photos":not thread_readonly,"can_send_documents":not thread_readonly,"can_send_voice_notes":not thread_readonly}
            if not thread_unknown:send({"@type":"updateChatPermissions","chat_id":cid,"permissions":permissions})
            reply(query,"messageThreadInfo",chat_id=cid,message_thread_id=300,messages=[message(300,cid,"Публикация")],reply_info={"reply_count":2},draft_message=thread_draft)
    elif kind == "getMessageThreadHistory":
        actions.append(query)
        assert query["message_id"]==300 and query["limit"]==50 and "only_local" not in query
        rows=[] if query["from_message_id"] else [message(330,query["chat_id"],"Комментарий 2"),message(320,query["chat_id"],"Комментарий 1")]
        for value in rows:value["topic_id"]={"@type":"messageTopicThread","message_thread_id":300}
        reply(query,"messages",messages=rows,total_count=2)
    elif kind == "getChatHistory":
        if hold_next_history:
            hold_next_history = False
            held_history.append(query)
        else:
            history(query)
    elif kind == "sendMessage":
        sent_count += 1
        if query.get("topic_id"):
            actions.append(query)
            assert query["chat_id"] in (A,-1001234567800) and query["topic_id"]=={"@type":"messageTopicThread","message_thread_id":300}
            assert query["reply_to"]["message_id"] in (300,320,330)
            text=query["input_message_content"]["text"]["text"]
            if text=="deny":reply(query,"error",code=403,message="thread denied");continue
            outgoing=message(340,query["chat_id"],text,True)
            outgoing["topic_id"]=query["topic_id"]
            reply(query,kind="message",**{k:v for k,v in outgoing.items() if k!="@type"})
            continue
        assert query["chat_id"] in (A, B) and isinstance(query["chat_id"], int)
        assert query["options"]["paid_message_star_count"] == 0
        body = query["input_message_content"]
        if "reply_to" in query:
            actions.append(query)
            assert query["reply_to"]["@type"]=="inputMessageReplyToMessage"
            assert query["reply_to"]["message_id"]==30
            assert query["reply_to"]["quote"]["text"]["text"]

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
        if query["options"].get("scheduling_state"):
            schedule = query["options"]["scheduling_state"]
            assert schedule["@type"] == "messageSchedulingStateSendAtDate" and schedule["send_date"] > int(time.time()) and schedule["repeat_period"] == 0
            actions.append(query)
            if text == "scheduled-deny":
                reply(query, "error", code=403, message="fixture scheduling rejected")
            else:
                pending = message(7000, query["chat_id"], text, True, {"@type": "messageSendingStatePending"})
                pending["scheduling_state"] = schedule
                reply(query, kind="message", **{k:v for k,v in pending.items() if k!="@type"})
                scheduled = message(7001, query["chat_id"], text, True)
                scheduled["scheduling_state"] = schedule
                send({"@type":"updateMessageSendSucceeded", "old_message_id":7000, "message":scheduled})
            continue
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
        if op == "holdThread":hold_thread=True
        elif op == "releaseThread":
            hold_thread=False
            for pending in held_threads:reply(pending,"messageThreadInfo",chat_id=A,message_thread_id=300,messages=[message(300,A)],draft_message=None)
            held_threads.clear()
        elif op == "readOnlyThread":thread_readonly=True
        elif op == "unknownThread":thread_unknown=True
        elif op == "holdDownload":
            hold_download=True
        elif op == "releaseDownload":
            hold_download=False
            for pending in held_downloads:reply(pending,"file",id=7,size=68,local={"is_downloading_completed":False,"is_downloading_active":False,"downloaded_size":0,"path":""})
            held_downloads.clear()
        elif op == "messageBurst":
            events=[{"@type":"updateNewMessage","message":message(10000+query.get("event",{}).get("base",0)+i,A)} for i in range(600)]
            sys.stdout.write("".join(json.dumps(e,ensure_ascii=False)+"\n" for e in events));sys.stdout.flush()
        elif op == "pinFixture":
            pinned_enabled=True
        elif op == "unpinFixture":
            pinned_enabled=False
        elif op == "holdContact":
            hold_contact=True
        elif op == "releaseContact":
            for request,value in held_contact_open:reply(request,kind="chat",**{k:v for k,v in value.items() if k!="@type"})
            held_contact_open.clear()
        elif op == "forwardRace":
            forward_race = True
        elif op == "releaseSearch":
            for request in held_search:
                reply(request, "chats", total_count=1, chat_ids=[C])
            held_search.clear()
        elif op == "holdHistory":
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
        reply(query, "fixtureStats", send_count=sent_count, last_only_local=last_only_local, search_calls=search_calls, actions=actions, people_calls=people_calls)
    else:
        raise AssertionError(kind)

send({"@type": "updateAuthorizationState", "authorization_state": {"@type": "authorizationStateClosed"}})
