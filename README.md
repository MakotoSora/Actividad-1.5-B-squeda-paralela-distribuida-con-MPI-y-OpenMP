# Práctica 1.5: Búsqueda Paralela Distribuida con MPI y OpenMP

**Materia:** Programación Paralela (2026B_IH067_PP_V) — Unidad 1 MPI

**Equipo (por primer apellido, orden alfabético):**
- Hermosillo Prado Carlos
- Montes de Oca del Risco Rafael Alejandro
- Ramírez Andrade Uriel Ismael Guadalupe

---

## Descripción

Sistema de búsqueda paralela híbrido (**MPI + OpenMP**) sobre un arreglo dinámico de gran tamaño. Compara dos algoritmos:

- **Búsqueda secuencial distribuida por bloques**: cada hilo revisa elemento por elemento su parte del bloque.
- **Búsqueda binaria distribuida por bloques**: requiere el arreglo ordenado; cada nodo revisa si el valor cae en el rango de su bloque y, si es así, cada hilo hace búsqueda binaria en su sub-intervalo.

Cada algoritmo se ejecuta en dos esquemas:

| Esquema | Descripción |
|---|---|
| **Local multihilo** | Solo el nodo 0 busca en el arreglo completo usando hilos OpenMP. |
| **Distribuido multinivel** | MPI reparte el arreglo entre los procesos de la red y OpenMP divide cada bloque entre los hilos locales. |

## Arquitectura

```
                 Nodo 0 (maestro)
   genera arreglo -> ordena con OpenMP (Merge Sort paralelo)
                        |
        MPI_Bcast (valor a buscar) + MPI_Scatterv (bloques)
      ┌──────────┬──────────┼──────────┬──────────┐
   Nodo 0     Nodo 1     Nodo 2     Nodo 3     Nodo 4
  [0..N/5)  [N/5..2N/5)   ...        ...       [4N/5..N)
  hilos 0-7  hilos 0-7  hilos 0-7  hilos 0-7  hilos 0-7
      └──────────┴──────────┼──────────┴──────────┘
          MPI_Reduce (MPI_MIN) -> índice global más bajo
```

- **Índice global** = índice local + offset del bloque del nodo.
- Los nodos que no encuentran el valor envían `INT_MAX`, que nunca gana en el `MPI_MIN`.

## Funciones principales (`practica1_5.cpp`)

| Función | Qué hace |
|---|---|
| `escribir()` | Imprime en pantalla y en el log del nodo (protegido con `omp critical`). |
| `pre()` | Arma el prefijo `[Equipo] [PC] [Nodo MPI] [Hilo OpenMP]`. |
| `mergeSort()` / `mezclar()` | Merge Sort secuencial (referencia de tiempo). |
| `ordenarParalelo()` | Merge Sort paralelo: cada hilo ordena un bloque y luego se mezclan por pares. |
| `buscarBloque()` | Búsqueda secuencial o binaria en un bloque, dividida entre hilos OpenMP. |
| `realizarBusqueda()` | Ejecuta el esquema local y el distribuido (`Bcast`, `Scatterv`, `Reduce`) y mide tiempos. |
| `comparativa()` | Muestra tiempos, speedup y eficiencia. |
| `leerEntero()` | Valida la entrada del teclado para que el menú no se cicle. |

## Complejidad

| Algoritmo | Secuencial | Paralelo (p procesos, h hilos) |
|---|---|---|
| Merge Sort | O(n log n) | ≈ O((n/h) log(n/h)) + mezclas |
| Búsqueda secuencial | O(n) | O(n / (p·h)) + comunicación O(n) |
| Búsqueda binaria | O(log n) | O(log(n / (p·h))) + comunicación O(n) |

## Requisitos

- Windows 10/11 con **MS-MPI** (runtime y SDK) instalado en las 3 PCs (misma versión).
- **Visual Studio Community** (compilador MSVC), plataforma **x64**.
- Las 3 PCs en la misma red (`192.168.137.x`).

## Compilación (MSVC x64 + MS-MPI + OpenMP)

1. Abrir **Developer Command Prompt for VS** (x64).
2. Ejecutar `compilar.bat` desde `C:\MPI\Proyecto_MPI`, que corre:

```
cl /EHsc /O2 /openmp /I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include" practica1_5.cpp /link /LIBPATH:"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64" msmpi.lib /out:practica1_5.exe
```

3. Copiar `practica1_5.exe` a `C:\MPI\Proyecto_MPI\` en las 3 PCs.

## Ejecución

### Local (5 procesos en una PC)

```
cd /d C:\MPI\Proyecto_MPI
mpiexec -n 5 practica1_5.exe
```
o ejecutar `ejecutar_local.bat`.

### Distribuida (3 PCs)

| PC | IP | Procesos |
|---|---|---|
| Maestro (nodo 0) | 192.168.137.1 | 1 |
| Trabajador 1 | 192.168.137.165 | 2 |
| Trabajador 2 | 192.168.137.153 | 2 |

1. En **las 3 PCs**: ejecutar `iniciar_smpd.bat` (o `smpd -d`) y dejar la ventana abierta.
2. En el maestro: ejecutar `probar_conexion.bat`. Debe mostrar los 3 nombres de PC.
3. En el maestro: ejecutar `ejecutar_red.bat`, o:

```
mpiexec -hosts 3 192.168.137.1 1 192.168.137.165 2 192.168.137.153 2 C:\MPI\Proyecto_MPI\practica1_5.exe
```

## Menú

| Opción | Acción |
|---|---|
| 1 | Generar arreglo dinámico (tamaño y valor máximo) |
| 2 | Ordenar arreglo con OpenMP (secuencial vs paralelo) |
| 3 | Búsqueda secuencial (local vs distribuida) |
| 4 | Búsqueda binaria (local vs distribuida) |
| 5 | Tiempos, speedup y eficiencia |
| 6 | Salir |

## Pruebas

**Prueba 1 — Arreglo de control (100 elementos):** impresiones detalladas activadas (N ≤ 1000).
```
1 -> 100 -> 50
2
3 -> (valor que aparezca en el arreglo)
4 -> (mismo valor)
3 -> 99        (caso no encontrado)
5
6
```

**Prueba 2 — Arreglo masivo (10,000,000 elementos):** solo tiempos.
```
1 -> 10000000 -> 1000000
2
3 -> 500000
4 -> 500000
5
6
```

## Logs

Cada proceso genera su archivo en la carpeta de ejecución de su PC:
```
log_equipo_[NOMBRE_PC]_nodo_[RANK].txt
```
Contiene: nombres del equipo (inicio y fin), especificaciones del procesador, tiempos de ordenamiento, trazabilidad de búsqueda por hilo y bloque, y tiempos de comunicación y de respuesta (`MPI_Wtime()`).

Ejemplo:
```
[Equipo: X] [PC: CarlosHP] [Nodo MPI: 0] [Hilo OpenMP: 7] [Esquema: Distribuido] [Bloque: 17 - 19] [Algoritmo: Busqueda Binaria] [Estado: Elemento Encontrado en Indice Global: 18]
```

## Estructura del repositorio

```
practica1_5.cpp       Código fuente
compilar.bat          Compilación con MSVC x64 + MS-MPI + OpenMP
Makefile              Compilación con mpicxx (Linux/MinGW)
hosts.txt             Hosts para ejecución en red con el Makefile
ejecutar_local.bat    Ejecución local (5 procesos)
iniciar_smpd.bat      Levanta smpd en cada PC
probar_conexion.bat   Verifica conexión entre las 3 PCs
ejecutar_red.bat      Ejecución distribuida (3 PCs)
README.md             Este archivo
```

## Problemas comunes

| Problema | Solución |
|---|---|
| No conecta con otra PC | Revisar que `smpd -d` esté abierto y permitir `smpd.exe`, `mpiexec.exe` y `practica1_5.exe` en el Firewall. |
| Falta alguna DLL al ejecutar en otra PC | Compilar con `compilar.bat` (no con la configuración Debug de Visual Studio). |
| `cl` no se reconoce | Abrir **Developer Command Prompt for VS** (x64), no el CMD normal. |
| "file not found" | El `.exe` no está en `C:\MPI\Proyecto_MPI` en alguna PC. |
| El menú se queda trabado | No presionar Ctrl+C en la consola; para copiar, seleccionar con el mouse. |
