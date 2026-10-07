@echo off
REM ============================================================
REM  Practica 1.5 - Compilacion con MSVC + MS-MPI + OpenMP
REM  Ejecutar desde "Developer Command Prompt for VS" (x64)
REM  en la carpeta C:\MPI\Proyecto_MPI
REM ============================================================
cd /d C:\MPI\Proyecto_MPI

cl /EHsc /O2 /openmp /I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include" practica1_5.cpp /link /LIBPATH:"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64" msmpi.lib /out:practica1_5.exe

if errorlevel 1 (
    echo [ERROR] La compilacion fallo.
    exit /b 1
)

echo [OK] practica1_5.exe generado en C:\MPI\Proyecto_MPI
