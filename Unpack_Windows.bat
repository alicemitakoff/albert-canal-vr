@echo off
rem Unpacks the Albert Canal project into a folder called AlbertCanal next to this file.
cd /d "%~dp0"
echo Unpacking the Albert Canal project. This takes a few minutes...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
 "$ErrorActionPreference = 'Stop'; $here = (Get-Location).Path; $dest = Join-Path $here 'AlbertCanal';" ^
 "Get-ChildItem (Join-Path $here 'parts\*.zip') | Sort-Object Name | ForEach-Object { Write-Host $_.Name; Expand-Archive -LiteralPath $_.FullName -DestinationPath $dest -Force };" ^
 "foreach ($f in 'AlbertCanalSwap-arm64.apk', 'main.1.com.aimovation.albertcanalswap.obb') { Write-Host $f; $out = [IO.File]::Create((Join-Path $dest ('AlbertCanal_Quest3_app\' + $f))); Get-ChildItem (Join-Path $here ('parts\' + $f + '.*')) | Sort-Object Name | ForEach-Object { $b = [IO.File]::ReadAllBytes($_.FullName); $out.Write($b, 0, $b.Length) }; $out.Close() }"
if errorlevel 1 (
  echo.
  echo Something went wrong while unpacking. Check there is at least 3 GB free and run this again.
  pause
  exit /b 1
)
echo.
echo Done. Everything is in the AlbertCanal folder next to this file.
pause
