# Berrygram для BlackBerry Classic

Рабочий проект нативного Telegram-клиента для BB10: C++, Qt 4 и Cascades.
Первое целевое устройство — BlackBerry Classic. Рабочее название можно поменять.

## Состояние

Добавлен исходный каркас: список демонстрационных чатов, экран переписки,
локальное добавление сообщения кнопкой или клавишей ввода, возврат через
NavigationPane. Данные живут в памяти и сбрасываются после завершения приложения.
Интерфейс явно обозначает демонстрационный режим.

**Подключения к Telegram пока нет. Сборка в BB10 SDK и запуск в симуляторе
ещё не выполнены.** В доступном окружении Mac M1 нет BB10 SDK; Windows-ПК
пользователя не подключён к этому рабочему окружению.

## Первый запуск

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

1. Собрать и запустить каркас в BB10 Device Simulator 10.3.1.
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

## Проверенные источники

Проверка ссылок и требований: 28 октября 2026 года. Это сведения из документации;
совместимость установщиков с конкретной версией Windows пока не проверена.

- [Официальное руководство BB10 Device Simulator](https://developer.blackberry.com/devzone/files/develop/simulator/BB10_Device_Simulator_UG.pdf):
  запуск приложений без телефона, Controller, режим Classic с трекпадом и кнопками.
- [Официальные примеры Cascades](https://github.com/blackberry/Cascades-Samples):
  структура нативного приложения, qmake, QML и BAR descriptor.
- [Документация Qt по BB10](https://wiki.qt.io/Qt-Creator-with-BlackBerry-10):
  Cascades работает с Qt 4; mkspec симулятора — `blackberry-x86-qcc`.
- [Ограничения VMware Fusion на Apple silicon](https://knowledge.broadcom.com/external/article/315602):
  x86-гости не запускаются в Fusion на M1; для симулятора выбран Windows-ПК.
- [TDLib](https://github.com/tdlib/td): актуальный исходный код и требования к сборке.
- [Получение Telegram API ID](https://core.telegram.org/api/obtaining_api_id).
- [Архивный проект arfeo/Telega](https://github.com/arfeo/Telega):
  пример ранее начатого BB10-клиента. Его текущая работоспособность не проверена.
