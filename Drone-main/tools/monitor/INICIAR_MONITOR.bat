@echo off
REM Duplo-clique para comecar a monitorar o ESP (drone) e gravar CSV.
REM Requer estar conectado no Wi-Fi do ESP (EQUIPE4-AP).
REM Edite -IntervalMs para mudar a taxa de coleta (ms).

cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0log_telemetry.ps1" -IntervalMs 200

echo.
echo Monitor encerrado.
pause
