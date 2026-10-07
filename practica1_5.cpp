// =====================================================================
// Practica 1.5: Busqueda Paralela Distribuida con MPI y OpenMP
// Busqueda secuencial y binaria por bloques (Local multihilo vs Distribuido)
// =====================================================================
#define _CRT_SECURE_NO_WARNINGS
#include <mpi.h>
#include <omp.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cstdlib>
#include <ctime>
#include <climits>
using namespace std;

// ---------- CAMBIAR AQUI LOS DATOS DEL EQUIPO ----------
const string EQUIPO  = "Equipo X";
const string NOMBRES = "Hermosillo Prado Carlos | Montes de Oca del Risco Rafael Alejandro | Ramirez Andrade Uriel Ismael Guadalupe";
// -------------------------------------------------------

int rank_, nprocs;          // rank y numero de procesos MPI
string pc;                  // nombre de la computadora
ofstream archivoLog;              // log local de este nodo
int N = 0;                  // tamano del arreglo
int* arr = nullptr;         // arreglo dinamico (solo en nodo 0)
bool ordenado = false;
bool detalle = true;        // impresiones detalladas (solo arreglos pequenos)
double tiempos[8] = { 0 };  // 0 sortSec,1 sortOMP,2 secLocal,3 secDist,4 binLocal,5 binDist,6 comSec,7 comBin

// Escribe en pantalla y en el log del nodo (seguro entre hilos)
void escribir(const string& s) {
    #pragma omp critical(salida)
    {
        cout << s << endl;
        archivoLog << s << endl;
    }
}

// Prefijo de identificacion: equipo, PC, nodo MPI e hilo OpenMP
string pre(int hilo) {
    return "[Equipo: " + EQUIPO + "] [PC: " + pc + "] [Nodo MPI: " + to_string(rank_) +
           "] [Hilo OpenMP: " + to_string(hilo) + "] ";
}

void mostrarArreglo(const string& titulo) {
    ostringstream s;
    s << pre(0) << titulo << ": ";
    for (int i = 0; i < N; i++) s << arr[i] << " ";
    escribir(s.str());
}

// ======================= ORDENAMIENTO =======================
// Mezcla a[l..m) y a[m..r) usando tmp
void mezclar(int* a, int* tmp, int l, int m, int r) {
    int i = l, j = m, k = l;
    while (i < m && j < r) tmp[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
    while (i < m) tmp[k++] = a[i++];
    while (j < r) tmp[k++] = a[j++];
    for (k = l; k < r; k++) a[k] = tmp[k];
}

// Merge Sort secuencial sobre a[l..r)
void mergeSort(int* a, int* tmp, int l, int r) {
    if (r - l < 2) return;
    int m = (l + r) / 2;
    mergeSort(a, tmp, l, m);
    mergeSort(a, tmp, m, r);
    mezclar(a, tmp, l, m, r);
}

// Merge Sort paralelo: cada hilo ordena un bloque y luego se mezclan por pares
void ordenarParalelo(int* a, int n) {
    int* tmp = new int[n];
    int h = omp_get_max_threads();
    int* lim = new int[h + 1];
    for (int i = 0; i <= h; i++) lim[i] = (int)((long long)n * i / h);

    #pragma omp parallel for
    for (int i = 0; i < h; i++) {
        mergeSort(a, tmp, lim[i], lim[i + 1]);
        if (detalle)
            escribir(pre(omp_get_thread_num()) + "[Ordenamiento OpenMP] [Bloque: " +
                     to_string(lim[i]) + " - " + to_string(lim[i + 1] - 1) + "] ordenado");
    }
    for (int paso = 1; paso < h; paso *= 2) {
        #pragma omp parallel for
        for (int i = 0; i < h - paso; i += 2 * paso) {
            int fin = (i + 2 * paso < h) ? i + 2 * paso : h;
            mezclar(a, tmp, lim[i], lim[i + paso], lim[fin]);
        }
    }
    delete[] tmp;
    delete[] lim;
}

// ======================= BUSQUEDA POR BLOQUE =======================
// Busca t en a[0..n) (bloque que inicia en 'offset' del arreglo global).
// El bloque se divide entre los hilos OpenMP. Regresa el menor indice global o INT_MAX.
int buscarBloque(int* a, int n, int offset, int t, bool binaria, const string& esquema) {
    string alg = binaria ? "Busqueda Binaria" : "Busqueda Secuencial";
    int mejor = INT_MAX;

    // Binaria: primero se evalua si el target cae en el rango del bloque
    if (binaria && (n == 0 || t < a[0] || t > a[n - 1])) {
        if (detalle && n > 0)
            escribir(pre(0) + "[Esquema: " + esquema + "] [Bloque: " + to_string(offset) + " - " +
                     to_string(offset + n - 1) + "] [Algoritmo: " + alg +
                     "] [Estado: Target fuera del rango del bloque (" + to_string(a[0]) + " .. " +
                     to_string(a[n - 1]) + ")]");
        return mejor;
    }

    #pragma omp parallel
    {
        int h = omp_get_thread_num(), nh = omp_get_num_threads();
        int ini = (int)((long long)n * h / nh);         // sub-bloque del hilo [ini, fin)
        int fin = (int)((long long)n * (h + 1) / nh);
        int pos = -1;

        if (!binaria) {                                  // secuencial
            for (int i = ini; i < fin; i++)
                if (a[i] == t) { pos = i; break; }
        }
        else if (ini < fin && t >= a[ini] && t <= a[fin - 1]) {   // binaria en su sub-intervalo
            int lo = ini, hi = fin - 1;
            while (lo < hi) {
                int m = (lo + hi) / 2;
                if (a[m] < t) lo = m + 1; else hi = m;
            }
            if (a[lo] == t) pos = lo;
        }

        if (detalle && ini < fin) {
            ostringstream s;
            s << pre(h) << "[Esquema: " << esquema << "] [Bloque: " << offset + ini << " - "
              << offset + fin - 1 << "] [Algoritmo: " << alg << "] [Estado: "
              << (pos >= 0 ? "Elemento Encontrado en Indice Global: " + to_string(offset + pos)
                           : string("No encontrado")) << "]";
            escribir(s.str());
        }
        #pragma omp critical(minimo)
        if (pos >= 0 && offset + pos < mejor) mejor = offset + pos;
    }
    return mejor;
}

string textoResultado(int r) {
    return r == INT_MAX ? "NO ENCONTRADO" : "Encontrado en indice global " + to_string(r);
}

// Ejecuta una busqueda en ambos esquemas: Local multihilo y Distribuido multinivel
void realizarBusqueda(bool bin, int t) {
    string alg = bin ? "Busqueda Binaria" : "Busqueda Secuencial";

    // ---------- Esquema Local Multihilo (solo nodo 0, arreglo completo) ----------
    double tLocal = 0;
    int resLocal = INT_MAX;
    if (rank_ == 0) {
        escribir(pre(0) + "===== ESQUEMA LOCAL MULTIHILO: " + alg + " (target " + to_string(t) + ") =====");
        double t0 = MPI_Wtime();
        resLocal = buscarBloque(arr, N, 0, t, bin, "Local");
        tLocal = MPI_Wtime() - t0;
    }

    // ---------- Esquema Distribuido Multinivel (MPI + OpenMP) ----------
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank_ == 0) escribir(pre(0) + "===== ESQUEMA DISTRIBUIDO MULTINIVEL: " + alg + " =====");
    double t0 = MPI_Wtime();
    MPI_Bcast(&t, 1, MPI_INT, 0, MPI_COMM_WORLD);       // target a todos los nodos

    int* cuentas = new int[nprocs];
    int* desp = new int[nprocs];
    for (int i = 0; i < nprocs; i++) {
        cuentas[i] = N / nprocs + (i < N % nprocs ? 1 : 0);
        desp[i] = (i == 0) ? 0 : desp[i - 1] + cuentas[i - 1];
    }
    int n = cuentas[rank_], offset = desp[rank_];
    int* bloque = new int[n > 0 ? n : 1];
    MPI_Scatterv(arr, cuentas, desp, MPI_INT, bloque, n, MPI_INT, 0, MPI_COMM_WORLD);
    double tCom = MPI_Wtime() - t0;

    if (detalle && n > 0)
        escribir(pre(0) + "[Bloque recibido: " + to_string(offset) + " - " + to_string(offset + n - 1) +
                 "] [Valores: " + to_string(bloque[0]) + " .. " + to_string(bloque[n - 1]) + "]");

    int resNodo = buscarBloque(bloque, n, offset, t, bin, "Distribuido");

    int resGlobal = INT_MAX;
    double t1 = MPI_Wtime();
    MPI_Reduce(&resNodo, &resGlobal, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);
    tCom += MPI_Wtime() - t1;
    double tDist = MPI_Wtime() - t0;

    ostringstream s;
    s << fixed << setprecision(6) << pre(0) << "[" << alg << "] Resultado del nodo: " << textoResultado(resNodo)
      << " | Tiempo comunicacion: " << tCom << " s | Tiempo total respuesta: " << tDist << " s";
    escribir(s.str());

    if (rank_ == 0) {
        ostringstream r;
        r << fixed << setprecision(6)
          << "\n>>> " << alg << " del valor " << t << "\n"
          << ">>> Local multihilo : " << textoResultado(resLocal) << " | Tiempo: " << tLocal << " s\n"
          << ">>> Distribuido     : " << textoResultado(resGlobal) << " | Tiempo: " << tDist << " s"
          << " (comunicacion: " << tCom << " s)";
        escribir(r.str());
        int k = bin ? 4 : 2;
        tiempos[k] = tLocal;
        tiempos[k + 1] = tDist;
        tiempos[bin ? 7 : 6] = tCom;
    }
    delete[] cuentas;
    delete[] desp;
    delete[] bloque;
}

// Speedup = tA / tB ; Eficiencia = Speedup / unidades (hilos o procesos)
void fila(ostringstream& s, const string& nombre, double tA, double tB, int unidades) {
    double sp = (tB > 0) ? tA / tB : 0;
    s << nombre << ": " << tA << " s vs " << tB << " s | Speedup: " << sp
      << " | Eficiencia: " << sp / unidades << "\n";
}

void comparativa() {
    int h = omp_get_max_threads();
    ostringstream s;
    s << fixed << setprecision(6)
      << "\n===== COMPARATIVA DE RENDIMIENTO (N=" << N << ", Procesos MPI=" << nprocs
      << ", Hilos OpenMP=" << h << ") =====\n";
    fila(s, "Ordenamiento        (Secuencial vs OpenMP)  ", tiempos[0], tiempos[1], h);
    fila(s, "Busqueda Secuencial (Local vs Distribuida)  ", tiempos[2], tiempos[3], nprocs);
    fila(s, "Busqueda Binaria    (Local vs Distribuida)  ", tiempos[4], tiempos[5], nprocs);
    s << "Comunicacion inter-nodo -> Secuencial: " << tiempos[6] << " s | Binaria: " << tiempos[7] << " s";
    escribir(s.str());
}

// Lee un entero validando la entrada (evita que el menu se cicle si escriben letras).
// Si la entrada se cierra (Ctrl+C / Ctrl+Z) regresa -1.
int leerEntero(const string& msg, int minimo) {
    int x;
    while (true) {
        cout << msg << flush;
        if (cin >> x && x >= minimo) return x;
        if (cin.eof()) return -1;
        cin.clear();
        cin.ignore(10000, '\n');
        cout << "Entrada no valida, intenta de nuevo.\n";
    }
}

// ============================ MAIN ============================
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    char nombre[MPI_MAX_PROCESSOR_NAME];
    int len;
    MPI_Get_processor_name(nombre, &len);
    pc = nombre;
    archivoLog.open("log_equipo_" + pc + "_nodo_" + to_string(rank_) + ".txt");

    // Primera linea: nombres del equipo
    if (rank_ == 0) escribir(NOMBRES); else archivoLog << NOMBRES << endl;
    MPI_Barrier(MPI_COMM_WORLD);

    // Encabezado de sesion de cada nodo
    const char* cpu = getenv("PROCESSOR_IDENTIFIER");
    escribir(pre(0) + "Inicio de sesion | CPU: " + (cpu ? cpu : "desconocido") +
             " | Nucleos: " + to_string(omp_get_num_procs()) +
             " | Hilos OpenMP: " + to_string(omp_get_max_threads()) +
             " | Procesos MPI: " + to_string(nprocs));
    MPI_Barrier(MPI_COMM_WORLD);

    int op = 0;
    while (true) {
        if (rank_ == 0) {
            cout << "\n========== MENU (" << EQUIPO << ") ==========\n"
                 << "1. Generar arreglo dinamico\n"
                 << "2. Ordenar arreglo con OpenMP\n"
                 << "3. Ejecutar busqueda secuencial (Local vs Distribuida)\n"
                 << "4. Ejecutar busqueda binaria (Local vs Distribuida)\n"
                 << "5. Mostrar tiempos y comparativa de rendimiento\n"
                 << "6. Salir\n";
            op = leerEntero("Opcion: ", 1);
            if (op == -1 || op > 6) op = (op == -1) ? 6 : 0;   // entrada cerrada -> salir
        }
        MPI_Bcast(&op, 1, MPI_INT, 0, MPI_COMM_WORLD);   // menu coordinado por MPI
        if (op == 6) break;

        if (op == 1) {
            if (rank_ == 0) {
                N = leerEntero("Tamano del arreglo: ", 1);
                int maxVal = leerEntero("Valor maximo aleatorio: ", 0);
                if (N < 1) N = 100;
                if (maxVal < 0) maxVal = 1000;
                delete[] arr;
                arr = new int[N];
                srand((unsigned)time(0));
                for (int i = 0; i < N; i++) arr[i] = (int)(((long long)rand() * 32768 + rand()) % (maxVal + 1));
                ordenado = false;
            }
            MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
            detalle = (N <= 1000);   // prueba de control: detallada; prueba masiva: solo tiempos
            escribir(pre(0) + "Arreglo de " + to_string(N) + " elementos | Impresiones detalladas: " +
                     (detalle ? "SI" : "NO"));
            if (rank_ == 0 && detalle) mostrarArreglo("Arreglo generado");
        }
        else if (op == 2 && rank_ == 0) {
            if (!arr) cout << "Primero genere el arreglo (opcion 1)\n";
            else {
                int* copia = new int[N];
                int* tmp = new int[N];
                for (int i = 0; i < N; i++) copia[i] = arr[i];

                double t0 = MPI_Wtime();
                mergeSort(copia, tmp, 0, N);              // secuencial (referencia)
                tiempos[0] = MPI_Wtime() - t0;

                t0 = MPI_Wtime();
                ordenarParalelo(arr, N);                  // paralelo OpenMP
                tiempos[1] = MPI_Wtime() - t0;

                delete[] copia;
                delete[] tmp;
                ordenado = true;
                ostringstream s;
                s << fixed << setprecision(6) << pre(0) << "Ordenamiento secuencial: " << tiempos[0]
                  << " s | Ordenamiento OpenMP (" << omp_get_max_threads() << " hilos): " << tiempos[1] << " s";
                escribir(s.str());
                if (detalle) mostrarArreglo("Arreglo ordenado");
            }
        }
        else if (op == 3 || op == 4) {
            bool bin = (op == 4);
            int ok = 1, t = 0;
            if (rank_ == 0) {
                if (!arr) { cout << "Primero genere el arreglo (opcion 1)\n"; ok = 0; }
                else if (bin && !ordenado) { cout << "Primero ordene el arreglo (opcion 2)\n"; ok = 0; }
                else t = leerEntero("Valor a buscar: ", 0);
            }
            MPI_Bcast(&ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (ok) realizarBusqueda(bin, t);
        }
        else if (op == 5 && rank_ == 0) {
            comparativa();
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    delete[] arr;
    MPI_Barrier(MPI_COMM_WORLD);
    // Ultima linea: nombres del equipo
    if (rank_ == 0) escribir(NOMBRES); else archivoLog << NOMBRES << endl;
    archivoLog.close();
    MPI_Finalize();
    return 0;
}
