# Minecraft Recreation — Unity Opus 5.5

Готовая однопользовательская Minecraft-подобная игра и полный Unity-проект. Мир, интерфейс, текстуры и звуки создаются кодом проекта; 3D-модели проходят через включённый Blender-пайплайн.

## Быстрый запуск без Unity

### Windows

1. Скачайте [Minecraft-Recreation-Windows-x64.zip](https://github.com/Prokopiy8247/Claude-Opus-5.5-Minecraft/releases/latest/download/Minecraft-Recreation-Windows-x64.zip).
2. Нажмите на архив правой кнопкой → **Извлечь всё**. Не запускайте EXE прямо из архива.
3. Откройте распакованную папку и запустите `PLAY-WINDOWS.bat` или `MinecraftRecreation.exe`.
4. Если Windows SmartScreen предупредит о неизвестном издателе, убедитесь, что файл скачан из этого репозитория, затем выберите **Подробнее → Выполнить в любом случае**.

Нельзя переносить один EXE отдельно: рядом с ним нужны `MinecraftRecreation_Data`, `UnityPlayer.dll` и остальные файлы из архива.

### macOS

1. Скачайте [Minecraft-Recreation-macOS-Universal.zip](https://github.com/Prokopiy8247/Claude-Opus-5.5-Minecraft/releases/latest/download/Minecraft-Recreation-macOS-Universal.zip).
2. Распакуйте архив и откройте `Minecraft Recreation.app` либо `PLAY ON MAC.command`.
3. Сборка не подписана и не нотарифицирована Apple. При первом запуске нажмите приложение правой кнопкой → **Открыть** → **Открыть**. Если кнопки нет: **Системные настройки → Конфиденциальность и безопасность → Всё равно открыть**.

Сборка Universal содержит Intel x64 и Apple Silicon ARM64. Минимальная версия — macOS 12.

## Первые минуты в игре

На стартовом экране выберите **Singleplayer → Create New World**, укажите имя и при желании seed. По умолчанию доступен творческий режим.

| Действие | Управление |
|---|---|
| Ходьба | `W A S D` |
| Прыжок / полёт в Creative | `Space` / двойное нажатие `Space` |
| Красться / спуститься | `Left Shift` |
| Бег | `Left Ctrl` или двойное `W` |
| Ломать / атаковать | Левая кнопка мыши |
| Ставить / использовать | Правая кнопка мыши |
| Инвентарь | `E` |
| Выбрать слот | `1–9` или колёсико мыши |
| Выбросить предмет | `Q` |
| Поменять руки | `F` |
| Чат / команда | `T` / `/` |
| Пауза | `Esc` |
| Смена камеры | `F5` |
| Полный экран | `F11` |
| Скриншот | `F2` |
| Отладочная информация | `F3` |
| Подсказка сочетаний | `F3+Q` |

Сохранение происходит автоматически и при выходе. Вручную сохранить мир можно через `Ctrl+S`.

Сохранения находятся в пользовательской папке Unity:

- Windows: `%USERPROFILE%\AppData\LocalLow\Opus 5.5\Minecraft Recreation`
- macOS: `~/Library/Application Support/Opus 5.5/Minecraft Recreation`

## Открыть исходники

1. Установите Unity Hub и Unity Editor **6000.6.0f1**.
2. Добавьте модули **Windows Build Support** и **Mac Build Support**.
3. В Unity Hub нажмите **Add project from disk** и выберите эту папку — `Unity Opus 5.5 Minecraft`.
4. Откройте `Assets/_Game/Scenes/Main.unity` и нажмите Play.

Сборки создаются из меню Unity:

- `Tools → Opus 5.5 Minecraft → Build Windows`
- `Tools → Opus 5.5 Minecraft → Build macOS`

Результаты появятся в `Builds/Windows` и `Builds/macOS`. macOS использует включённый профиль `Assets/Settings/Build Profiles/macOS Universal.asset`.

Для пакетной сборки используйте:

```text
<UNITY_EDITOR> -batchmode -quit -projectPath "<PROJECT_PATH>" -executeMethod MCR.EditorTools.BatchTools.BuildWindows -logFile "<PROJECT_PATH>/Tools/_out/build-windows.log"

<UNITY_EDITOR> -batchmode -quit -projectPath "<PROJECT_PATH>" -executeMethod MCR.EditorTools.BatchTools.BuildMacOS -logFile "<PROJECT_PATH>/Tools/_out/build-macos.log"
```

Создать архивы для зрителей:

```bash
python Tools/package_release.py
```

## Документы

- [Исходный промпт](Unity_Minecraft_Prompt.md)
- [Итоговый отчёт](OPUS_5.5_FINAL_REPORT.md)
- [Матрица функций](FEATURE_MATRIX.md)
- [Руководство разработчика](DEVELOPMENT.md)

Проект использует Unity 6000.6.0f1, URP 17.6.0 и Input System 1.20.0. Сборки Windows x64 и macOS Universal созданы в release-конфигурации без development/debug-флагов.
