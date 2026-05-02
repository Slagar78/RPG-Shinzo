@echo off
:: Сборка RPG Shinzo в EXE (без консоли)
echo ========================================
echo   BUILD RPG SHINZO
echo ========================================
echo.

:: Задайте имя выходного файла
set EXE_NAME=Shinzo.exe

echo [1/2] Сборка EXE...
ocran game.rb --windows --output %EXE_NAME%

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ОШИБКА] Сборка завершилась с ошибкой!
    pause
    exit /b 1
)

echo.
echo [2/2] Готово! Файл: %EXE_NAME%
echo.
echo ========================================
echo   СБОРКА УСПЕШНО ЗАВЕРШЕНА
echo ========================================
pause