#include "TelegramController.hpp"
#include <bb/system/InvokeRequest>
#include <bb/system/FileTransferMode>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <bb/multimedia/AudioChannelConfiguration>
#include <bb/multimedia/MediaError>
#include <bb/cascades/pickers/FileType>
#include <QNetworkConfiguration>
#include <QCoreApplication>
#include <QRegExp>
#include <bps/netstatus.h>

TelegramController::TelegramController(TelegramStore *store, QObject *parent)
    : QObject(parent), m_store(store), m_chats(new bb::cascades::ArrayDataModel(this)),
      m_messages(new bb::cascades::ArrayDataModel(this)), m_infoRows(new bb::cascades::ArrayDataModel(this)), m_recipients(new bb::cascades::ArrayDataModel(this)), m_scrollSerial(0), m_incomingSerial(0), m_forwardSerial(0), m_contacts(new bb::cascades::ArrayDataModel(this)),m_recording(false)
{
    m_scheduled=new bb::cascades::ArrayDataModel(this);m_downloads=new bb::cascades::ArrayDataModel(this);m_autoWifi=m_autoMobile=false;
    connect(store,SIGNAL(scheduledChanged()),this,SLOT(updateScheduled()));
    connect(store,SIGNAL(downloadsChanged()),this,SLOT(updateDownloads()));
    connect(&m_network,SIGNAL(onlineStateChanged(bool)),this,SLOT(networkChanged()));
    connect(&m_network,SIGNAL(configurationChanged(QNetworkConfiguration)),this,SLOT(networkChanged()));
    connect(&m_memoryProbe,SIGNAL(finished(int,QProcess::ExitStatus)),this,SLOT(memoryFinished()));
    connect(&m_networkPoll,SIGNAL(timeout()),this,SLOT(networkChanged()));
    connect(store,SIGNAL(contactsChanged()),this,SLOT(updateContacts()));
    connect(store,SIGNAL(contactChatReady(QString)),this,SLOT(contactReady(QString)));
    connect(store,SIGNAL(infoChanged()),this,SLOT(updateInfoRows()));
    connect(store,SIGNAL(incomingMessage()),this,SLOT(incoming()));
    connect(store,SIGNAL(forwardReady()),this,SLOT(forwardReady()));
    connect(store,SIGNAL(recipientChatsChanged()),this,SLOT(updateRecipientRows()));
    connect(store, SIGNAL(chatsChanged()), this, SLOT(updateChats()));
    connect(store, SIGNAL(messagesChanged()), this, SLOT(updateMessages()));
    connect(store, SIGNAL(changed()), this, SIGNAL(changed()));
    connect(store, SIGNAL(scrollRequested()), this, SLOT(scroll()));
    connect(&m_picker, SIGNAL(fileSelected(QStringList)), this, SLOT(fileSelected(QStringList)));
    connect(&m_recorder, SIGNAL(error(bb::multimedia::MediaError::Type,unsigned int)), this, SLOT(recordingError()));
    connect(&m_recordLimit, SIGNAL(timeout()), this, SLOT(finishRecording()));
    connect(store, SIGNAL(changed()), this, SLOT(checkAttachmentChat()));
    m_recordLimit.setSingleShot(true);
    updateChats();
    updateMessages();
}

void TelegramController::updateScheduled(){reconcile(m_scheduled,m_store->scheduledMessages(),"messageId");emit changed();}
void TelegramController::updateDownloads(){reconcile(m_downloads,m_store->downloads(),"fileId");emit changed();}
void TelegramController::networkChanged(){
    const QNetworkConfiguration config=m_network.defaultConfiguration();const QNetworkConfiguration::BearerType type=config.bearerType();
    bool wifi=type==QNetworkConfiguration::BearerWLAN,known=config.isValid() && m_network.isOnline() && (wifi || type==QNetworkConfiguration::Bearer2G || type==QNetworkConfiguration::BearerCDMA2000 || type==QNetworkConfiguration::BearerWCDMA || type==QNetworkConfiguration::BearerHSPA || type==QNetworkConfiguration::BearerWiMAX);
    netstatus_interface_details_t *details=0;
    if(netstatus_get_interface_details(0,&details)==BPS_SUCCESS && details){const netstatus_interface_type_t kind=netstatus_interface_get_type(details);wifi=kind==NETSTATUS_INTERFACE_TYPE_WIFI;known=wifi || kind==NETSTATUS_INTERFACE_TYPE_CELLULAR;netstatus_free_interface_details(&details);}
    m_networkLabel=wifi?QString::fromUtf8("Wi-Fi"):known?QString::fromUtf8("Мобильная сеть"):QString::fromUtf8("Тип сети не определён");
    m_store->setAutoDownload(known && (wifi?m_autoWifi:m_autoMobile));m_store->autoDownload();
}
void TelegramController::refreshDiagnostics(){
    m_diagnostics=m_store->diagnostics()+QString::fromUtf8("\nСеть: ")+m_networkLabel;emit changed();
    if(m_memoryProbe.state()==QProcess::NotRunning)m_memoryProbe.start("pidin",QStringList()<<"-p"<<QString::number(QCoreApplication::applicationPid())<<"mem");
}
void TelegramController::memoryFinished(){
    const QStringList lines=QString::fromLocal8Bit(m_memoryProbe.readAllStandardOutput()).split('\n');m_memoryProbe.readAllStandardError();
    if(lines.size()>1){const QStringList fields=lines.at(1).simplified().split(' ');if(fields.size()>6 && QRegExp("[0-9]+[KMG]?").exactMatch(fields.at(6)))m_diagnostics+=QString::fromUtf8("\nПамять процесса: ")+fields.at(6);}
    emit changed();
}

void TelegramController::reconcile(bb::cascades::ArrayDataModel *model, const QVariantList &rows, const QString &key)
{
    // Replace changed rows in place. Incoming messages append without resetting
    // the ListView; prepending history keeps existing rows instead of clearing it.
    for (int i = 0; i < rows.size(); ++i) {
        const QVariantMap next = rows.at(i).toMap();
        if (i >= model->size()) {
            QVariantList tail;for(int j=i;j<rows.size();++j)tail<<rows.at(j);
            model->append(tail);
            break;
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

bool TelegramController::openMedia(const QString &id)
{
    const QString path = m_store->mediaPath(id);
    if (path.isEmpty()) return false;
    bb::system::InvokeRequest request;
    request.setAction("bb.action.OPEN");
    request.setUri(QUrl::fromLocalFile(path));
    request.setMimeType(m_store->mediaMime(id));
    request.setFileTransferMode(bb::system::FileTransferMode::CopyReadOnly);
    return m_invoke.invoke(request) != 0;
}

bool TelegramController::openLink(const QString &address){
    const QUrl url(address);if(!url.isValid() || (url.scheme()!="http" && url.scheme()!="https") || url.host().isEmpty() || !url.userInfo().isEmpty())return false;
    bb::system::InvokeRequest request;request.setAction("bb.action.OPEN");request.setUri(url);return m_invoke.invoke(request)!=0;
}

QString TelegramController::newUploadPath(const QString &name)
{
    m_store->cleanupUploads();
    QDir uploads(m_store->uploadDirectory());
    if (QFileInfo(uploads.absolutePath()).isSymLink() || !QDir().mkpath(uploads.absolutePath())) return QString();
    QFile::setPermissions(uploads.absolutePath(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    // Keep inputs until TDLib has finished using them. Bound local retained copies.
    qlonglong bytes = 0; int count = 0;
    const QStringList directories = uploads.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &directory, directories) {
        const QFileInfoList files = QDir(uploads.filePath(directory)).entryInfoList(QDir::Files);
        foreach (const QFileInfo &file, files) { bytes += file.size(); ++count; }
    }
    if (count >= 32 || bytes >= 206 * 1024 * 1024) return QString();
    const QString folder = uploads.filePath(QUuid::createUuid().toString().remove('{').remove('}'));
    if (!QDir().mkpath(folder)) return QString();
    QFile::setPermissions(folder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return QDir(folder).filePath(QFileInfo(name).fileName());
}

void TelegramController::chooseAttachment(const QString &kind)
{
    if (!m_store->canSendMedia(kind) || m_recording) return;
    m_pickerChat = m_store->currentChatId(); m_pickerKind = kind;
    m_picker.setType(kind == "photo" ? bb::cascades::pickers::FileType::Picture :
        kind == "voice" ? bb::cascades::pickers::FileType::Music :
        (bb::cascades::pickers::FileType::Picture | bb::cascades::pickers::FileType::Document |
         bb::cascades::pickers::FileType::Music | bb::cascades::pickers::FileType::Video | bb::cascades::pickers::FileType::Other));
    m_picker.open();
}

void TelegramController::fileSelected(const QStringList &files)
{
    if (files.isEmpty() || m_pickerChat != m_store->currentChatId() || !m_store->canSendMedia(m_pickerKind)) return;
    QFileInfo input(files.first());
    if (!input.isFile() || input.size() <= 0 || input.size() > 50 * 1024 * 1024) {
        m_mediaError = QString::fromUtf8("Выберите непустой файл до 50 МБ."); emit changed(); return;
    }
    const QString target = newUploadPath(input.fileName());
    if (target.isEmpty() || !QFile::copy(input.absoluteFilePath(), target)) {
        m_mediaError = QString::fromUtf8("Не удалось скопировать вложение в каталог приложения."); emit changed(); return;
    }
    leaveAttachment();
    QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
    m_attachment.insert("path", target); m_attachment.insert("complete", true); m_attachment.insert("kind", m_pickerKind); m_attachmentChat = m_pickerChat;
    if (!m_store->stageAttachment(target, m_pickerKind, m_store->draftText())) {
        m_mediaError = m_store->errorText(); emit changed(); return;
    }
    m_attachment = m_store->attachmentDraft();
    // Imported voice files have unknown duration; use the recorder for voice notes.
    if (m_pickerKind == "voice") m_mediaError = QString::fromUtf8("Для голосового используйте запись; готовый аудиофайл можно отправить как документ.");
    emit changed();
}

void TelegramController::leaveAttachment()
{
    m_recordLimit.stop();
    if (m_recording) { m_recording = false; m_recorder.reset(); }
    // Only unfinished recordings are temporary; prepared inputs belong to the journal.
    if (m_attachment.value("recordId").toString().isEmpty() && !m_attachment.isEmpty() && !m_attachment.value("complete").toBool())
        QFile::remove(m_attachment.value("path").toString());
    m_attachment.clear(); m_attachmentChat.clear(); m_mediaError.clear(); emit changed();
}

void TelegramController::cancelAttachment()
{
    // Stop the microphone even when a storage error prevents changing the journal.
    leaveAttachment();
    if (!m_store->clearAttachmentDraft()) { m_mediaError = m_store->errorText(); emit changed(); }
}

void TelegramController::checkAttachmentChat()
{
    if ((!m_store->ready() || m_attachmentChat != m_store->currentChatId()) && !m_attachment.isEmpty()) leaveAttachment();
    if (!m_recording && m_store->ready()) {
        const QVariantMap restored = m_store->attachmentDraft();
        if (m_attachment != restored) { m_attachment = restored; m_attachmentChat = m_store->currentChatId(); emit changed(); }
    }
}

void TelegramController::toggleRecording()
{
    if (m_recording) { finishRecording(); return; }
    if (!m_store->canSendMedia("voice")) return;
    cancelAttachment();
    const QString target = newUploadPath("voice.m4a");
    if (target.isEmpty()) { m_mediaError = QString::fromUtf8("Недостаточно места для записи."); emit changed(); return; }
    m_attachment.insert("path", target); m_attachment.insert("kind", "voice"); m_attachmentChat = m_store->currentChatId();
    m_recorder.setOutputUrl(QUrl::fromLocalFile(target));
    m_recorder.setAudioChannelConfiguration(bb::multimedia::AudioChannelConfiguration::Mono);
    m_recording = true;
    if (m_recorder.record() != bb::multimedia::MediaError::None) { recordingError(); return; }
    m_recordLimit.start(300000); emit changed();
}

void TelegramController::finishRecording()
{
    if (!m_recording) return;
    const int seconds = qMax(1, int((m_recorder.duration() + 999) / 1000));
    m_recording = false; m_recordLimit.stop();
    if (m_recorder.reset() != bb::multimedia::MediaError::None) { recordingError(); return; }
    QFile::setPermissions(m_attachment.value("path").toString(), QFile::ReadOwner | QFile::WriteOwner);
    m_attachment.insert("duration", seconds); m_attachment.insert("complete", true);
    if (!m_store->stageAttachment(m_attachment.value("path").toString(), "voice", m_store->draftText(), seconds)) {
        m_mediaError = m_store->errorText(); emit changed(); return;
    }
    m_attachment = m_store->attachmentDraft(); emit changed();
}

void TelegramController::recordingError()
{
    if (!m_recording && (m_attachment.isEmpty() || !m_attachment.value("recordId").toString().isEmpty())) return;
    cancelAttachment(); m_mediaError = QString::fromUtf8("Не удалось записать голос. Проверьте разрешение микрофона."); emit changed();
}

bool TelegramController::sendSelected(const QString &caption)
{
    if (m_attachment.isEmpty()) return m_store->sendText(caption);
    if (m_recording || m_attachmentChat != m_store->currentChatId()) return false;
    const bool sent = m_store->sendAttachment(m_attachment.value("path").toString(), m_attachment.value("kind").toString(),
        caption, m_attachment.value("duration").toInt(), m_attachment.value("width").toInt(), m_attachment.value("height").toInt());
    if (sent) { m_attachment.clear(); m_attachmentChat.clear(); emit changed(); }
    return sent;
}

bool TelegramController::scheduleSelected(const QString &caption, const QDateTime &when)
{
    if(!when.isValid() || when.toTime_t()<=QDateTime::currentDateTime().toTime_t())return false;
    const uint date=when.toTime_t();
    if(m_attachment.isEmpty())return m_store->sendText(caption,date);
    if(m_recording || m_attachmentChat!=m_store->currentChatId())return false;
    const bool sent=m_store->sendAttachment(m_attachment.value("path").toString(),m_attachment.value("kind").toString(),caption,
        m_attachment.value("duration").toInt(),m_attachment.value("width").toInt(),m_attachment.value("height").toInt(),date);
    if(sent){m_attachment.clear();m_attachmentChat.clear();emit changed();}return sent;
}

void TelegramController::recoverAttachment()
{
    if (m_store->failedAttachment().isEmpty()) return;
    leaveAttachment();
    if (!m_store->recoverAttachment()) { m_mediaError = m_store->errorText(); emit changed(); return; }
    m_attachment = m_store->attachmentDraft(); m_attachmentChat = m_store->currentChatId(); emit changed();
}

bool TelegramController::previewAttachment()
{
    if (m_recording || m_attachment.isEmpty()) return false;
    bb::system::InvokeRequest request; request.setAction("bb.action.OPEN");
    request.setUri(QUrl::fromLocalFile(m_attachment.value("path").toString()));
    request.setFileTransferMode(bb::system::FileTransferMode::CopyReadOnly);
    return m_invoke.invoke(request) != 0;
}

void TelegramController::updateInfoRows(){reconcile(m_infoRows,m_store->infoRows(),"key");emit changed();}
void TelegramController::incoming(){++m_incomingSerial;emit changed();}
void TelegramController::forwardReady(){++m_forwardSerial;emit changed();}
void TelegramController::refreshRecipients(){
    const int previous=m_recipients->size();
    QVariantList rows;
    foreach(const QVariant &value,m_store->knownChats()) {
        QVariantMap row=value.toMap();
        if(!row.value("canSend").toBool() || row.value("topics").toBool() || row.value("type")=="chatTypeSecret")continue;
        if(!m_recipientQuery.isEmpty() && !row.value("title").toString().contains(m_recipientQuery,Qt::CaseInsensitive))continue;
        row.remove("mainOrder");row.remove("archiveOrder");row.insert("time",QString());row.insert("unreadLabel",QString());rows.append(row);
    }
    reconcile(m_recipients,rows,"chatId");
    if(previous!=m_recipients->size())emit changed();
}
bool TelegramController::beginMessageAction(const QString &id,const QString &action){
    if(m_recording || !m_attachment.isEmpty()) {m_mediaError=QString::fromUtf8("Сначала уберите подготовленное вложение или закончите запись.");emit changed();return false;}
    return m_store->beginMessageAction(id,action);
}

void TelegramController::updateContacts(){reconcile(m_contacts,m_store->contacts(),"key");emit changed();}
bool TelegramController::openContact(const QString &id,QObject *target){
    if(!target || target->objectName()!="contactsPage")return false;
    m_contactTarget=target;return m_store->openContact(id);
}
void TelegramController::cancelContactOpen(){m_contactTarget=0;m_store->cancelContactOpen();}
void TelegramController::contactReady(const QString &id){
    if(m_contactTarget)QMetaObject::invokeMethod(m_contactTarget.data(),"openContactChat",Qt::QueuedConnection,Q_ARG(QVariant,QVariant(id)));
    m_contactTarget=0;
}
