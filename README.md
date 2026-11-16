# Berrygram для BlackBerry Classic

Рабочий проект нативного Telegram-клиента для BB10: C++, Qt 4 и Cascades.
Первое целевое устройство — BlackBerry Classic. Рабочее название можно поменять.

## Состояние

Добавлен исходный каркас: список демонстрационных чатов, экран переписки,
локальное добавление сообщения кнопкой или клавишей ввода, возврат через
NavigationPane. Данные живут в памяти и сбрасываются после завершения приложения.
Интерфейс явно обозначает демонстрационный режим.

**29 октября 2026 года обе сборки выполнены на Mac с M1** через Linux amd64
контейнер с BlackBerry Native SDK 10.3.1.995:

| Цель | Результат | Размер |
| --- | --- | --- |
| BB10 x86 Simulator-Debug | `build/simulator/berrygram-simulator.bar` | 248 528 байт |
| BB10 ARMv7 Device-Debug | `build/device/berrygram-device.bar` | 249 671 байт |

Проверены успешное завершение компилятора и упаковщика, целостность ZIP,
архитектура ELF внутри каждого BAR, manifest и соответствие QML исходникам.
Отчёт сохранён в `build/verification.json`.

**Подключения к Telegram пока нет. Интерфейс в BB10 и установка на физический
Classic ещё не проверены.** Это development-пакеты; сборка не подтверждает
возможность установки на обычный телефон без подходящего способа загрузки BAR.

**Проверка UTM на M1:** образы BB10 10.3.2 и 10.3.1 загружают QNX, но во всех
проверенных конфигурациях останавливаются на `START SERVICE: screen`.
Safe Mode с профилем Classic 720×720 и смена CPU на Intel Nehalem не помогли.
Рабочий графический симулятор на M1 пока не получен; BAR в него не установлен.
Конфигурации, источники и результаты описаны в [отчёте UTM](docs/utm-experiment.md).

**Telegram-движок без GUI:** добавлен локальный процесс `berrygram-tdjson`.
На M1 прошли восемь интеграционных сценариев с настоящей TDLib 1.8.69:
начальное состояние авторизации, JSON-запросы, Unicode и закрытие клиента.
Этот процесс также собран для BB10 x86 и ARMv7. Сама TDLib для QNX ещё
не собрана; вход в аккаунт и подключение к интерфейсу не выполнены.
[Проверка движка и следующие ограничения](docs/telegram-engine.md).

## Первый запуск

Для Mac с M1 подготовлены [сборка через Docker и заметки по UTM](docs/macos-m1.md).
С установленным и запущенным Docker Desktop:

```sh
bash scripts/build-docker.sh simulator
bash scripts/build-docker.sh device
```

Для имеющегося Windows-ПК с Intel/AMD подготовлена
[инструкция по SDK, симулятору и сборке](docs/windows-setup.md).
После настройки NDK из `cmd.exe`:

```bat
call C:\bbndk\bbndk-env_10_3_1_995.bat
scripts\build-simulator.cmd
```

Путь и имя файла окружения зависят от установленного SDK.
Скрипт должен создать `build\simulator\berrygram-simulator.bar`.
Это команда для проверки на Windows, а не отчёт об успешной сборке.

## Что дальше

1. Проверить исходный VMware Device Simulator на имеющемся Windows-ПК с Intel/AMD
   и установить собранный x86 BAR. Для UTM потребуется отдельное исследование
   совместимости графической службы; перебор основных настроек результата не дал.
2. Проверить квадратный экран, ввод текста, трекпад и Back/Menu в режиме Classic.
3. Проверить возможность сборки актуального Telegram-движка для QNX/BB10.
   Нужны отдельные библиотеки для x86-симулятора и ARMv7-телефона.
4. Подключить авторизацию Telegram, список настоящих чатов и текстовые сообщения.
5. Добавить изображения, файлы, сохранение истории и восстановление соединения.
6. На физическом Classic проверить установку BAR, клавиатуру, фоновые ограничения
   и расход батареи; затем заниматься Hub и уведомлениями.

Подход к Telegram и технические ограничения описаны в
[заметках по архитектуре](docs/architecture.md).

## Файлы

- `berrygram.pro` — проект qmake для BB10 Qt 4/Cascades.
- `src/ChatController.*` — демонстрационные данные и модель для QML.
- `assets/main.qml`, `assets/ChatPage.qml` — нативные экраны.
- `bar-descriptor.xml` — конфигурации x86 Simulator-Debug и ARMv7 Device-Debug.
- `scripts/build-simulator.cmd` — сборка и упаковка под Windows.
- `scripts/build-docker.sh`, `scripts/container-build.sh` — Linux SDK через Docker,
  включая запуск x86-инструментов на Mac с M1.
- `engine/tdjson-host.c` — отдельный процесс для JSON C API TDLib.
- `scripts/build-telegram-host.sh` — сборка процесса для macOS, BB10 x86 и ARMv7.
- `tests/telegram-host-integration.py` — проверки с настоящей TDLib без аккаунта.

## Проверенные источники

Проверка сборки на M1: 29 октября 2026 года. Ссылки на архивные установщики
проверены 28 октября. Совместимость установщиков с конкретной версией Windows
пока не проверена.

- [Официальное руководство BB10 Device Simulator](https://developer.blackberry.com/devzone/files/develop/simulator/BB10_Device_Simulator_UG.pdf):
  запуск приложений без телефона, Controller, режим Classic с трекпадом и кнопками.
- [Официальные примеры Cascades](https://github.com/blackberry/Cascades-Samples):
  структура нативного приложения, qmake, QML и BAR descriptor.
- [Документация Qt по BB10](https://wiki.qt.io/Qt-Creator-with-BlackBerry-10):
  Cascades работает с Qt 4; mkspec симулятора — `blackberry-x86-qcc`.
- [Ограничения VMware Fusion на Apple silicon](https://knowledge.broadcom.com/external/article/315602):
  x86-гости не запускаются в Fusion на M1; UTM/QEMU — отдельный путь эмуляции.
- [Сборка Term49C через Docker](https://github.com/jxw1102/Term49C#building-with-docker):
  community-подход к запуску Linux SDK на Apple silicon. Собственная сборка
  нашего приложения этим способом также проверена.
- [TDLib](https://github.com/tdlib/td): актуальный исходный код и требования к сборке.
- [Получение Telegram API ID](https://core.telegram.org/api/obtaining_api_id).
- [Архивный проект arfeo/Telega](https://github.com/arfeo/Telega):
  пример ранее начатого BB10-клиента. Его текущая работоспособность не проверена.
