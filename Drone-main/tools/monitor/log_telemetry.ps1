<#
  log_telemetry.ps1
  Coletor de telemetria do drone (ESP32) + monitor de cliques do mouse.

  Roda OFFLINE: so faz HTTP local para o ESP (AP EQUIPE4-AP, 192.168.4.1).
  - Le /flightStatus + /sensors a cada -IntervalMs e grava em telemetry_*.csv
  - Captura TODO clique do mouse (esq/dir) em qualquer lugar da tela e grava
    em telemetry_*_clicks.csv (timestamp, botao, x, y, janela ativa).
  Os dois arquivos tem timestamp na mesma base de tempo: cruze por 'ts'.

  Uso:
    .\log_telemetry.ps1
    .\log_telemetry.ps1 -IntervalMs 100

  Parar: Ctrl+C.
#>
param(
    [string]$Ip = "192.168.4.1",
    [int]$IntervalMs = 200,
    [string]$OutDir = "$PSScriptRoot\logs"
)

# Estamos no AP do ESP, sem internet: desliga proxy do sistema para nao atrasar.
[System.Net.WebRequest]::DefaultWebProxy = $null

# API do Windows para ler mouse global e janela em foco.
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Win32 {
  [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int vKey);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X; public int Y; }
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);
}
"@

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

$stamp      = Get-Date -Format "yyyyMMdd_HHmmss"
$csv        = Join-Path $OutDir "telemetry_$stamp.csv"
$clicksCsv  = Join-Path $OutDir "telemetry_${stamp}_clicks.csv"
$base       = "http://$Ip"
$flightUrl  = "$base/flightStatus"
$sensorsUrl = "$base/sensors"

Write-Host "=== Monitor de telemetria + cliques ===" -ForegroundColor Cyan
Write-Host "ESP:       $base"
Write-Host "Intervalo: $IntervalMs ms"
Write-Host "Telemetria:$csv"
Write-Host "Cliques:   $clicksCsv"
Write-Host "Pressione Ctrl+C para parar."
Write-Host ""

$start = Get-Date
$count = 0
$script:clicksSince = 0

function Get-MousePos {
    $p = New-Object Win32+POINT
    [void][Win32]::GetCursorPos([ref]$p)
    return $p
}
function Get-ActiveWindowTitle {
    $h  = [Win32]::GetForegroundWindow()
    $sb = New-Object System.Text.StringBuilder 256
    [void][Win32]::GetWindowText($h, $sb, 256)
    return $sb.ToString()
}
function Record-Click([string]$btn) {
    $p   = Get-MousePos
    $win = Get-ActiveWindowTitle
    $ts  = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss.fff")
    [pscustomobject]@{ ts = $ts; button = $btn; x = $p.X; y = $p.Y; window = $win } |
        Export-Csv -Path $clicksCsv -NoTypeInformation -Append -Encoding UTF8
    Write-Host ("  >> CLICK {0} @ ({1},{2})  [{3}]" -f $btn, $p.X, $p.Y, $win) -ForegroundColor Green
    $script:clicksSince++
}

# Esquema fixo de colunas (mantem o CSV consistente mesmo sem resposta do ESP).
function New-Row {
    return [ordered]@{
        ts = $null; elapsed_s = $null; ok = $null; latency_ms = $null
        mouse_x = $null; mouse_y = $null; window = $null; clicks_since = $null
        controlEnabled = $null; failsafe = $null; failsafeLatched = $null
        commandAgeMs = $null; arming = $null
        sp_throttle = $null; sp_roll = $null; sp_pitch = $null; sp_yaw = $null
        st_roll = $null; st_pitch = $null; st_yaw = $null
        rate_roll = $null; rate_pitch = $null; rate_yaw = $null
        corr_roll = $null; corr_pitch = $null; corr_yaw = $null
        m1 = $null; m2 = $null; m3 = $null; m4 = $null
        mpu_online = $null; mpu_valid = $null; mpu_calib = $null; mpu_calibPct = $null
        ax = $null; ay = $null; az = $null
        gx = $null; gy = $null; gz = $null
        att_roll = $null; att_pitch = $null; att_yaw = $null
        accelNormG = $null; mpu_temp = $null; mpu_ageMs = $null
        bmp_online = $null; bmp_altM = $null; bmp_refReady = $null
        gps_valid = $null; gps_sats = $null
    }
}

function Write-Telemetry {
    $t0 = Get-Date
    $ok = $true; $fs = $null; $sn = $null
    try {
        $fs = Invoke-RestMethod -Uri $flightUrl  -TimeoutSec 2
        $sn = Invoke-RestMethod -Uri $sensorsUrl -TimeoutSec 2
    } catch { $ok = $false }
    $t1      = Get-Date
    $latency = [math]::Round(($t1 - $t0).TotalMilliseconds, 1)
    $elapsed = [math]::Round(($t1 - $start).TotalSeconds, 2)
    $ts      = $t0.ToString("yyyy-MM-dd HH:mm:ss.fff")
    $mp      = Get-MousePos

    $row = New-Row
    $row.ts = $ts; $row.elapsed_s = $elapsed; $row.ok = $ok; $row.latency_ms = $latency
    $row.mouse_x = $mp.X; $row.mouse_y = $mp.Y
    $row.window = Get-ActiveWindowTitle
    $row.clicks_since = $script:clicksSince
    $script:clicksSince = 0

    if ($ok) {
        $row.controlEnabled  = $fs.controlEnabled
        $row.failsafe        = $fs.failsafe
        $row.failsafeLatched = $fs.failsafeLatched
        $row.commandAgeMs    = $fs.commandAgeMs
        $row.arming          = $fs.arming
        $row.sp_throttle = $fs.setpoint.throttle; $row.sp_roll = $fs.setpoint.roll
        $row.sp_pitch    = $fs.setpoint.pitch;    $row.sp_yaw  = $fs.setpoint.yaw
        $row.st_roll  = $fs.state.roll;  $row.st_pitch  = $fs.state.pitch;  $row.st_yaw  = $fs.state.yaw
        $row.rate_roll = $fs.rate.roll;  $row.rate_pitch = $fs.rate.pitch;  $row.rate_yaw = $fs.rate.yaw
        $row.corr_roll = $fs.correction.roll; $row.corr_pitch = $fs.correction.pitch; $row.corr_yaw = $fs.correction.yaw
        $row.m1 = $fs.motors[0]; $row.m2 = $fs.motors[1]; $row.m3 = $fs.motors[2]; $row.m4 = $fs.motors[3]

        $row.mpu_online   = $sn.mpu9259.online
        $row.mpu_valid    = $sn.mpu9259.valid
        $row.mpu_calib    = $sn.mpu9259.calibrationComplete
        $row.mpu_calibPct = $sn.mpu9259.calibrationProgress
        $row.ax = $sn.mpu9259.accelG[0]; $row.ay = $sn.mpu9259.accelG[1]; $row.az = $sn.mpu9259.accelG[2]
        $row.gx = $sn.mpu9259.gyroDps[0]; $row.gy = $sn.mpu9259.gyroDps[1]; $row.gz = $sn.mpu9259.gyroDps[2]
        $row.att_roll  = $sn.mpu9259.realAttitude.roll
        $row.att_pitch = $sn.mpu9259.realAttitude.pitch
        $row.att_yaw   = $sn.mpu9259.realAttitude.yaw
        $row.accelNormG = $sn.mpu9259.accelNormG
        $row.mpu_temp   = $sn.mpu9259.temperatureC
        $row.mpu_ageMs  = $sn.mpu9259.ageMs
        $row.bmp_online   = $sn.bmp280.online
        $row.bmp_altM     = $sn.bmp280.altitudeM
        $row.bmp_refReady = $sn.bmp280.referenceReady
        $row.gps_valid = $sn.gps.valid
        $row.gps_sats  = $sn.gps.satellites
    }

    (New-Object PSObject -Property $row) |
        Export-Csv -Path $csv -NoTypeInformation -Append -Encoding UTF8

    $script:count++
    if ($ok) {
        Write-Host ("[{0}] #{1} ctrl={2} fs={3} thr={4} att R/P/Y={5}/{6}/{7} M=[{8},{9},{10},{11}] {12}ms" -f `
            $ts, $script:count, $fs.controlEnabled, $fs.failsafe, $fs.setpoint.throttle, `
            $fs.state.roll, $fs.state.pitch, $fs.state.yaw, `
            $fs.motors[0], $fs.motors[1], $fs.motors[2], $fs.motors[3], $latency)
    } else {
        Write-Host ("[{0}] #{1} SEM RESPOSTA do ESP ({2}) - confira o Wi-Fi EQUIPE4-AP" -f $ts, $script:count, $base) -ForegroundColor Yellow
    }
}

# Laco rapido (15 ms): pega cliques na hora; telemetria sai a cada -IntervalMs.
$lastTelemetry = (Get-Date).AddMilliseconds(-$IntervalMs)  # forca 1a coleta imediata
$prevLeft  = $false
$prevRight = $false

while ($true) {
    $left  = ([Win32]::GetAsyncKeyState(0x01) -band 0x8000) -ne 0  # botao esquerdo
    $right = ([Win32]::GetAsyncKeyState(0x02) -band 0x8000) -ne 0  # botao direito
    if ($left  -and -not $prevLeft)  { Record-Click "ESQ" }
    if ($right -and -not $prevRight) { Record-Click "DIR" }
    $prevLeft  = $left
    $prevRight = $right

    if (((Get-Date) - $lastTelemetry).TotalMilliseconds -ge $IntervalMs) {
        Write-Telemetry
        $lastTelemetry = Get-Date
    }

    Start-Sleep -Milliseconds 15
}
