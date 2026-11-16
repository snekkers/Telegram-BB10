# BB10 на Windows с Intel/AMD

## Что требуется

| Компонент | Для чего |
| --- | --- |
| Momentics IDE for BlackBerry 2.1.2 | Работа с историческим BB10 SDK; IDE и SDK имеют разные версии |
| BlackBerry Native SDK 10.3.1 | Qt 4, Cascades, QNX-компилятор, qmake и упаковка BAR |
| BlackBerry 10 Device Simulator 10.3.1 | x86-образ BB10 с поддержкой Classic в Controller |
| VMware Workstation на Windows | Запуск виртуальной машины симулятора |

Нужен именно SDK для **BlackBerry 10**, а не современный QNX Software Development
Platform. Установка одного Momentics не гарантирует наличие целевой платформы.
Это старые инструменты: их работу на имеющейся версии Windows нужно подтвердить
первым запуском и сборкой.

## Получение инструментов

Официальные страницы загрузки BB10 устарели. Во время исследования ссылки
`developer.blackberry.com/native/downloads/` не открылись. Не следует рассчитывать,
что встроенное скачивание SDK в Momentics продолжает работать.

- [Сохранённая страница загрузок BlackBerry](https://docs.waitberry.com/native/download/)
  помогает сверить исторические версии; это зеркало, не актуальный сервис BlackBerry.
- Архивы [Native SDK for BlackBerry 10](https://archive.org/details/native-SDK-for-blackberry10),
  [bbdevtools](https://archive.org/details/bbdevtools) и
  [BlackBerry 10 Device Simulator](https://archive.org/details/blackberry10-device-simulator)
  содержат установщики и компоненты, перечисленные ниже. Их наличие и размеры
  проверены по метаданным Internet Archive; бинарные файлы не скачивались
  и не запускались.
- [Официальное руководство симулятора](https://developer.blackberry.com/devzone/files/develop/simulator/BB10_Device_Simulator_UG.pdf)
  остаётся доступным.
- [Документация VMware Workstation](https://techdocs.broadcom.com/us/en/vmware-cis/desktop-hypervisors/workstation-pro.html)
  — установка гипервизора для Windows. Совместимость современного Workstation
  с конкретным старым образом BB10 пока не проверена.

Найденные файлы для Windows:

| Компонент | Ссылка | Размер, приблизительно |
| --- | --- | --- |
| Momentics 2.1.2 x64 | [Установщик IDE](https://archive.org/download/native-SDK-for-blackberry10/Momentics%20IDE/momentics-2.1.2-201503050937.win32.x86_64.setup.exe) | 391 MB |
| SDK host tools | [bbndk.win32.tools.10.3.1.12.zip](https://archive.org/download/bbdevtools/bbndk.win32.tools.10.3.1.12.zip) | 305 MB |
| SDK target libraries | [bbndk.win32.libraries.10.3.1.995.zip](https://archive.org/download/bbdevtools/bbndk.win32.libraries.10.3.1.995.zip) | 1 608 MB |
| Cascades host support | [bbndk.win32.cshost.10.3.1.995.zip](https://archive.org/download/bbdevtools/bbndk.win32.cshost.10.3.1.995.zip) | 21 MB |
| QNX make configuration | [bbndk.win32.qconfigmk.10.3.1.995.zip](https://archive.org/download/bbdevtools/bbndk.win32.qconfigmk.10.3.1.995.zip) | 5 KB |
| Simulator 10.3.1.2744 | [Установщик симулятора для Windows](https://archive.org/download/blackberry10-device-simulator/BlackBerry10Simulator-Installer-BB10_3_1-2744-Win-201504012145.exe) | 1 156 MB |

Контрольные суммы из архивных метаданных сохранены в `toolchain-downloads.json`
рядом с этой инструкцией. Они помогают сверить загрузку с архивом; это не
независимое подтверждение подписи исходного поставщика.

Оригинальные URL BlackBerry для tools, libraries, simulator и Controller
вернули HTTP 404 при проверке 28 октября 2026 года.

## Настройка SDK

1. Установить Momentics. Распаковать tools в `C:\bbndk_api`, чтобы появился
   каталог `host_10_3_1_12`. Туда же распаковать libraries: появится
   `target_10_3_1_995`. Дополнения cshost и qconfigmk распаковать с объединением
   их содержимого с тем же каталогом target.
2. В Momentics открыть **Help → Update API Levels → Custom → Add New Custom SDK**.
   Указать Host Path `C:\bbndk_api\host_10_3_1_12` и Target Path
   `C:\bbndk_api\target_10_3_1_995`. Такая ручная установка не требует
   скачивания API level через IDE.
3. Найти файл `bbndk-env_*.bat` установленной платформы. Открыть `cmd.exe`
   и выполнить его через `call`:

   ```bat
   call C:\bbndk\bbndk-env_10_3_1_995.bat
   where qmake
   qmake -query QT_VERSION
   where make
   where blackberry-nativepackager
   where blackberry-deploy
   ```

4. Проверить, что `qmake` относится к BB10 Qt 4. Системный Qt 5/6 для этого
   проекта не подходит. Фактический каталог установки может отличаться.

Использовать `cmd.exe`: запуск `.bat` из PowerShell не переносит изменения
окружения обратно в родительский процесс PowerShell.

## Симулятор

1. Установить VMware Workstation на Windows-ПК с Intel/AMD и включённой
   аппаратной виртуализацией.
2. Развернуть сохранённый пакет BB10 Device Simulator и открыть его VM в VMware.
   Нужен образ симулятора, не прошивка телефона и не autoloader.
3. Загрузить BB10. Если пакет содержит отдельный Controller, запустить его
   и подключить к IP виртуальной машины.
4. Выбрать квадратный профиль/разрешение 720 × 720 для проверки раскладки Classic.
   В Controller открыть **Classic**: там моделируются трекпад и аппаратные кнопки.
   Названия выбора экранного профиля могут отличаться между сборками.
5. Записать IP из нижней части окна симулятора. Официальное руководство
   предупреждает не использовать адрес Development Mode из системных настроек
   вместо адреса виртуальной машины.

Машина должна быть доступна с Windows-хоста по виртуальной сети VMware. Для
первого запуска достаточно сети NAT/host-only с доступом хоста к гостю;
интернет понадобится при подключении Telegram.

## Сборка и установка

Скопировать весь репозиторий на Windows, например в `C:\src\Blackberry`.
В том же `cmd.exe`, где уже вызван файл окружения SDK:

```bat
cd /d C:\src\Blackberry
scripts\build-simulator.cmd
```

Скрипт последовательно запускает qmake с `blackberry-x86-qcc`, make и
blackberry-nativepackager с конфигурацией `Simulator-Debug` и `-devMode`.
Он не устанавливает SDK и не обращается к серверу подписи.

После успешного создания BAR подставить настоящий IP симулятора:

```bat
blackberry-deploy -installApp -launchApp -device SIMULATOR_IP -package build\simulator\berrygram-simulator.bar
```

Если конкретная сборка симулятора требует пароль, добавить `-password` с его
настроенным значением. Не использовать здесь пароль Telegram.

**В этой сессии команды под Windows не запускались.** Если qmake не находит
`blackberry-x86-qcc`, сначала проверить версию SDK и каталог `mkspecs`, а не
подменять его обычным настольным Qt.

## Проверка первого запуска

1. Приложение открывает три демонстрационных чата и явно указывает режим демо.
2. Выбор каждого чата открывает правильную историю и заголовок.
3. Кириллица отображается корректно; длинное сообщение переносится по строкам.
4. Непустой текст добавляется кнопкой отправки и клавишей Enter/Send.
   Пустой текст не добавляется.
5. Возврат в список и повторный вход сохраняют локальную историю до завершения
   приложения. Полный перезапуск очищает её.
6. В Controller → Classic проверить прокрутку, выбор трекпадом и аппаратную Back.
   Если стандартной навигации Cascades недостаточно, добавить обработку после
   подтверждения поведения на симуляторе.

## Физический Classic

Симулятор использует x86, телефон — ARMv7. BAR для симулятора на телефон
не устанавливается: необходима отдельная сборка `Device-Debug`.

Кроме сборки, нужно подтвердить способ установки нового BAR на конкретную
прошивку телефона. Старый сценарий получения ключей и debug token зависит
от закрытой инфраструктуры BlackBerry. Есть современные проекты, которые
используют устройства с отключённой проверкой подписи; этот путь нужно
выбирать и проверять отдельно, когда появится телефон.

Свидетельство автора о запуске нового нативного приложения на Classic:
[PocketJS: BlackBerry Classic](https://pocketjs.pocket.nexus/blog/blackberry-classic/).
Это не проверка устройства пользователя.
