#include "ChatController.hpp"
#include "TelegramSession.hpp"

#include <bb/cascades/AbstractPane>
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <QDebug>
#include <cstdlib>

Q_DECL_EXPORT int main(int argc, char **argv)
{
    bb::cascades::Application app(argc, argv);
    ChatController controller;
    TelegramSession telegram;

    bb::cascades::QmlDocument *document =
        bb::cascades::QmlDocument::create("asset:///main.qml").parent(&app);
    document->setContextProperty("_chat", &controller);
    document->setContextProperty("_telegram", &telegram);

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
