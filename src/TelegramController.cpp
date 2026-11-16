#include "TelegramController.hpp"

TelegramController::TelegramController(TelegramStore *store, QObject *parent)
    : QObject(parent), m_store(store), m_chats(new bb::cascades::ArrayDataModel(this)),
      m_messages(new bb::cascades::ArrayDataModel(this)), m_scrollSerial(0)
{
    connect(store, SIGNAL(chatsChanged()), this, SLOT(updateChats()));
    connect(store, SIGNAL(messagesChanged()), this, SLOT(updateMessages()));
    connect(store, SIGNAL(changed()), this, SIGNAL(changed()));
    connect(store, SIGNAL(scrollRequested()), this, SLOT(scroll()));
    updateChats();
    updateMessages();
}

void TelegramController::reconcile(bb::cascades::ArrayDataModel *model, const QVariantList &rows, const QString &key)
{
    // Replace changed rows in place. Incoming messages append without resetting
    // the ListView; prepending history keeps existing rows instead of clearing it.
    for (int i = 0; i < rows.size(); ++i) {
        const QVariantMap next = rows.at(i).toMap();
        if (i >= model->size()) {
            model->append(QVariant(next));
            continue;
        }
        if (model->value(i).toMap().value(key) != next.value(key)) {
            for (int j = i + 1; j < model->size(); ++j) {
                if (model->value(j).toMap().value(key) == next.value(key)) {
                    model->removeAt(j);
                    break;
                }
            }
            model->insert(i, QVariant(next));
        } else if (model->value(i).toMap() != next)
            model->replace(i, QVariant(next));
    }
    while (model->size() > rows.size()) model->removeAt(model->size() - 1);
}

void TelegramController::updateChats() { reconcile(m_chats, m_store->chats(), "chatId"); }
void TelegramController::updateMessages() { reconcile(m_messages, m_store->messages(), "messageKey"); }
void TelegramController::scroll() { ++m_scrollSerial; emit changed(); }
