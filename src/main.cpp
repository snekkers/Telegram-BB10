#include "ChatController.hpp"
#include "TelegramSession.hpp"
#include "TelegramStore.hpp"
#include "TelegramController.hpp"
#include "UiTools.hpp"
#include "SearchController.hpp"
#include "AutoPager.hpp"
#include "AvatarImages.hpp"
#include "EmojiView.hpp"
#include "NotificationCenter.hpp"
#include "MediaController.hpp"
#include "EmojiCatalog.hpp"
#include <QResource>

#include <bb/cascades/AbstractPane>
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/ThemeSupport>
#include <bb/cascades/VisualStyle>
#include <bb/cascades/Color>
#include <bb/cascades/SceneCover>
#include <QDebug>
#include <QCoreApplication>
#include <QtDeclarative/qdeclarative.h>
#include <cstdlib>

Q_DECL_EXPORT int main(int argc, char **argv)
{
    bb::cascades::Application app(argc, argv);
    app.themeSupport()->setVisualStyleAndPrimaryColor(bb::cascades::VisualStyle::Bright,
        bb::cascades::Color::fromARGB(0xff5288b7), bb::cascades::Color::fromARGB(0xff42739d));
    qmlRegisterType<AutoPager>("Berrygram", 1, 0, "AutoPager");
    qmlRegisterType<AvatarImage>("Berrygram",1,0,"AvatarImage");
    QResource::registerResource("app/native/assets/emoji.rcc");
    qmlRegisterType<EmojiView>("Berrygram",1,0,"EmojiView");
    ChatController controller;
    TelegramSession telegram;
    TelegramStore store(&telegram);
    EmojiCatalog emoji;
    MediaController media(&store,&controller);
    NotificationCenter notifications(&telegram,&store);
    AvatarProvider avatars(&store);
    TelegramController telegramController(&store);
    TelegramSearch telegramSearch(&telegram,&store);
    SearchController searchController(&telegramSearch,&controller);
    UiTools uiTools(&controller,&telegramController,&searchController,&telegram,&avatars);
    QObject::connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), &store, SLOT(flushDrafts()));

    bb::cascades::QmlDocument *document =
        bb::cascades::QmlDocument::create("asset:///main.qml").parent(&app);
    document->setContextProperty("_emoji", &emoji);
    document->setContextProperty("_media", &media);
    document->setContextProperty("_notifications", &notifications);
    document->setContextProperty("_chat", &controller);
    document->setContextProperty("_telegram", &telegram);
    document->setContextProperty("_live", &telegramController);
    document->setContextProperty("_ui", &uiTools);
    document->setContextProperty("_avatars",&avatars);
    document->setContextProperty("_search", &searchController);

    bb::cascades::QmlDocument *coverDocument=bb::cascades::QmlDocument::create("asset:///ActiveFrame.qml").parent(&app);
    coverDocument->setContextProperty("_notifications",&notifications);
    bb::cascades::SceneCover *cover=coverDocument->createRootObject<bb::cascades::SceneCover>();
    if(cover)app.setCover(cover);

    if (document->hasErrors()) {
        qCritical() << "Cannot load assets/main.qml";
        return EXIT_FAILURE;
    }

    bb::cascades::AbstractPane *root =
        document->createRootObject<bb::cascades::AbstractPane>();
    if (!root) {
        qCritical() << "Cannot create the Cascades scene";
        return EXIT_FAILURE;
    }

    app.setScene(root);
    uiTools.setRoot(root);
    return bb::cascades::Application::exec();
}
