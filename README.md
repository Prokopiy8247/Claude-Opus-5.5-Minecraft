# Claude Opus 5.5 — Minecraft в игровых движках

Публичный репозиторий проектов, созданных Claude Opus 5.5 для сравнительного YouTube-видео. Каждый движок находится в отдельной папке и имеет собственный гайд по запуску.

## Проекты

| Папка | Движок | Статус |
|---|---|---|
| [`Unity Opus 5.5 Minecraft`](./Unity%20Opus%205.5%20Minecraft/) | Unity 6000.6.0f1 | Готово |
| [`Godot Opus 5.5 Minecraft`](./Godot%20Opus%205.5%20Minecraft/) | Godot 4.7.2 | Готово |
| [`Unreal Opus 5.5 Minecraft`](./Unreal%20Opus%205.5%20Minecraft/) | Unreal Engine 5.8 | Готово |

## Быстрый старт Unity-версии

Разработка не требуется — скачайте готовую сборку:

- [Windows 10/11, x64](https://github.com/Prokopiy8247/Claude-Opus-5.5-Minecraft/releases/download/unity-v1.0.0/Minecraft-Recreation-Windows-x64.zip)
- [macOS 12+, Intel и Apple Silicon](https://github.com/Prokopiy8247/Claude-Opus-5.5-Minecraft/releases/download/unity-v1.0.0/Minecraft-Recreation-macOS-Universal.zip)

Пошаговый запуск, управление и инструкции для разработчиков находятся в [README Unity-проекта](./Unity%20Opus%205.5%20Minecraft/README.md). Исходный промпт также сохранён в папке проекта.

## Другие версии

- Для Godot скачайте сборку из [релиза Godot v1.0.0](https://github.com/Prokopiy8247/Claude-Opus-5.5-Minecraft/releases/tag/godot-v1.0.0) и следуйте [инструкции](./Godot%20Opus%205.5%20Minecraft/README.md).
- Для Unreal Engine откройте [папку проекта](./Unreal%20Opus%205.5%20Minecraft/) и следуйте её инструкции. Для первого запуска потребуется Unreal Engine 5.8.x и системный C++ toolchain.

## Безопасность и правовой статус

Перед публикацией из Unity-проекта удалены локальные пути пользователя, Unity Cloud Project ID и Organization ID. Автоматическая проверка блокирует распространённые ключи, токены, приватные ключи и локальные пользовательские пути. Правила сообщения об уязвимостях: [SECURITY.md](./SECURITY.md).

Это неофициальный учебно-исследовательский проект. Он не связан с Mojang Studios или Microsoft. Minecraft является товарным знаком Microsoft. Код, процедурная графика, модели и звуки проекта созданы отдельно; оригинальные файлы Minecraft не распространяются.
