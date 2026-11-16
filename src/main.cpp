#include "ChatController.hpp"
#include "TelegramSession.hpp"
#include "TelegramStore.hpp"
#include "TelegramController.hpp"

#include <bb/cascades/AbstractPane>
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/ThemeSupport>
#include <bb/cascades/VisualStyle>
#include <bb/cascades/Color>
#include <QDebug>
#include <QCoreApplication>
#include <cstdlib>

Q_DECL_EXPORT int main(int argc, char **argv)
{
    bb::cascades::Application app(argc, argv);
    app.themeSupport()->setVisualStyleAndPrimaryColor(bb::cascades::VisualStyle::Bright,
        bb::cascades::Color::fromARGB(0xff5288b7), bb::cascades::Color::fromARGB(0xff42739d));
    ChatController controller;
    TelegramSession telegram;
    TelegramStore store(&telegram);
    TelegramController telegramController(&store);
    QObject::connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), &store, SLOT(flushDrafts()));

    bb::cascades::QmlDocument *document =
        bb::cascades::QmlDocument::create("asset:///main.qml").parent(&app);
    document->setContextProperty("_chat", &controller);
    document->setContextProperty("_telegram", &telegram);
    document->setContextProperty("_live", &telegramController);

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
    return bb::cascades::Application::exec();
}
