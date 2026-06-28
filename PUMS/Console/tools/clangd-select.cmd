@echo off

setlocal EnableDelayedExpansion

rem Cursor/VS Code: значение "clangd.path". Порядок: portable → LLVM в Program Files → VS → PATH.

rem Portable: скачайте clangd-windows-*.zip с https://github.com/clangd/clangd/releases

rem            и распакуйте в tools\clangd-portable\clangd_VERSION\



rem Скрипт лежит в tools\; portable — tools\clangd-portable\

set "PORT=%~dp0clangd-portable"



if exist "%PORT%" (

  for /f "delims=" %%D in ('dir /b /ad "%PORT%\clangd_*" 2^>nul') do (

    set "CAND=!PORT!\%%D\bin\clangd.exe"

    if exist "!CAND!" (

      "!CAND!" --version >nul 2>&1

      if not errorlevel 1 (

        "!CAND!" %*

        exit /b !errorlevel!

      )

    )

  )

)



if exist "%ProgramFiles%\LLVM\bin\clangd.exe" (

  "%ProgramFiles%\LLVM\bin\clangd.exe" %*

  exit /b %errorlevel%

)



for %%V in (Community Professional Enterprise Preview BuildTools) do (

  set "VSL=%ProgramFiles%\Microsoft Visual Studio\2022\%%V\VC\Tools\Llvm\x64\bin\clangd.exe"

  if exist "!VSL!" (

    "!VSL!" %*

    exit /b !errorlevel!

  )

)



for /f "delims=" %%i in ('where clangd 2^>nul') do (

  "%%i" --version >nul 2>&1

  if not errorlevel 1 (

    "%%i" %*

    exit /b !errorlevel!

  )

)



echo clangd-select: не найден рабочий clangd.exe. >&2

echo   Установите LLVM ^(clangd^), добавьте в PATH, или распакуйте portable в tools\clangd-portable\ >&2

echo   Релизы: https://github.com/clangd/clangd/releases >&2

exit /b 127

