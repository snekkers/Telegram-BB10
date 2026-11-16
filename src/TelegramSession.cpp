#include "TelegramSession.hpp"
#include "JsonCodec.hpp"
#include "PrivateFile.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegExp>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
QString ru(const char *s) { return QString::fromUtf8(s); }
QVariantMap method(const char *name)
{
    QVariantMap value;
    value.insert("@type", QString::fromLatin1(name));
    return value;
}
const int MaxLine = 8 * 1024 * 1024;
}

TelegramSession::TelegramSession(QObject *parent)
    : QObject(parent),
      m_engine(QCoreApplication::applicationDirPath() + "/telegram/bin/berrygram-tdjson"),
      m_library(QCoreApplication::applicationDirPath() + "/telegram/lib/libtdjson.so"),
      m_dataDirectory(QDir::currentPath() + "/data/telegram")
{
    initialize();
}

TelegramSession::TelegramSession(const QString &engine, const QString &library,
                               const QString &dataDirectory, QObject *parent)
    : QObject(parent), m_engine(engine), m_library(library), m_dataDirectory(dataDirectory)
{
    initialize();
}

void TelegramSession::initialize()
{
    m_sequence = 0;
    m_restartRequested=m_resumeRuntime=false;m_receivedEvents=m_receivedBytes=0;m_uptime.start();
    m_readQueued = false;
    m_busy = m_stopping = m_failed = false;
    m_phase = "stopped";
    m_status = available() ? ru("Движок остановлен")
                          : ru("В этой сборке нет Telegram-движка. Доступно демо.");
    m_timer.setSingleShot(true);
    connect(&m_process, SIGNAL(started()), this, SLOT(started()));
    connect(&m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(readOutput()));
    connect(&m_process, SIGNAL(readyReadStandardError()), this, SLOT(discardErrorOutput()));
    connect(&m_process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
    connect(&m_process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished(int,QProcess::ExitStatus)));
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(timedOut()));
}

TelegramSession::~TelegramSession()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.closeWriteChannel(); // EOF asks the host for TDLib close.
        if (!m_process.waitForFinished(12000)) {
            m_process.kill();
            m_process.waitForFinished(1000);
        }
    }
}

bool TelegramSession::available() const
{
    return QFileInfo(m_engine).isExecutable() && QFileInfo(m_library).isFile();
}

void TelegramSession::setPhase(const QString &phase, const QString &status)
{
    m_phase = phase;
    m_status = status;
    emit changed();
}

void TelegramSession::start()
{
    if (m_process.state() != QProcess::NotRunning || !available())
        return;
    m_input.clear();
    m_pending.clear();
    m_versionRequest.clear();
    m_authorizationType.clear();
    m_error.clear();
    m_version.clear();
    m_stopping = m_failed = false;
    m_busy = true;
    // Never expose the GNU runtime to the Cascades UI process.
    QStringList environment = QProcess::systemEnvironment();
    QStringList childEnvironment;
    QString loaderPath = QFileInfo(m_library).absolutePath();
    for (int i = 0; i < environment.size(); ++i) {
        if (environment.at(i).startsWith("LD_LIBRARY_PATH="))
            loaderPath += ":" + environment.at(i).mid(16);
        else
            childEnvironment.append(environment.at(i));
    }
    childEnvironment.append("LD_LIBRARY_PATH=" + loaderPath);
    m_process.setEnvironment(childEnvironment);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    setPhase("starting", ru("Запуск Telegram-движка…"));
    m_timer.start(20000);
    m_process.start(m_engine, QStringList() << "--library" << m_library);
}

void TelegramSession::started()
{
    if (m_stopping) {
        m_process.closeWriteChannel();
        return;
    }
    QVariantMap getVersion = method("getOption");
    getVersion.insert("name", "version");
    m_versionRequest = request(getVersion);
}

QString TelegramSession::request(QVariantMap object)
{
    if (m_process.state() != QProcess::Running)
        return QString();
    QString id = "berrygram." + QString::number(++m_sequence);
    object.insert("@extra", id);
    QByteArray bytes = JsonCodec::encode(object);
    if (bytes.isEmpty() || bytes.size() > 64 * 1024 || m_process.bytesToWrite() > 128 * 1024) {
        fail(ru("Не удалось подготовить запрос движку."));
        return QString();
    }
    bytes.append('\n');
    if (m_process.write(bytes) != bytes.size()) {
        fail(ru("Не удалось передать запрос движку."));
        return QString();
    }
    return id;
}

bool TelegramSession::authenticate(QVariantMap object)
{
    m_error.clear();
    m_pending = request(object);
    if (m_pending.isEmpty())
        return false;
    m_busy = true;
    m_timer.start(60000);
    emit changed();
    return true;
}

QString TelegramSession::sendReady(QVariantMap object)
{
    if (m_phase != "ready" || m_stopping || m_failed)
        return QString();
    return request(object);
}

bool TelegramSession::loadDatabaseKey(QByteArray *key)
{
    QDir directory(m_dataDirectory);
    if (!QFileInfo(m_dataDirectory).exists() && !QDir().mkpath(m_dataDirectory))
        return false;
    const QFile::Permissions privateDirectory = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    if (QFileInfo(m_dataDirectory).isSymLink() || !QFile::setPermissions(m_dataDirectory, privateDirectory))
        return false;
    const QString keyPath = directory.filePath("database.key");
    const QByteArray encodedPath = QFile::encodeName(keyPath);
    struct stat info;
    if (::lstat(encodedPath.constData(), &info) == 0) {
        if (!S_ISREG(info.st_mode) || info.st_size != 32 ||
            !QFile::setPermissions(keyPath, QFile::ReadOwner | QFile::WriteOwner))
            return false;
        QFile file(keyPath);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        *key = file.readAll();
        return key->size() == 32;
    }
    if (errno != ENOENT)
        return false;
    // Never replace a lost key while a database may still need it.
    if (!directory.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty())
        return false;
    QFile entropy("/dev/urandom");
    if (!entropy.open(QIODevice::ReadOnly)) {
        entropy.setFileName("/dev/random"); // QNX random service.
        if (!entropy.open(QIODevice::ReadOnly))
            return false;
    }
    char randomBytes[32];
    if (entropy.read(randomBytes, sizeof randomBytes) != sizeof randomBytes)
        return false;
    *key = QByteArray(randomBytes, sizeof randomBytes);
    int fd = ::open(encodedPath.constData(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return false;
    int offset = 0;
    while (offset < int(sizeof randomBytes)) {
        ssize_t count = ::write(fd, randomBytes + offset, sizeof randomBytes - offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            break;
        offset += count;
    }
    bool written = offset == sizeof randomBytes;
    if (::fsync(fd) != 0)
        written = false;
    if (::close(fd) != 0)
        written = false;
    if (!written)
        ::unlink(encodedPath.constData());
    return written;
}

bool TelegramSession::configure(const QString &apiId, const QString &apiHash)
{
    if (m_phase != "parameters" || m_busy)
        return false;
    bool validId = false;
    int id = apiId.trimmed().toInt(&validId);
    QString hash = apiHash.trimmed();
    if (!validId || id <= 0 || !QRegExp("[0-9a-fA-F]{32}").exactMatch(hash)) {
        m_error = ru("Введите положительный api_id и api_hash из 32 шестнадцатеричных символов.");
        emit changed();
        return false;
    }
    QByteArray key;
    if (!loadDatabaseKey(&key)) {
        m_error = ru("Не удалось безопасно открыть ключ базы. Сохранённая сессия не изменена.");
        emit changed();
        return false;
    }
    QVariantMap parameters = method("setTdlibParameters");
    parameters.insert("use_test_dc", false);
    parameters.insert("database_directory", QDir(m_dataDirectory).filePath("db"));
    parameters.insert("files_directory", QDir(m_dataDirectory).filePath("files"));
    parameters.insert("database_encryption_key", QString::fromLatin1(key.toBase64()));
    parameters.insert("use_file_database", true);
    parameters.insert("use_chat_info_database", true);
    parameters.insert("use_message_database", true);
    parameters.insert("use_secret_chats", false);
    parameters.insert("api_id", id);
    parameters.insert("api_hash", hash);
    parameters.insert("system_language_code", "ru");
    parameters.insert("device_model", "BlackBerry Classic");
    parameters.insert("system_version", "BlackBerry 10");
    parameters.insert("application_version", "0.15.0.20");
    const bool accepted=authenticate(parameters);
    if(accepted){m_runtimeApiId=apiId.trimmed();m_runtimeApiHash=hash;}
    return accepted;
}

bool TelegramSession::hasSavedConfiguration() const
{
    QVariantMap configuration;
    return PrivateFile::read(QDir(m_dataDirectory).filePath("api.json"), &configuration) &&
        configuration.value("api_id").toInt() > 0 &&
        QRegExp("[0-9a-fA-F]{32}").exactMatch(configuration.value("api_hash").toString());
}

QString TelegramSession::savedApiId() const
{
    QVariantMap configuration;
    if (!PrivateFile::read(QDir(m_dataDirectory).filePath("api.json"), &configuration)) return QString();
    return configuration.value("api_id").toString();
}

bool TelegramSession::configureAndRemember(const QString &apiId, const QString &apiHash)
{
    if (!configure(apiId, apiHash)) return false;
    QVariantMap configuration;
    configuration.insert("api_id", apiId.trimmed());
    configuration.insert("api_hash", apiHash.trimmed());
    if (!PrivateFile::write(QDir(m_dataDirectory).filePath("api.json"), configuration))
        m_error = ru("Подключение начато, но сохранить параметры API не удалось.");
    emit changed();
    return true;
}

bool TelegramSession::useSavedConfiguration()
{
    QVariantMap configuration;
    if (!PrivateFile::read(QDir(m_dataDirectory).filePath("api.json"), &configuration)) return false;
    return configure(configuration.value("api_id").toString(), configuration.value("api_hash").toString());
}

bool TelegramSession::testConfigurationAvailable() const
{
#ifdef BERRYGRAM_DEVELOPMENT_TEST_API
    return true;
#else
    return false;
#endif
}

bool TelegramSession::useTestConfiguration()
{
#ifdef BERRYGRAM_DEVELOPMENT_TEST_API
    // Official, restricted TEST ONLY credentials, not another developer's keys.
    // https://github.com/telegramdesktop/tdesktop/blob/dev/docs/api_credentials.md
    // Never persist them or enable this option in a release build.
    return configure("17349", "344583e45741c457fe1862106095a5eb");
#else
    return false;
#endif
}

bool TelegramSession::forgetConfiguration()
{
    const QString path = QDir(m_dataDirectory).filePath("api.json");
    const bool result = !QFile::exists(path) || QFile::remove(path);
    emit changed();
    return result;
}

bool TelegramSession::submit(const QString &value)
{
    if (m_busy || m_stopping)
        return false;
    QVariantMap call;
    const QString trimmed = value.trimmed();
    QString phone=trimmed;
    if(m_phase=="phone")phone.remove(QRegExp("[\\s().-]"));
    if (m_phase == "phone" && QRegExp("\\+[0-9]{5,15}").exactMatch(phone)) {
        call = method("setAuthenticationPhoneNumber");
        call.insert("phone_number", phone);
    } else if (m_phase == "code" && !trimmed.isEmpty() && trimmed.size() <= 128) {
        call = method("checkAuthenticationCode");
        call.insert("code", trimmed);
    } else if (m_phase == "password" && !value.isEmpty() && value.size() <= 1024) {
        call = method("checkAuthenticationPassword");
        call.insert("password", value); // Whitespace is significant in passwords.
    } else if (m_phase == "email" && trimmed.contains('@') && trimmed.size() <= 320) {
        call = method("setAuthenticationEmailAddress");
        call.insert("email_address", trimmed);
    } else if (m_phase == "emailCode" && !trimmed.isEmpty() && trimmed.size() <= 128) {
        call = method("checkAuthenticationEmailCode");
        QVariantMap code = method("emailAddressAuthenticationCode");
        code.insert("code", trimmed);
        call.insert("code", code);
    } else {
        m_error = ru("Проверьте введённое значение. Номер нужен в формате +79991234567.");
        emit changed();
        return false;
    }
    return authenticate(call);
}

void TelegramSession::readOutput()
{
    m_readQueued = false;
    emit batchStarted();
    int processed = 0;
    while (processed < 64 && (m_process.bytesAvailable() > 0 || m_input.contains('\n'))) {
        if (!m_input.contains('\n')) m_input.append(m_process.read(65536));
        int end;
        while (processed < 64 && (end = m_input.indexOf('\n')) >= 0) {
            ++processed;
            QByteArray line = m_input.left(end);
            ++m_receivedEvents;m_receivedBytes+=line.size();
            m_input.remove(0, end + 1);
            if (line.endsWith('\r'))
                line.chop(1);
            QVariantMap object;
            if (line.size() > MaxLine || !JsonCodec::decode(line, &object)) {
                fail(ru("Движок вернул некорректный ответ."));
                emit batchFinished();
                return;
            }
            handle(object);
        }
        if (m_input.size() > MaxLine && !m_input.contains('\n')) {
            fail(ru("Ответ движка превышает допустимый размер."));
            emit batchFinished();
            return;
        }
    }
    emit batchFinished();
    if (!m_readQueued && (m_process.bytesAvailable() > 0 || m_input.contains('\n'))) {
        m_readQueued = true;
        QTimer::singleShot(0, this, SLOT(readOutput()));
    }
}

void TelegramSession::handle(const QVariantMap &object)
{
    const QString type = object.value("@type").toString();
    const QString extra = object.value("@extra").toString();
    if (!m_versionRequest.isEmpty() && extra == m_versionRequest) {
        if (type == "optionValueString")
            m_version = object.value("value").toString();
        m_versionRequest.clear();
        emit changed();
    }
    if (!m_pending.isEmpty() && extra == m_pending && type != "ok") {
        m_pending.clear();
        m_busy = false;
        m_timer.stop();
        if (type == "error") {
            // Whitelist symbolic errors. Never show or log raw requests/errors.
            QString message = object.value("message").toString();
            QRegExp flood("FLOOD_WAIT_([0-9]+)");
            if (message == "PHONE_CODE_INVALID" || message == "EMAIL_CODE_INVALID")
                m_error = ru("Неверный код. Попробуйте ещё раз.");
            else if (message == "PASSWORD_HASH_INVALID")
                m_error = ru("Неверный пароль двухэтапной проверки.");
            else if (message == "PHONE_CODE_EXPIRED")
                m_error = ru("Код истёк. Остановите движок и начните вход заново.");
            else if (message == "PHONE_NUMBER_INVALID")
                m_error = ru("Telegram не принял номер телефона.");
            else if (message == "API_ID_INVALID")
                m_error = ru("Telegram не принял api_id или api_hash.");
            else if (flood.exactMatch(message))
                m_error = ru("Telegram просит подождать %1 секунд.").arg(flood.cap(1));
            else
                m_error = ru("Telegram отклонил запрос (код %1). Проверьте параметры и соединение.")
                    .arg(object.value("code").toInt());
        }
        emit changed();
    }
    if (type == "updateAuthorizationState")
        authorization(object.value("authorization_state").toMap());
    emit eventReceived(object);
}

void TelegramSession::authorization(const QVariantMap &state)
{
    const QString type = state.value("@type").toString();
    if (m_stopping || m_failed)
        return;
    if (type == m_authorizationType)
        return;
    m_authorizationType = type;
    m_timer.stop();
    m_pending.clear(); // A state update can precede the response to its request.
    m_busy = false;
    m_error.clear();
    if (type == "authorizationStateWaitTdlibParameters") {
        setPhase("parameters", ru("Введите параметры приложения Telegram"));
        if(m_resumeRuntime){m_resumeRuntime=false;if(!m_busy && !m_runtimeApiId.isEmpty())configure(m_runtimeApiId,m_runtimeApiHash);}
    }
    else if (type == "authorizationStateWaitPhoneNumber")
        setPhase("phone", ru("Введите номер существующего аккаунта с кодом страны"));
    else if (type == "authorizationStateWaitCode")
        setPhase("code", ru("Введите код входа из Telegram или другого указанного им канала"));
    else if (type == "authorizationStateWaitPassword")
        setPhase("password", ru("Введите пароль двухэтапной проверки"));
    else if (type == "authorizationStateWaitEmailAddress")
        setPhase("email", ru("Telegram запрашивает адрес электронной почты"));
    else if (type == "authorizationStateWaitEmailCode")
        setPhase("emailCode", ru("Введите код из письма Telegram"));
    else if (type == "authorizationStateReady")
        setPhase("ready", ru("Вход выполнен. Чаты доступны на главном экране."));
    else if (type == "authorizationStateClosed")
        fail(ru("Telegram закрыл сессию. Можно запустить движок снова."));
    else if (type == "authorizationStateClosing" || type == "authorizationStateLoggingOut") {
        m_busy = true;
        setPhase("closing", ru("Telegram закрывает сессию…"));
        m_timer.start(12000);
    } else if (type == "authorizationStateWaitRegistration")
        setPhase("unsupported", ru("Нужна регистрация. Создайте аккаунт в официальном Telegram и повторите вход."));
    else
        setPhase("unsupported", ru("Этот способ входа пока не поддерживается. Остановите движок и используйте официальный Telegram."));
}

void TelegramSession::stop()
{
    if (m_process.state() == QProcess::NotRunning)
        return;
    m_stopping = true;
    m_busy = true;
    m_pending.clear();
    m_process.closeWriteChannel();
    setPhase("closing", ru("Закрытие движка с сохранением сессии…"));
    m_timer.start(12000);
}

void TelegramSession::fail(const QString &message)
{
    m_failed = true;
    m_error = message;
    m_pending.clear();
    m_busy = m_process.state() != QProcess::NotRunning;
    setPhase("failed", ru("Движок остановлен из-за ошибки"));
    if (m_busy) {
        m_process.closeWriteChannel();
        m_timer.start(12000);
    } else {
        m_timer.stop();
    }
}

void TelegramSession::discardErrorOutput()
{
    m_process.readAllStandardError(); // Do not persist TDLib diagnostics with account data.
}

void TelegramSession::processError(QProcess::ProcessError error)
{
    if (!m_stopping && !m_failed && error != QProcess::Crashed)
        fail(ru("Не удалось запустить движок или открыть канал связи. Проверьте комплект библиотек."));
}

void TelegramSession::finished(int code, QProcess::ExitStatus exitStatus)
{
    // readyRead can leave a bounded batch queued when the worker exits. Apply
    // its final send results before notifying the store that it disconnected.
    while (m_process.bytesAvailable()>0 || m_input.contains('\n')) readOutput();
    m_timer.stop();
    m_pending.clear();
    m_busy = false;
    if (m_stopping && !m_failed && code == 0 && exitStatus == QProcess::NormalExit)
        setPhase("stopped", ru("Движок остановлен. Сессия сохранена на устройстве."));
    else if (!m_failed) {
        m_failed = true;
        m_error = ru("Движок завершился неожиданно (код %1). Повторный запуск доступен.").arg(code);
        setPhase("failed", ru("Ошибка Telegram-движка"));
    } else
        emit changed();
    if(m_restartRequested)QTimer::singleShot(0,this,SLOT(restartAfterClose()));
}

void TelegramSession::reconnect(){
    if(m_restartRequested)return;
    m_restartRequested=true;
    if(m_process.state()==QProcess::NotRunning)restartAfterClose();else stop();
}
void TelegramSession::restartAfterClose(){
    if(!m_restartRequested || m_process.state()!=QProcess::NotRunning)return;
    m_restartRequested=false;m_resumeRuntime=true;start();
}
QString TelegramSession::diagnostics() const{
    return ru("Время работы: %1 мин\nСобытий: %2\nПолучено от движка: %3 КБ\nБуфер: %4 КБ\nОчередь передачи: %5 КБ")
        .arg(m_uptime.elapsed()/60000).arg(m_receivedEvents).arg(m_receivedBytes/1024).arg(m_input.size()/1024).arg(m_process.bytesToWrite()/1024);
}

void TelegramSession::timedOut()
{
    if (m_stopping || m_failed || m_phase == "closing") {
        m_failed = true;
        m_error = ru("Движок не завершился вовремя и был остановлен принудительно.");
        m_process.kill();
        return;
    }
    fail(ru("Нет ответа от движка. Сессия закрывается; после остановки можно повторить вход."));
}
