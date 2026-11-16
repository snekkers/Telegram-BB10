# Порт TDLib для BB10 ARMv7

Разработка и сборка выполнены на Mac M1 с 8 GiB RAM, 29–30 октября 2026.
Телефон и работающий BB10 GUI для этой стадии не использовались.
Проверки исполнения на QNX и входа в Telegram пока не выполнены.

**Полная `libtdjson.so` 1.8.69 для ARMv7 собрана и прошла строгую линковку.**
Размер — 40 612 312 байт. Проверены ARM EABI5 softfp, четыре обязательных
экспорта JSON C API и цепочка зависимостей. Подготовлен диагностический ZIP
`build/device/telegram-runtime.zip`, 17 984 350 байт.

Отчёт с SHA-256 каждого ELF, входной ревизией и хэшем патча:
`build/porting/verification.json`. В нём явно отмечено отсутствие проверки
исполнения на устройстве и входа в аккаунт.

## Инструменты

- Исходники TDLib 1.8.69: upstream `4c6377aa7386c39217878a9a56d581922ad37ca4`.
- OpenSSL 3.5.9 LTS и zlib 1.3.2 собраны из исходников.
- GNU toolchain 9.3.0 из [BerryCore](https://github.com/sw7ft/berrycore/tree/ffa18de9bf2867d5beb94d4a89e2930bee786d54/toolchain),
  архив проверен по SHA-256 из Git LFS.
- Основную компиляцию выполняет Apple Clang 21.0.0 на M1.
  GNU QNX linker, archiver и библиотеки GCC используются через Docker.
- Для CMake на Mac закреплён Kitware CMake 3.31.6 из PyPI.

URL, ревизии и SHA-256 входных архивов находятся в `porting/sources.json`.
Скачанные архивы, SDK и все продукты сборки остаются в игнорируемом `build/`.

Clang получает ARMv7 softfp, 4-байтные enum/wchar_t, заголовки BB10 и GNU C++9,
а также эмуляцию TLS, согласованную с runtime GCC. Triple `armv7-none-eabi`
используется для генерации ELF-объектов. QNX-макросы, заголовки, CRT и линкер
выбираются явно; конечные программы используют `/usr/lib/ldqnx.so.2`.
Это не приложение для bare-metal ARM.

Название `qnx800` — каталог community toolchain. Заголовки и системные
библиотеки в нём относятся к BB10 10.3.1; современный QNX SDP 8 для телефона
не используется.

## Подтверждённые промежуточные результаты

| Компонент | Проверка |
| --- | --- |
| C++17 | GCC 9.3 и Clang собирают и линкуют ARM-программу с optional, atomic, mutex и thread |
| ABI | ARM EABI5 softfp; проверки размеров и выравнивания при компиляции |
| OpenSSL 3.5.9 | Статические ARM-библиотеки crypto и TLS собраны |
| OpenSSL probe | Программа с RAND_bytes, SHA-256 и TLS context слинкована для BB10; не запускалась |
| zlib 1.3.2 | Статическая ARM-библиотека собрана |
| tdutils | Собрана для ARM с OpenSSL и zlib |
| SQLite / tdactor / tdnet / tddb / tde2e / tdmtproto | Собраны для ARM |
| Полная libtdjson | Собрана; строгая проверка внешних символов и экспортов C API прошла |
| ARM EABI memory helpers | 12 контрактных проверок на Mac прошли; исполнение на ARM не проверено |

Логи находятся в `build/porting/logs/`. Успешная компиляция и линковка не
подтверждают работу потоков, poll/socketpair, базы данных или источника энтропии
на конкретной прошивке телефона.

Финальная линковка использует `--no-undefined` и
`--no-allow-shlib-undefined`. Обнаруженное при первой проверке смешение
заголовков старого OpenSSL из SDK и новых библиотек устранено порядком include
paths. Перед упаковкой проверены 713 файлов зависимостей компилятора:
старые заголовки OpenSSL/zlib из SDK не используются.

## Изменения для QNX

`porting/tdlib-qnx.patch` содержит изменения для закреплённой ревизии TDLib:

- Отдельная платформа `TD_QNX`, POSIX `poll` и пробуждение через `socketpair`.
- Вместо недоступного `fdopendir` используется существующий обход по пути
  через `opendir`. Гонки при изменении каталога и поведение на устройстве
  требуют отдельной проверки.
- Порядок включения `sys/time.h` и `sys/resource.h` исправлен.
- Неподдерживаемый alternate signal stack возвращает ошибку; используются
  только имеющиеся флаги `sigaction`.
- Время файлов берётся с секундной точностью, предоставляемой BB10 `stat`.

Для Clang создаются приватные overlays заголовков внутри `build/porting/`.
Они исправляют C++-перегрузки строковых функций из community sysroot и
самодостаточность `sys/resource.h`. Исходный SDK не меняется. Проверочная
программа контролирует const/mutable перегрузки строковых функций.

`porting/aeabi-memory.c` предоставляет отсутствующие в системной libc вызовы
копирования/очистки памяти ARM EABI. Он компилируется GCC с `-fno-builtin`,
чтобы исключить рекурсию при перенаправлении на `memcpy/memmove/memset`.

Конфигурация OpenSSL не использует утилиты, syslog BIO, ассемблерные оптимизации,
динамические модули и async API на `setcontext`. Проверка TLS-сертификатов
этими настройками не отключается. Эффективность криптографии, работа энтропии
и доверенные CA на BB10 требуют проверки исполнения.

Крупные файлы TDLib разделяются официальным `SplitSource.php` с seed 0.
Оригиналы остаются в каталоге исходников; повторное разбиение предотвращает
маркер `.berrygram-split-v1`.

## Повторить сборку на M1

Нужны Xcode Command Line Tools и запущенный Docker Desktop. Из корня проекта:

```sh
python3 scripts/prepare-qnx-port.py
python3 -m pip install --no-deps --require-hashes \
  --target build/tools/cmake-native -r porting/requirements-macos.txt
docker build --platform linux/amd64 -t berrygram/porting:bookworm porting
bash scripts/run-qnx-port.sh generate
bash scripts/build-qnx-native.sh probe
bash scripts/build-qnx-native.sh dependencies
bash scripts/build-qnx-native.sh openssl-probe
bash scripts/build-qnx-native.sh tdjson
python3 scripts/stage-qnx-runtime.py
```

Стадия скачивания проверяет закреплённые хэши и применяет QNX-патч.
Компиляционные контейнеры работают без сети. Основная компиляция идёт
нативно на M1; Docker нужен для GNU ELF tools и генерации исходников.
Сборка рассчитана на ARMv7. Библиотека TDLib для x86-симулятора пока не собрана.

Эта библиотека пока не подключена к Cascades и не включена в демо-BAR.

## Проверка на телефоне позже

Комплект формируется в `build/device/telegram-runtime/` после успешной
строгой линковки. В нём должны находиться отдельный C-процесс, TDLib,
GNU C++ runtime и программы диагностики. Это не установочный BAR.
Он не включается автоматически в текущий демо-интерфейс.

При доступном способе запуска нативных программ на устройстве:

```sh
sh run-probe.sh
```

Скрипт сначала проверяет C++-потоки, затем источник энтропии, контрольный SHA-256
и создание TLS context. Только после их успеха запускается TDLib без параметров
аккаунта: ожидаются версия, `authorizationStateWaitTdlibParameters` и закрытие.
Проверка не создаёт авторизованную сессию и не требует `api_id/api_hash`.

GNU C++ runtime загружается только в дочернем процессе. Смешивать его с
Dinkum/Cascades в одном UI-процессе не требуется. В дальнейшем отдельно
проверяются файловые операции и восстановление БД, сеть, сохранение сессии,
авторизация и интеграция с интерфейсом.
