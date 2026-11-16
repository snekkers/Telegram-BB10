#include "UiTools.hpp"
#include "ChatController.hpp"
#include "TelegramController.hpp"
#include "SearchController.hpp"
#include "PrivateFile.hpp"
#include "AutoPager.hpp"
#include <bb/cascades/ActionItem>
#include <bb/cascades/ListScrollStateHandler>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/AbstractTextControl>
#include <bb/cascades/Application>
#include <bb/cascades/ImageButton>
#include <bb/cascades/Button>
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
#include <bb/system/Clipboard>
#include <QDir>
#include <QFile>
#include <QtGui/QImage>
#include <QRegExp>
#include <QUrl>
#include <screen/screen.h>

using namespace bb::cascades;

UiTools::UiTools(ChatController *demo, TelegramController *live, SearchController *search, QObject *parent)
    : QObject(parent), m_demo(demo), m_live(live), m_search(search),
      m_directory(QDir::currentPath()+"/data/ui-check"), m_checks(false), m_pending(false),m_lastSubmitMs(-1000)
{
    m_submitClock.start();
#ifdef BERRYGRAM_UI_CHECKS
    m_checks = QFile::exists(m_directory+"/enabled");
    if (m_checks) {
        connect(&m_timer,SIGNAL(timeout()),this,SLOT(checkRequest()));
        m_timer.start(750);
    }
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
    if (m_live->ready() || !navigation) {
        m_result.insert("error","demo_only");
    } else {
        const QString action=request.value("action").toString();
        bool ok=false;
        if (action=="demo-list" && navigation->count()==1) {
            navigation->setProperty("demo",true);ok=true;
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
        } else if (action=="demo-chat" && navigation->count()==1) {
            navigation->setProperty("demo",true);
            const QString id=request.value("group").toBool()?"demo-project":"demo-anna";
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
        } else if(action=="capture") {
            ok=true;
        } else if(action=="disable-checks") {
            QFile::remove(m_directory+"/enabled");m_timer.stop();m_checks=false;ok=true;
        } else if(navigation->top() && navigation->count()>1) {
            Page *page=navigation->top();
            TextArea *field=page->findChild<TextArea*>("messageField");
            if(action=="scroll-top" || action=="scroll-bottom") {
                ListView *list=page->findChild<ListView*>(page->objectName()=="searchPage"?"searchResults":"messageList");
                if(list){list->scrollToPosition(action=="scroll-top"?ScrollPosition::Beginning:ScrollPosition::End,ScrollAnimation::None);ok=true;}
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
        navigation->top()->objectName()=="searchPage" ||
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
        m_result.insert("message_count",m_demo->messages()->childCount(QVariantList()));
        m_result.insert("chat_count",m_demo->chatCount());
        QVariantList chatRows;
        for(int i=0;i<m_demo->chatCount();++i)chatRows.append(m_demo->chats()->data(QVariantList()<<i));
        m_result.insert("chat_rows",chatRows);
        m_result.insert("category",navigation->property("chatCategory"));
        m_result.insert("search_count",m_search->count());
        m_result.insert("target_index",m_demo->targetMessageIndex());
        m_result.insert("target_message",m_demo->targetMessageId());
        m_result.insert("composer_mode",m_demo->composerContext().value("mode"));
        m_result.insert("forward_state",m_demo->forwardSelection().value("state"));
        m_result.insert("info",m_demo->chatInfo());
        m_result.insert("info_count",m_demo->infoCount());
        m_result.insert("pending_new_count",page->property("pendingNewCount"));
        m_result.insert("persistent_search_button",page->findChild<QObject*>("globalSearchButton")!=0);
        ListScrollStateHandler *history=page->findChild<ListScrollStateHandler*>("historyScroll");
        if(history){m_result.insert("history_at_end",history->isAtEnd());m_result.insert("first_visible",history->firstVisibleItem());}
        QVariantList visibleMessages;
        for(int i=0;i<m_demo->messages()->childCount(QVariantList());++i)visibleMessages.append(m_demo->messages()->data(QVariantList()<<i));
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
    m_result.insert("capture_ok",demoPage && m_result.value("ok").toBool() && capture(m_directory+"/"+m_id+".png"));
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
