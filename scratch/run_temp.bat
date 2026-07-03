@echo off
cd build\sandbox_game\Release
if exist game_run.log del game_run.log
start /b game.exe > game_run.log 2>&1
timeout /t 3 /nobreak >nul
taskkill /f /im game.exe
