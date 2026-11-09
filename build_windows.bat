@echo off
REM Нужны: Visual Studio 2022 (рабочая нагрузка "Разработка классических приложений на C++"), CMake и Git.
cmake -B build -G "Visual Studio 17 2022" -A x64 || goto :err
cmake --build build --config Release --target PitCoreTune_VST3 --parallel || goto :err
echo.
echo Готово. Папка "Pit Core Tune.vst3" лежит здесь:
echo   build\PitCoreTune_artefacts\Release\VST3\
echo Скопируйте её в C:\Program Files\Common Files\VST3\
pause
exit /b 0
:err
echo Сборка не удалась. Пришлите последние строки ошибки.
pause
exit /b 1
