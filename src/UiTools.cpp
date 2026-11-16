#include "UiTools.hpp"
#include "ChatController.hpp"
#include "TelegramController.hpp"
#include "SearchController.hpp"
#include "PrivateFile.hpp"
#include "AutoPager.hpp"
#include "TelegramSession.hpp"
#include "AvatarImages.hpp"
#include <bb/cascades/ActionItem>
#include <bb/cascades/ListScrollStateHandler>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/AbstractTextControl>
#include <bb/cascades/Application>
#include <bb/cascades/ImageButton>
#include <bb/cascades/Button>
#include <bb/cascades/CheckBox>
#include <bb/cascades/SegmentedControl>
#include <bb/cascades/TextField>
#include <bb/cascades/ListView>
#include <bb/cascades/NavigationPane>
#include <bb/cascades/Page>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/Shortcut>
#include <bb/cascades/TextArea>
#include <bb/cascades/TextAreaAutoSizeProperties>
#include <bb/cascades/TextEditor>
#include <bb/cascades/TitleBar>
#include <bb/cascades/Window>
#include <bb/cascades/ScrollView>
#include <bb/cascades/DropDown>
#include <bb/system/Clipboard>
#include <bb/platform/Notification>
#include <bps/navigator.h>
#include <QDir>
#include <QFile>
#include <QtGui/QImage>
#include <QRegExp>
#include <QUrl>
#include <screen/screen.h>

using namespace bb::cascades;

UiTools::UiTools(ChatController *demo, TelegramController *live, SearchController *search,TelegramSession *session,AvatarProvider *avatars,QObject *parent)
    : QObject(parent), m_demo(demo), m_live(live), m_search(search),m_session(session),m_avatars(avatars),m_resumeSaved(false),
      m_directory(QDir::currentPath()+"/data/ui-check"), m_checks(false), m_pending(false),m_lastSubmitMs(-1000)
{
    m_submitClock.start();m_viewTimer.setSingleShot(true);connect(&m_viewTimer,SIGNAL(timeout()),this,SLOT(flushViewState()));
#ifdef BERRYGRAM_UI_CHECKS
    m_checks = QFile::exists(m_directory+"/enabled");
    if (m_checks) {
        connect(&m_timer,SIGNAL(timeout()),this,SLOT(checkRequest()));
        connect(session,SIGNAL(changed()),this,SLOT(resumeSaved()));
        m_timer.start(750);
    }
#endif
}
void UiTools::resumeSaved(){
#ifdef BERRYGRAM_UI_CHECKS
    if(!m_resumeSaved)return;
    if(m_session->phase()=="parameters"){m_resumeSaved=false;m_session->useSavedConfiguration();}
    else if(m_session->phase()=="failed" || m_session->phase()=="ready")m_resumeSaved=false;
#endif
}
void UiTools::setRoot(AbstractPane *root) { m_root=root; }
void UiTools::openMenu(QObject *object) {
    Page *page=qobject_cast<Page*>(object);
    if(page) page->openActionMenu();
}
void UiTools::copyText(const QString &text) {
    if(text.isEmpty())return;
    bb::system::Clipboard clipboard;
    clipboard.clear();clipboard.insert("text/plain",text.toUtf8());
}
bool UiTools::acceptSubmit(QObject *page) {
    const qlonglong now=m_submitClock.elapsed();
    if(!page || (m_submitPage==page && now-m_lastSubmitMs<250))return false;
    m_submitPage=page;m_lastSubmitMs=now;return true;
}

void UiTools::scheduleViewState(QObject *page){if(!page || !page->property("liveChat").toBool())return;m_viewPage=page;m_viewTimer.start(2000);}
void UiTools::watchCursor(QObject *page,QObject *field){TextArea *area=qobject_cast<TextArea*>(field);if(!area)return;m_viewPage=page;connect(area->editor(),SIGNAL(cursorPositionChanged(int)),this,SLOT(queueCursorState()));}
void UiTools::queueCursorState(){if(m_viewPage)scheduleViewState(m_viewPage.data());}
void UiTools::flushViewState(){
    m_viewTimer.stop();NavigationPane *nav=qobject_cast<NavigationPane*>(m_root.data());QObject *page=nav?nav->top():0;
    if(page && page->objectName()=="chatPage" && page->property("liveChat").toBool())QMetaObject::invokeMethod(page,"saveViewState");
}
void UiTools::checkRequest()
{
#ifdef BERRYGRAM_UI_CHECKS
    if (!m_checks || m_pending || !m_root) return;
    QVariantMap request;
    if (!PrivateFile::read(m_directory+"/request.json",&request,8192)) return;
    QFile::remove(m_directory+"/request.json");
    m_id=request.value("id").toString();
    if (!QRegExp("[a-z0-9-]{1,48}").exactMatch(m_id)) return;
    m_result.clear();m_result.insert("id",m_id);m_result.insert("ok",false);
    // Checks are strictly offline; they cannot navigate or send in a real account.
    NavigationPane *navigation=qobject_cast<NavigationPane*>(m_root.data());
    const QString operation=request.value("action").toString();
    if(operation=="session-state" || operation=="resume-saved" || operation=="stop-session" || operation=="live-people" || operation=="disable-checks") {
        if(operation=="resume-saved" && m_session->hasSavedConfiguration()) {
            m_resumeSaved=true;
            if(m_session->phase()=="stopped")m_session->start();else resumeSaved();
        }
        if(operation=="stop-session"){m_resumeSaved=false;m_session->stop();}
        if(operation=="live-people" && m_live->ready())m_live->loadContacts();
        if(operation=="disable-checks"){m_resumeSaved=false;QFile::remove(m_directory+"/enabled");m_timer.stop();m_checks=false;}
        m_result.insert("ok",true);m_result.insert("metadata_only",true);
    } else if (m_live->ready() || !navigation) {
        m_result.insert("error","demo_only");
    } else {
        const QString action=request.value("action").toString();
        bool ok=false;
        if (action=="demo-list" && navigation->count()==1) {
            navigation->setProperty("demo",true);ok=true;
        } else if(action=="preferences" && navigation->count()==1){
            ok=QMetaObject::invokeMethod(navigation,"openSettings");
        } else if(action=="contacts" && navigation->count()==1){
            navigation->setProperty("demo",true);
            ImageButton *button=navigation->top()->titleBar()->findChild<ImageButton*>("newChatButton");
            ok=button && QMetaObject::invokeMethod(button,"clicked");
        } else if (action=="chat-tab" && navigation->count()==1) {
            SegmentedControl *tabs=navigation->top()->findChild<SegmentedControl*>("chatTabs");
            if(tabs){tabs->setSelectedIndex(request.value("index").toInt()==1?1:0);ok=true;}
        } else if(action=="search" && navigation->count()==1) {
            navigation->setProperty("demo",true);
            ActionItem *item=navigation->top()->findChild<ActionItem*>("globalSearchAction");
            ok=item && QMetaObject::invokeMethod(item,"triggered");
        } else if(action=="main-menu" && navigation->count()==1) {
            ImageButton *button=navigation->top()->titleBar()->findChild<ImageButton*>("chatMenuButton");
            ok=button && QMetaObject::invokeMethod(button,"clicked");
        } else if(action=="archive-view" && navigation->count()==1) {
            ActionItem *item=navigation->top()->findChild<ActionItem*>("archiveAction");
            ok=item && QMetaObject::invokeMethod(item,"triggered");
        } else if ((action=="demo-chat" || action=="demo-channel") && navigation->count()==1) {
            navigation->setProperty("demo",true);
            const QString id=action=="demo-channel"?"demo-design":request.value("group").toBool()?"demo-project":"demo-anna";
            ok=QMetaObject::invokeMethod(navigation,"openChatById",Q_ARG(QVariant,QVariant(id)),Q_ARG(QVariant,QVariant(QString())));
        } else if (action=="reference" && navigation->count()==1) {
            navigation->setProperty("demo",true);m_demo->openChat("demo-anna");
            QmlDocument *document=QmlDocument::create(QUrl::fromLocalFile(m_directory+"/reference/ChatPage.qml")).parent(this);
            document->setContextProperty("_chat",m_demo);document->setContextProperty("_live",m_live);
            document->setContextProperty("_ui",this);
            Page *page=document->createRootObject<Page>();
            if(page){navigation->push(page);ok=true;}
        } else if(action=="back" && navigation->count()>1) {
            navigation->pop();ok=true;
        } else if(action=="notification-test" && navigation->property("demo").toBool()) {
            bb::platform::Notification notice("berrygram-test");notice.setTitle("Berrygram");notice.setBody(QString::fromUtf8("Проверка уведомлений · демо"));
            const int error=notice.notify();const int badge=navigator_set_badge(NAVIGATOR_BADGE_SPLAT);
            m_result.insert("notification_error",error);m_result.insert("badge_error",badge);ok=error==0 && badge==0;
        } else if(action=="notification-clear"){
            bb::platform::Notification::deleteFromInbox("berrygram-test");navigator_clear_badge();ok=true;
        } else if(action=="capture") {
            ok=true;
        } else if(action=="disable-checks") {
            QFile::remove(m_directory+"/enabled");m_timer.stop();m_checks=false;ok=true;
        } else if(navigation->top() && navigation->count()>1) {
            Page *page=navigation->top();
            TextArea *field=page->findChild<TextArea*>("messageField");
            if(action=="settings-toggle"){
                const QString field=request.value("field").toString();
                if(field=="dark" || field=="preview" || field=="enter"){
                    CheckBox *check=page->findChild<CheckBox*>(field=="dark"?"darkSetting":field=="preview"?"previewSetting":"enterSetting");if(check){check->setChecked(request.value("value").toBool());ok=true;}
                }else if(field=="font"){DropDown *drop=page->findChild<DropDown*>("fontSetting");if(drop){drop->setSelectedIndex(qBound(0,request.value("value").toInt(),2));ok=true;}}
            }else if(action=="emoji-picker"){
                ImageButton *button=page->findChild<ImageButton*>("emojiButton");ok=button && QMetaObject::invokeMethod(button,"clicked");
            }else if(action=="emoji-pick"){
                ListView *list=page->findChild<ListView*>("emojiList");QVariantList index;index<<request.value("index",0).toInt();ok=list && QMetaObject::invokeMethod(list,"triggered",Q_ARG(QVariantList,index));
            }else if(action=="emoji-category"){
                DropDown *category=page->findChild<DropDown*>("emojiCategory");if(category){category->setSelectedIndex(request.value("index").toInt());ok=true;}
            }else if(action=="select-message"){
                ok=m_demo->toggleSelected(request.value("message_id").toString());
            }else if(action=="selected-delete" || action=="selected-forward"){
                ok=m_demo->prepareSelection(action=="selected-delete"?"delete":"forward");
            }else if(action=="selected-confirm-delete"){
                Button *button=page->findChild<Button*>("confirmDeleteButton");ok=button && QMetaObject::invokeMethod(button,"clicked");
            }else if(action=="selected-cancel"){
                m_demo->clearSelection();ok=true;
            }else if(action=="photo-demo"){
                ok=QMetaObject::invokeMethod(navigation,"openPhoto",Q_ARG(QVariant,QVariant("demo-photo-0")),Q_ARG(QVariant,QVariant(false)));
            }else if(action=="photo-next" || action=="photo-previous" || action=="photo-save"){
                Button *button=page->findChild<Button*>(action=="photo-next"?"nextPhotoButton":action=="photo-previous"?"previousPhotoButton":"savePhotoButton");ok=button && button->isEnabled() && QMetaObject::invokeMethod(button,"clicked");
            }else if(action=="photo-zoom"){
                ScrollView *scroll=page->findChild<ScrollView*>("photoScroll");if(scroll){scroll->zoomToPoint(0,0,2,ScrollAnimation::None);ok=true;}
            }else if(action=="scroll-top" || action=="scroll-bottom") {
                ListView *list=page->findChild<ListView*>(page->objectName()=="searchPage"?"searchResults":"messageList");
                if(list){list->scrollToPosition(action=="scroll-top"?ScrollPosition::Beginning:ScrollPosition::End,ScrollAnimation::None);ok=true;}
            } else if(page->objectName()=="contactsPage"){
                if(action=="contact-query"){
                    TextField *query=page->findChild<TextField*>("contactSearch");if(query){query->setText(request.value("text").toString().left(128));ok=true;}
                }else if(action=="contact-tab"){
                    SegmentedControl *tabs=page->findChild<SegmentedControl*>("contactTabs");if(tabs){tabs->setSelectedIndex(request.value("index").toInt()==1?1:0);ok=true;}
                }else if(action=="contact-open" && m_demo->contactCount()>0){
                    ListView *list=page->findChild<ListView*>("contactsList");
                    ok=list && QMetaObject::invokeMethod(list,"triggered",Q_ARG(QVariantList,QVariantList()<<qBound(0,request.value("index").toInt(),m_demo->contactCount()-1)));
                }
            } else if(action=="presence"){
                m_demo->demoPresence(request.value("status").toString());ok=true;
            } else if(action=="rich-demo"){
                ok=m_demo->injectRichDemo();
            } else if(action=="stress-demo"){
                ok=m_demo->injectStressDemo();
            } else if(action=="reply-jump"){
                ok=QMetaObject::invokeMethod(page,"openReply",Q_ARG(QVariant,QVariant(QString())),Q_ARG(QVariant,QVariant(request.value("message_id").toString())));
            } else if(action=="incoming") {
                ok=m_demo->injectIncomingDemo();
            } else if(page->objectName()=="chatInfoPage") {
                if(action=="info-tab") {
                    SegmentedControl *tabs=page->findChild<SegmentedControl*>("infoTabs");
                    if(tabs){tabs->setSelectedIndex(qBound(0,request.value("index").toInt(),3));ok=true;}
                } else {
                    const QString name=action=="notifications"?"notificationsButton":action=="pin"?"pinChatButton":action=="archive"?"archiveChatButton":"";
                    Button *button=page->findChild<Button*>(name);ok=button && QMetaObject::invokeMethod(button,"clicked");
                }
            } else if(page->objectName()=="forwardPage") {
                if(action=="recipient-query") {
                    TextField *query=page->findChild<TextField*>("recipientSearch");
                    if(query){query->setText(request.value("text").toString().left(128));ok=true;}
                } else if(action=="forward-recipient") {
                    ListView *list=page->findChild<ListView*>("recipientList");
                    ok=list && QMetaObject::invokeMethod(list,"triggered",Q_ARG(QVariantList,QVariantList()<<qBound(0,request.value("index").toInt(),4)));
                } else if(action=="forward-confirm") {
                    Button *button=page->findChild<Button*>("forwardConfirmButton");ok=button && button->isEnabled() && QMetaObject::invokeMethod(button,"clicked");
                }
            } else if(page->objectName()=="searchPage") {
                TextField *query=page->findChild<TextField*>("searchQuery");
                if(action=="search-query" && query) {query->setText(request.value("text").toString().left(256));ok=true;}
                else if(action=="search-mode" || action=="search-filter") {
                    SegmentedControl *tabs=page->findChild<SegmentedControl*>(action=="search-mode"?"searchTabs":"searchFilters");
                    const int index=qBound(0,request.value("index").toInt(),action=="search-mode"?2:3);
                    if(tabs){tabs->setSelectedIndex(index);ok=true;}
                } else if(action=="search-open" && m_search->count()>0) {
                    ListView *list=page->findChild<ListView*>("searchResults");
                    ok=list && QMetaObject::invokeMethod(list,"triggered",Q_ARG(QVariantList,QVariantList()<<0));
                }
            } else if(action=="reply" || action=="edit" || action=="forward") {
                const QString id=request.value("message_id").toString();
                if(QRegExp("[0-9]{1,10}").exactMatch(id)) {
                    ActionItem *item=page->findChild<ActionItem*>(action+"Action-"+id);
                    ok=item && item->isEnabled() && QMetaObject::invokeMethod(item,"triggered");
                }
            } else if(action=="chat-info") {
                ActionItem *item=page->findChild<ActionItem*>("chatInfoAction");ok=item && QMetaObject::invokeMethod(item,"triggered");
            } else if(action=="new-messages") {
                Button *button=page->findChild<Button*>("newMessagesButton");ok=button && QMetaObject::invokeMethod(button,"clicked");
            } else if(action=="cancel-composer") {
                Button *button=page->findChild<Button*>("cancelComposerButton");ok=button && QMetaObject::invokeMethod(button,"clicked");
            } else if(action=="compose" && field) {
                field->setText(request.value("text").toString().left(4096));
                field->editor()->setCursorPosition(field->text().size());field->requestFocus();ok=true;
            } else if((action=="send" || action=="newline" || action=="double-send") && field) {
                Shortcut *shortcut=field->findChild<Shortcut*>(action=="send"?"sendShortcut":"newlineShortcut");
                if(action=="double-send")shortcut=field->findChild<Shortcut*>("sendShortcut");
                ok=shortcut && QMetaObject::invokeMethod(shortcut,"triggered");
                if(ok && action=="double-send")QMetaObject::invokeMethod(shortcut,"triggered");
            } else if(action=="send-button" && field) {
                ImageButton *button=page->findChild<ImageButton*>("sendButton");
                ok=button && button->isEnabled() && QMetaObject::invokeMethod(button,"clicked");
            } else if(action=="menu") {
                ImageButton *button=page->titleBar()?page->titleBar()->findChild<ImageButton*>("chatMenuButton"):0;
                ok=button && QMetaObject::invokeMethod(button,"clicked");
            }
        }
        m_result.insert("ok",ok);
        if(!ok)m_result.insert("error","unsupported_action_or_state");
    }
    m_pending=true;
    QTimer::singleShot(650,this,SLOT(finishCheck()));
#endif
}

void UiTools::finishCheck()
{
#ifdef BERRYGRAM_UI_CHECKS
    NavigationPane *navigation=qobject_cast<NavigationPane*>(m_root.data());
    const bool demoPage=navigation && navigation->top() && !m_live->ready() &&
        navigation->property("demo").toBool() && (navigation->count()==1 ||
        navigation->top()->objectName()=="settingsPage" || navigation->top()->objectName()=="emojiPage" || navigation->top()->objectName()=="photoPage" ||
        navigation->top()->objectName()=="searchPage" ||
        navigation->top()->objectName()=="contactsPage" ||
        navigation->top()->objectName()=="chatInfoPage" || navigation->top()->objectName()=="forwardPage" ||
        (navigation->top()->objectName()=="chatPage" && !navigation->top()->property("liveChat").toBool()));
    if(demoPage) {
        Page *page=navigation->top();
        m_result.insert("page",page->objectName());
        m_result.insert("action_bar_hidden",page->actionBarVisibility()==ChromeVisibility::Hidden);
        m_result.insert("menu_state",int(page->actionMenuVisualState()));
        TextArea *field=page->findChild<TextArea*>("messageField");
        m_result.insert("multiline",field!=0);
        if(field) {
            m_result.insert("draft",field->text());
            m_result.insert("composer_enabled",field->isEnabled());
            m_result.insert("composer_focused",field->isFocused());
            m_result.insert("max_lines",field->autoSize()->maxLineCount());
            m_result.insert("composer_height",field->property("measuredHeight"));
        }
        QObject *content=page->findChild<QObject*>("chatContent");
        QObject *composer=page->findChild<QObject*>("composer");
        if(content && composer) {
            m_result.insert("content_height",content->property("measuredHeight"));
            m_result.insert("composer_bottom",composer->property("measuredTop").toDouble()+composer->property("measuredHeight").toDouble());
        }
        QObject *send=page->findChild<QObject*>("sendButton");
        if(send)m_result.insert("send_enabled",send->property("enabled"));
        m_result.insert("selection_count",m_demo->selectionCount());m_result.insert("selection_state",m_demo->selectionState());
        ListView *emojiList=page->findChild<ListView*>("emojiList");if(emojiList)m_result.insert("emoji_count",emojiList->dataModel()->childCount(QVariantList()));
        if(page->objectName()=="settingsPage")m_result.insert("dark",page->property("currentDark"));
        if(page->objectName()=="photoPage"){m_result.insert("photo_position",page->property("photoPosition"));m_result.insert("photo_loaded",page->property("photoLoaded"));}
        m_result.insert("message_count",m_demo->messages()->childCount(QVariantList()));
        m_result.insert("chat_count",m_demo->chatCount());
        QVariantList chatRows;
        for(int i=0;i<m_demo->chatCount();++i)chatRows.append(m_demo->chats()->data(QVariantList()<<i));
        m_result.insert("chat_rows",chatRows);
        m_result.insert("category",navigation->property("chatCategory"));
        m_result.insert("search_count",m_search->count());
        m_result.insert("target_index",m_demo->targetMessageIndex());
        m_result.insert("target_message",m_demo->targetMessageId());
        m_result.insert("contact_count",m_demo->contactCount());
        QVariantList contactRows;
        for(int i=0;i<m_demo->contactCount();++i)contactRows.append(m_demo->contacts()->data(QVariantList()<<i));
        m_result.insert("contact_rows",contactRows);
        m_result.insert("status_text",m_demo->currentChatStatus());m_result.insert("status_active",m_demo->currentStatusActive());
        m_result.insert("composer_visible",page->property("composerVisible"));
        QObject *messageList=page->findChild<QObject*>("messageList");if(messageList)m_result.insert("message_list_height",messageList->property("measuredHeight"));
        QSet<AvatarImage*> pictures;
        foreach(AvatarImage *picture,page->findChildren<AvatarImage*>())pictures.insert(picture);
        if(page->titleBar())foreach(AvatarImage *picture,page->titleBar()->findChildren<AvatarImage*>())pictures.insert(picture);
        int photoCount=0;foreach(AvatarImage *picture,pictures)if(!picture->source().isEmpty())++photoCount;
        m_result.insert("photo_count",photoCount);
        m_result.insert("demo_avatar_cached",!m_avatars->url("demo:anna").isEmpty());
        m_result.insert("composer_mode",m_demo->composerContext().value("mode"));
        m_result.insert("forward_state",m_demo->forwardSelection().value("state"));
        m_result.insert("info",m_demo->chatInfo());
        m_result.insert("info_count",m_demo->infoCount());
        m_result.insert("pending_new_count",page->property("pendingNewCount"));
        m_result.insert("persistent_search_button",page->findChild<QObject*>("globalSearchButton")!=0);
        ListScrollStateHandler *history=page->findChild<ListScrollStateHandler*>("historyScroll");
        if(history){m_result.insert("history_at_end",history->isAtEnd());m_result.insert("first_visible",history->firstVisibleItem());}
        QVariantList visibleMessages;
        const int messageTotal=m_demo->messages()->childCount(QVariantList());
        for(int i=qMax(0,messageTotal-10);i<messageTotal;++i)visibleMessages.append(m_demo->messages()->data(QVariantList()<<i));
        m_result.insert("demo_messages",visibleMessages);
        if(page->objectName()=="searchPage")m_result.insert("can_load_search",m_search->canLoadMore());
        TextField *query=page->findChild<TextField*>("searchQuery");
        if(query)m_result.insert("query",query->text());
        if(page->objectName()=="searchPage") {
            m_result.insert("search_mode",page->property("mode"));
            m_result.insert("search_filter",page->property("filter"));
            QVariantList ids;
            for(int i=0;i<m_search->count();++i)ids.append(m_search->results()->data(QVariantList()<<i).toMap().value("chatId"));
            m_result.insert("search_chat_ids",ids);
        }
    }
    m_result.insert("ready",m_live->ready());m_result.insert("phase",m_session->phase());m_result.insert("has_saved_api",m_session->hasSavedConfiguration());
    if(m_live->ready()){
        m_result.insert("live_contact_count",m_live->contactCount());m_result.insert("live_contacts_loading",m_live->contactsLoading());m_result.insert("live_contacts_error",m_live->contactsError());
    }
    m_result.insert("capture_ok",!m_result.value("metadata_only").toBool() && demoPage && m_result.value("ok").toBool() && capture(m_directory+"/"+m_id+".png"));
    PrivateFile::write(m_directory+"/result.json",m_result,16384);
    m_pending=false;
#endif
}

bool UiTools::capture(const QString &path)
{
#ifdef BERRYGRAM_UI_CHECKS
    struct Buffers {
        screen_context_t context; screen_pixmap_t pixmap;
        Buffers():context(0),pixmap(0){}
        ~Buffers(){if(pixmap)screen_destroy_pixmap(pixmap);if(context)screen_destroy_context(context);}
    } buffers;
    screen_window_t window=Application::instance()->mainWindow()->handle();
    int size[2]={0,0};
    if(!window || screen_get_window_property_iv(window,SCREEN_PROPERTY_SIZE,size)!=0 || size[0]<=0 || size[1]<=0) return false;
    if(screen_create_context(&buffers.context,SCREEN_APPLICATION_CONTEXT)!=0 || screen_create_pixmap(&buffers.pixmap,buffers.context)!=0) return false;
    int usage=SCREEN_USAGE_READ|SCREEN_USAGE_NATIVE,format=SCREEN_FORMAT_RGBA8888;
    if(screen_set_pixmap_property_iv(buffers.pixmap,SCREEN_PROPERTY_USAGE,&usage)!=0 ||
       screen_set_pixmap_property_iv(buffers.pixmap,SCREEN_PROPERTY_FORMAT,&format)!=0 ||
       screen_set_pixmap_property_iv(buffers.pixmap,SCREEN_PROPERTY_BUFFER_SIZE,size)!=0 ||
       screen_create_pixmap_buffer(buffers.pixmap)!=0) return false;
    screen_buffer_t buffer=0;
    if(screen_get_pixmap_property_pv(buffers.pixmap,SCREEN_PROPERTY_RENDER_BUFFERS,(void**)&buffer)!=0 ||
       screen_read_window(window,buffer,0,0,0)!=0) return false;
    unsigned char *pixels=0;int stride=0;
    if(screen_get_buffer_property_pv(buffer,SCREEN_PROPERTY_POINTER,(void**)&pixels)!=0 ||
       screen_get_buffer_property_iv(buffer,SCREEN_PROPERTY_STRIDE,&stride)!=0) return false;
    const QImage image(pixels,size[0],size[1],stride,QImage::Format_ARGB32);
    const bool saved=image.save(path,"PNG");
    if(saved)QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner);
    return saved;
#else
    Q_UNUSED(path);return false;
#endif
}
