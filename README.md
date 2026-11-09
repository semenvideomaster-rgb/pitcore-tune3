# Pit Core Tune — VST3 (Windows, FL Studio)

Автотюн + Neve-эквалайзер + хорус + делей + реверб. Эффект (ставится на микрофонный/вокальный трек).

## Сборка без установки чего-либо (GitHub Actions)
1. Создайте пустой репозиторий на github.com, загрузите в него все файлы из этой папки (включая скрытую `.github`).
2. Вкладка **Actions → Build VST3 (Windows) → Run workflow** (или просто после push).
3. Через ~10 минут скачайте артефакт **PitCoreTune-VST3** — внутри папка `Pit Core Tune.vst3`.

## Сборка локально
Нужны Visual Studio 2022 (C++), CMake, Git. Запустите `build_windows.bat`.

## Установка
Скопируйте папку `Pit Core Tune.vst3` в `C:\Program Files\Common Files\VST3\`.
FL Studio: Options → Manage plugins → Find plugins (Verify). Плагин появится в Effects (Fx); вставьте в слот микшера.

## Заметки
- Латентность 20 мс, сообщается хосту (компенсируется автоматически).
- Интерфейс — стандартный JUCE (ползунки), не неоновый.
- Моно-источник: вход суммируется в моно, выход одинаковый на L/R.
