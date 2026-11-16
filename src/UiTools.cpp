#include "UiTools.hpp"
#include "ChatController.hpp"
#include "TelegramController.hpp"
#include "PrivateFile.hpp"
#include <bb/cascades/AbstractPane>
#include <bb/cascades/AbstractTextControl>
#include <bb/cascades/Application>
#include <bb/cascades/ImageButton>
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

UiTools::UiTools(ChatController *demo, TelegramController *live, QObject *parent)
    : QObject(parent), m_demo(demo), m_live(live),
      m_directory(QDir::currentPath()+"/data/ui-check"), m_checks(false), m_pending(false)
{
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
        if (action=="demo-chat" && navigation->count()==1) {
            navigation->setProperty("demo",true);
            const QString id=request.value("group").toBool()?"demo-project":"demo-anna";
            ok=QMetaObject::invokeMethod(navigation,"openChatById",Q_ARG(QVariant,QVariant(id)));
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
            if(action=="compose" && field) {
                field->setText(request.value("text").toString().left(4096));
                field->editor()->setCursorPosition(field->text().size());field->requestFocus();ok=true;
            } else if((action=="send" || action=="newline") && field) {
                Shortcut *shortcut=field->findChild<Shortcut*>(action=="send"?"sendShortcut":"newlineShortcut");
                ok=shortcut && QMetaObject::invokeMethod(shortcut,"triggered");
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
