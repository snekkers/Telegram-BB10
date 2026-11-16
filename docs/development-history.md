# История разработки Telegram для BlackBerry 10

В репозитории сохранена только разработка Telegram (первоначальное название — Berrygram):
нативное приложение Cascades, отдельный движок TDLib, порт QNX ARMv7,
диагностический стенд macOS, ресурсы, проверки и инструкции сборки.
Актуальная версия приложения: **0.16.0.21**.

## Как восстановлена история

На 16 ноября 2026 года исходных Git-коммитов, именованных веток и удалённого
репозитория у проекта не было. История восстановлена из сохранённых деревьев Git
и сверена с относящимися к Telegram чатами проекта. Восстановлены **28 различающихся
снимков Telegram**: от первого нативного каркаса до аватаров групп и обсуждений каналов.

Каждый такой снимок оформлен отдельным последовательным коммитом. Коммиты созданы
при публикации, без подмены дат авторства. В сообщениях указаны исходное дерево и
время записи его объекта. Эти времена отражают сохранение снимка и могут быть
позже самих изменений. Снимки не являются журналом каждого отдельного редактирования;
отсутствующие промежуточные состояния не воссоздавались предположительно.
Таблица ниже показывает время записи объекта в часовом поясе Москвы.

Отдельным заключительным коммитом добавлены эта справка, ссылка из README и правила
исключения локальных зависимостей и других приложений. Вся опубликованная история
отфильтрована по границам Telegram-проекта. Локальные файлы других приложений сохранены.
Сборки, SDK, ключи подписи, базы аккаунтов и сеансы не включены.

## Сохранённые этапы

| № | Время записи снимка, Москва | Коммит | Изменение |
|---|---|---|---|
| 1 | 2026-10-29 17:47:42 | `959c94cb78c1` | Add initial Berrygram Cascades chat prototype and Windows setup |
| 2 | 2026-10-29 18:08:20 | `fff456e06d9a` | Add reproducible BB10 builds on Apple silicon with Docker |
| 3 | 2026-10-29 19:23:42 | `5960dbd777a9` | Document and automate BB10 simulator experiments in UTM |
| 4 | 2026-10-29 23:05:35 | `9c17bac21f0b` | Add isolated TDLib JSON host and integration checks |
| 5 | 2026-10-30 00:15:01 | `3a76affeec3d` | Port TDLib and OpenSSL to QNX ARMv7 with native build tooling |
| 6 | 2026-10-30 00:30:41 | `e8ca58e42246` | Connect Telegram authorization to the native login screen |
| 7 | 2026-10-30 12:55:16 | `fc0b6f3a0d42` | Add Telegram chat history, updates and text delivery |
| 8 | 2026-10-30 15:45:57 | `c98e0171bac8` | Add macOS diagnostic client, drafts, search and cached history |
| 9 | 2026-10-30 19:18:29 | `3b5c39ab3ac2` | Add photo, document and voice attachment workflows |
| 10 | 2026-11-12 21:50:46 | `bbdeeadc42ee` | Persist drafts and attachment recovery across restarts |
| 11 | 2026-11-12 22:46:35 | `fabefe0d0c1b` | Document initial installation and testing on BlackBerry Classic |
| 12 | 2026-11-12 23:30:29 | `384f342d8f17` | Fix ARM JSON handling and verify TDLib on Classic |
| 13 | 2026-11-12 23:32:12 | `ffc8acca074c` | Record successful on-device TDLib startup confirmation |
| 14 | 2026-11-12 23:54:29 | `e7ef78e4f494` | Redesign Berrygram UI and correct outgoing message alignment |
| 15 | 2026-11-13 13:33:08 | `773fce6c8d81` | Refine native chat interface and add media presentation |
| 16 | 2026-11-13 13:55:14 | `159e02b97637` | Add Telegram search and native search screen |
| 17 | 2026-11-13 15:02:28 | `a78327897615` | Add message actions, forwarding and automatic history paging |
| 18 | 2026-11-13 17:01:22 | `597d27072440` | Add contacts, presence and cached Telegram avatars |
| 19 | 2026-11-13 18:45:42 | `cfee98eb9532` | Remove ambiguous previous-send warning |
| 20 | 2026-11-13 19:08:06 | `f14b5a195014` | Render color emoji and preserve compose focus after sending |
| 21 | 2026-11-14 10:36:49 | `da9df0cf563e` | Add notifications, photo viewer, emoji picker and message selection |
| 22 | 2026-11-14 12:18:07 | `79a132648fa0` | Add reading preferences and improve chat and photo performance |
| 23 | 2026-11-14 14:00:43 | `87c463f62689` | Add Telegram reactions, pinned messages and scheduled sending |
| 24 | 2026-11-15 14:50:04 | `083376bcd985` | Update Telegram application icon and record source attribution |
| 25 | 2026-11-15 14:58:16 | `cae3e2cedb47` | Rename application to Telegram and restore existing login automatically |
| 26 | 2026-11-15 15:11:36 | `df44643e75cd` | Bound message processing, caches and history request recovery |
| 27 | 2026-11-15 15:48:59 | `50ae69512673` | Add voice seeking, scheduled message management and diagnostics |
| 28 | 2026-11-16 21:37:58 | `8aec1009945d` | Add group avatars and channel comment threads in Telegram 0.16.0 |

## Версии и воспроизводимость

Метки `telegram/v<versionNumber>.<buildId>` указывают на последний сохранённый
снимок соответствующей версии из `bar-descriptor.xml`. Для версий без отдельного
сохранённого снимка метки не создавались. Исходное название Berrygram сохранено
в старых версиях и путях сборки.

Инструкции сборки и ограничения: [README](../README.md),
[порт QNX](qnx-port.md), [стенд macOS](macos-diagnostic.md),
[проверки на Classic](classic-device.md), [комментарии каналов](community-comments.md).

При подготовке публикации проверен синтаксис 31 Python-файла, 18 shell-скриптов
и 3 JavaScript-файлов; для QML JavaScript учтена директива `.pragma library`.
История проверена на случайное включение секретов и посторонних приложений.
Полная пересборка SDK и проверки приложения с аккаунтом при публикации повторно
не выполнялись; результаты прошлых проверок описаны в тематических документах.
