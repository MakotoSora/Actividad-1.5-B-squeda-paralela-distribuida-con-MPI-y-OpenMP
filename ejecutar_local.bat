@echo off
REM ============================================================
REM  Practica 1.5 - Ejecucion LOCAL (60%)
REM  5 procesos MPI en esta PC: nodo 0 (maestro) + nodos 1-4
REM  Cada proceso usa los hilos OpenMP disponibles
REM ============================================================
cd /d C:\MPI\Proyecto_MPI
mpiexec -n 5 C:\MPI\Proyecto_MPI\practica1_5.exe
