# Порт TDLib для BB10 ARMv7

Разработка и первоначальная сборка выполнены на Mac M1 с 8 GiB RAM,
29–30 октября 2026. 12 ноября исправленный движок 0.6.1 проверен на физическом
Classic SQC100-4 с BB10 10.3.3.3216. Вход в Telegram ещё не проверен.

**Полная `libtdjson.so` 1.8.69 для ARMv7 собрана и прошла строгую линковку.**
Проверены ARM EABI5 softfp, четыре обязательных экспорта JSON C API и цепочка
зависимостей. Подготовлен диагностический ZIP `build/device/telegram-runtime.zip`.

Отчёт с SHA-256 каждого ELF, входной ревизией и хэшем патча:
`build/porting/verification.json`. Исполнение на устройстве подтверждается
`build/porting/device-tests.json`; отчёт привязан к SHA-256 всех шести ELF.
Вход в аккаунт отмечен отдельно как непроверенный.

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
| C++17 | optional, atomic, mutex и thread проверены исполнением на Classic |
| ABI | ARM EABI5 softfp; проверки размеров и выравнивания при компиляции |
| OpenSSL 3.5.9 | Статические ARM-библиотеки crypto и TLS собраны |
| OpenSSL probe | RAND_bytes, SHA-256 и создание TLS context прошли на Classic; сетевой TLS не проверен |
| zlib 1.3.2 | Статическая ARM-библиотека собрана |
| tdutils | Собрана для ARM с OpenSSL и zlib |
| SQLite / tdactor / tdnet / tddb / tde2e / tdmtproto | Собраны для ARM |
| Полная libtdjson | Собрана; строгая проверка внешних символов и экспортов C API прошла |
| TDLib на Classic | Версия 1.8.69, JSON с русским текстом/эмодзи, UTF-16 offsets и штатное закрытие прошли |
| ARM EABI memory helpers | 12 контрактных проверок на Mac прошли; исполнение на ARM не проверено |

Логи первоначальной сборки находятся в `build/porting/logs/`, повторной —
в `build/porting/header-fix-build.log`. Успешная компиляция и линковка сами по себе
не подтверждают исполнение. Отдельные проверки на Classic покрывают запуск
потоков, источник энтропии и обработку запросов; база аккаунта и сеть требуют
проверки после получения собственных API-параметров.

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
программа контролирует const/mutable перегрузки строковых функций, включая
вызовы через volatile-указатели с существующим и отсутствующим символом.

В первом запуске на телефоне найдена ошибка overlay: без макроса
`__CORRECT_ISO_CPP_STRING_H_PROTO` GNU `<cstring>` повторно определял mutable
wrappers поверх уже объявленных перегрузок с assembler aliases. Результат —
рекурсивные builtin-вызовы и ошибочная оптимизация JSON-парсера: даже `{}`
возвращал `Unexpected string end`. В 0.6.1 overlay объявляет наличие правильных
ISO-перегрузок до включения `<cstring>`, вся TDLib пересобрана. Исправление
проверено на физическом Classic сначала отдельным парсером, затем полным
процессом `berrygram-tdjson`. ABI, библиотека памяти и upstream TDLib для
исправления не менялись.

`porting/aeabi-memory.c` предоставляет отсутствующие в системной libc вызовы
копирования/очистки памяти ARM EABI. Он компилируется GCC с `-fno-builtin`,
чтобы исключить рекурсию при перенаправлении на `memcpy/memmove/memset`.

Конфигурация OpenSSL не использует утилиты, syslog BIO, ассемблерные оптимизации,
динамические модули и async API на `setcontext`. Проверка TLS-сертификатов
этими настройками не отключается. Работа энтропии проверена на Classic;
эффективность криптографии, доверенные CA и сетевое соединение ещё не проверены.

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

Библиотека включена в ARMv7 BAR, запускается отдельным процессом из Cascades.

## Проверка на телефоне

Комплект формируется в `build/device/telegram-runtime/` после успешной
строгой линковки. В нём должны находиться отдельный C-процесс, TDLib,
GNU C++ runtime и программы диагностики. Это не установочный BAR.
Его основные runtime-файлы также включаются в ARMv7 BAR.

При доступном способе запуска нативных программ на устройстве:

```sh
sh run-probe.sh
```

Скрипт сначала проверяет C++-потоки, затем источник энтропии, контрольный SHA-256
и создание TLS context. Только после их успеха запускается TDLib без параметров
аккаунта: ожидаются версия, `authorizationStateWaitTdlibParameters` и закрытие.
Проверка не создаёт авторизованную сессию и не требует `api_id/api_hash`.

Для USB-подключённого Classic с индивидуальным SSH-ключом и закреплённым
ключом сервера проверенный сценарий можно повторить с Mac:

```sh
python3 tests/classic-runtime-tests.py \
  --identity /path/to/classic_rsa --known-hosts /path/to/known_hosts
python3 scripts/stage-qnx-runtime.py --device-report build/porting/device-tests.json
```

Требуется Paramiko 3.5.1; для отдельной установки есть параметр
`--paramiko-path`. Тест выполняет программы от `devuser` во временном каталоге,
передаёт JSON частями, проверяет версию, Unicode и закрытие, затем удаляет
свои временные файлы. Данные аккаунта не использует. Без `--device-report`
упаковщик честно оставляет `device_execution_tested: false`; с отчётом сверяет
все хэши и три обязательных успешных проверки до копирования файлов.

GNU C++ runtime загружается только в дочернем процессе. Смешивать его с
Dinkum/Cascades в одном UI-процессе не требуется. В дальнейшем отдельно
проверяются файловые операции и восстановление БД, сеть, сохранение сессии,
авторизация и интеграция с интерфейсом.
