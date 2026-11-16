# BB10 в UTM на M1

Эксперимент с запуском BB10 10.3.2 и 10.3.1 через эмуляцию x86.
Дата: 29 октября 2026. Хост: Apple M1, 8 GiB RAM, macOS 27.0.1.
Сборка нашего BAR уже проверена отдельно; этот документ относится к самой ОС.

## Результат

**QNX загружается, графический интерфейс BB10 не запустился.** Во всех
проверенных вариантах консоль остаётся на `START SERVICE: screen`.
Собранный BAR в UTM не установлен, приложение в BB10 ещё не проверено.

| Образ | CPU | Графика / сеть | Режим | Наблюдение |
| --- | --- | --- | --- | --- |
| Архивный 10.3.2 | qemu64 | VMware SVGA, 16 MiB / e1000 | Обычный, профиль по умолчанию | Остановка на `screen` |
| Архивный 10.3.2 | qemu64 | VMware SVGA, 128 MiB / pcnet | Обычный, профиль по умолчанию | Остановка на `screen` |
| SDK 10.3.1.995 | qemu64 | VMware SVGA, 128 MiB / pcnet | Обычный, профиль по умолчанию | Остановка на `screen` |
| SDK 10.3.1.995 | qemu64 | VMware SVGA, 128 MiB / pcnet | Safe Mode, Classic 720×720 | Остановка на `screen` |
| SDK 10.3.1.995 | Nehalem | VMware SVGA, 128 MiB / pcnet | Обычный, профиль по умолчанию | Остановка на `screen` |
| SDK 10.3.1.995 | Nehalem | VMware SVGA, 128 MiB / pcnet | Safe Mode, Classic 720×720 | Остановка на `screen` |

Последний Safe Mode выбран вручную в меню гостя: Enter → 6 → Enter → 5 → Enter.
Выбор подтверждён скриншотом, после чего загрузка наблюдалась несколько минут.
При старте также видна строка `Memory fault`; её связь с остановкой `screen`
не установлена. Обе ВМ после испытаний остановлены.

Проверки дисков `qemu-img check` прошли. Это подтверждает структуру QCOW2,
но не совместимость гостевых драйверов с виртуальным оборудованием QEMU.
Принадлежность проблемы графическому драйверу остаётся гипотезой; точная причина
пока не установлена.

В `build/emulation/trials/` сохранены конфигурации, QEMU debug logs и снимки
консоли. Итог последнего испытания:
`trial-06-intel-safe-result.json`, `trial-06-intel-safe-screenshot.png`.
Debug log QEMU содержит параметры запуска; это не журнал ошибок службы `screen`.

Loopback-перенаправления принимают TCP-соединения со стороны Mac, но это само
по себе не подтверждает доступность сервисов в госте. Баннер SSH/Telnet не получен.
Проба `blackberry-deploy -listDeviceInfo` через `host.docker.internal:8443`
завершилась по таймауту 15 секунд; поддержка такого синтаксиса порта SDK также
не подтверждена, поэтому по этой пробе нельзя установить причину отсутствия связи.

## Инструменты и образ

- Установлен UTM 5.0.8 из [официального релиза](https://github.com/utmapp/UTM/releases/tag/v5.0.8).
  SHA-256 DMG совпал с опубликованным GitHub digest:
  `7b2d7b2e2a02b3de1757ef19a8f36573e6d9cceae8c8cca0abcbcaa4a852019a`.
  Проверки `codesign` и `spctl` прошли; macOS определила приложение как
  `Notarized Developer ID`.
- Источник диска:
  [Black Berry 10 Simulator BB10.3.2](https://archive.org/details/black-berry-10-simulator-bb-10-3-2).
  Автор описывает его как конвертированный для UTM. Ожидаемые размер и SHA-1
  взяты из метаданных архива: 2 135 883 776 байт,
  `a58392e402ae6de197faa428cb172a998c2f6046`.

Архивная контрольная сумма подтверждает соответствие загрузки архиву,
а не независимую подлинность исходной прошивки BlackBerry.

Для контрольного испытания использован исходный simulator из закреплённого
Docker-образа SDK:

```text
delaya73/bbndk@sha256:26334b9fe9fe3e8d5984a814e2ebadbef94838994dbb768efd78f7b3da418bc4
/home/admin/bin/bbndk/simulator_10_3_1_995/
```

Папка скопирована в `build/emulation/vendor-10.3.1/`. Исходные VMDK и VMX
сохранены. VMX задаёт 2048 MiB RAM, IDE-диск, VMware SVGA с 128 MiB VRAM
и включённым `mks.enable3d`. Тип сетевого адаптера в VMX явно не задан;
`pcnet` в контрольной ВМ QEMU — выбранный для эксперимента вариант.

Для преобразования использована библиотека qemu-img из установленного UTM.
Обёртка `scripts/utm-qemu-img.sh` собирает небольшой launcher через `clang`.
Проверенная версия qemu-img — 10.0.12; путь к библиотеке рассчитан на UTM 5.0.8.

```sh
bash scripts/utm-qemu-img.sh convert -f vmdk -O qcow2 \
  build/emulation/vendor-10.3.1/BlackBerry10Simulator.vmdk \
  build/emulation/BlackBerry10Simulator-SDK-10.3.1.qcow2
bash scripts/utm-qemu-img.sh check \
  build/emulation/BlackBerry10Simulator-SDK-10.3.1.qcow2
```

Получен QCOW2 размером 1 912 274 944 байта, виртуальный размер 8 GiB.
Конвертированный исходник сохранён отдельно от диска контрольной ВМ
`build/emulation/BB10 SDK 10.3.1.utm/Data/bb10.qcow2`.

## Подготовка

Следующие команды воспроизводят начальный эксперимент, который **не дал
работающего графического симулятора**. Скрипт не является готовой настройкой BB10.

Из каталога проекта:

```sh
bash scripts/download-bb10-image.sh
python3 scripts/create-utm-vm.py build/emulation/BlackBerry10SimulatorBB10_3_2.qcow2
open "build/emulation/BB10 Classic.utm"
/Applications/UTM.app/Contents/MacOS/utmctl list
```

Скрипт загрузки умеет продолжать оборванную передачу и проверяет размер и SHA-1.
Скрипт создания ВМ повторно проверяет диск и отказывается перезаписывать
существующую ВМ. На APFS он создаёт отдельный clone файла диска.

## Начальная конфигурация

| Параметр | Значение |
| --- | --- |
| Backend | QEMU, эмуляция x86_64 |
| Machine | PC/i440FX, VMware backdoor `vmport=on` |
| CPU | `qemu64`, 1 ядро |
| RAM / JIT cache | 2048 / 256 MiB |
| Firmware | BIOS, UEFI выключен |
| Display | `vmware-svga` |
| Disk interface | IDE |
| Network | e1000, user-mode NAT |
| Input | PS/2 и USB 2.0 |

Буфер обмена, общие папки, звук и USB sharing выключены. Порты перенаправлены
только на loopback Mac:

| Mac | BB10 | Назначение |
| --- | --- | --- |
| `127.0.0.1:8443` | TCP 443 | Инструменты установки |
| `127.0.0.1:2222` | TCP 22 | SSH/отладка, если сервис доступен |
| `127.0.0.1:2323` | TCP 23 | Консоль симулятора, если сервис доступен |

Это выбранная для эксперимента конфигурация. Наличие порта в настройках ещё
не подтверждает, что соответствующий сервис запущен внутри гостевой ОС.

## Запуск и диагностика

```sh
/Applications/UTM.app/Contents/MacOS/utmctl start "BB10 Classic"
/Applications/UTM.app/Contents/MacOS/utmctl status "BB10 Classic"
```

QEMU debug log должен появиться внутри `build/emulation/BB10 Classic.utm/Data`.
Проверка загрузки ОС, графики и установка приложения выполняются после
успешного завершения загрузки диска; один статус `started` не считается
подтверждением работающего BB10.

## Следующая проверка на Windows

Исходный VMware-комплект уже находится на Mac:
`build/emulation/vendor-10.3.1/BlackBerry10Simulator.vmx` и файлы
`BlackBerry10Simulator*.vmdk` рядом. Для переноса нужны VMX и все части VMDK;
одного маленького файла `BlackBerry10Simulator.vmdk` недостаточно.
Оригинальный комплект пока не запускался в VMware на Windows.

1. Скопировать папку `vendor-10.3.1` на Windows-ПК с Intel/AMD. Вложенный
   `controllers` содержит Linux-бинарники; для Windows понадобится отдельный
   Windows Controller из установщика симулятора.
2. Открыть `BlackBerry10Simulator.vmx` в VMware Workstation и проверить загрузку
   обычного режима или Safe Mode → Classic 720×720.
3. Скопировать `build/simulator/berrygram-simulator.bar` из проекта. Для запуска
   уже собранного BAR повторная компиляция не требуется, но нужен инструмент
   установки `blackberry-deploy` и доступ к IP работающего симулятора.
4. После установки проверить список чатов, переход в переписку, кириллицу,
   отправку локального сообщения и возврат. Это текущие функции демо;
   подключения к Telegram пока нет.

Подробности установки Windows-инструментов — в [windows-setup.md](windows-setup.md).
