/*
Nombre: Erika
Apellido: Benitez
Fecha: 19/05/2026
Tema: Juego gorilla/*
    TP_GorillaBas_BenitezAyala_ErikaJessenia.cpp

    Version 2D grafica de Gorilla.bas usando solamente herramientas de Windows.
    No usa SFML, SDL, Allegro ni ninguna libreria externa.

    Tecnologia usada:
    - C++ estandar para la logica del juego.
    - Win32 API para crear la ventana, procesar teclado y manejar el bucle.
    - GDI de Windows para dibujar rectangulos, circulos, lineas y textos.

    ==================================================
    ARQUITECTURA DEL PROGRAMA
    ==================================================

    El juego se organiza en estructuras y funciones separadas:

    - Edificio:
      Guarda rectangulo, color y dimensiones. Los edificios se generan de forma
      procedural al iniciar o reiniciar una ronda.

    - Jugador:
      Guarda nombre, posicion, color, puntaje y radio visual. Cada jugador se
      coloca sobre un edificio diferente.

    - Proyectil:
      Guarda posicion, velocidad y estado activo. Su movimiento se actualiza
      frame por frame con fisica basica.

    - Juego:
      Contiene todo el estado global de la partida: edificios, jugadores,
      turno, proyectil, angulo, velocidad, fase actual y mensaje.

    Funciones principales:

    - generarEdificios():
      Crea la ciudad con edificios de anchos y alturas variables.

    - crearJugadores():
      Posiciona los jugadores sobre edificios alejados entre si.

    - reiniciarRonda():
      Regenera el escenario, limpia la trayectoria y vuelve al estado de apuntar.

    - iniciarDisparo():
      Convierte el angulo y velocidad elegidos en componentes vx/vy.

    - actualizarProyectil():
      Integra la fisica usando delta time y gravedad.

    - detectarColisiones():
      Revisa impacto contra edificios, jugador enemigo o salida de pantalla.

    - dibujarJuego():
      Renderiza todo usando GDI: fondo, edificios, jugadores, trayectoria,
      proyectil, HUD, menu y mensajes.

    - WindowProc():
      Procesa eventos de Windows: teclado, temporizador, repintado y cierre.

    ==================================================
    FLUJO DEL JUEGO
    ==================================================

    1. Aparece un menu inicial.
    2. ENTER comienza la partida.
    3. El jugador activo ajusta angulo y velocidad con el teclado.
    4. ESPACIO dispara.
    5. El proyectil se mueve con trayectoria parabolica.
    6. Si impacta al rival, gana la ronda.
    7. Si impacta edificio o sale de pantalla, cambia el turno.
    8. R reinicia la ronda. ESC sale.

    ==================================================
    FISICA DEL DISPARO
    ==================================================

    Se usan las formulas basicas:

        vx = velocidad * cos(angulo)
        vy = velocidad * sin(angulo)

    En pantalla, la coordenada Y crece hacia abajo. Por eso la velocidad vertical
    inicial se guarda negativa cuando el proyectil sube.

    En cada frame:

        posicion.x += velocidad.x * dt
        posicion.y += velocidad.y * dt
        velocidad.y += gravedad * dt

    Esto produce una curva parabolica visible en la ventana.

    ==================================================
    CONTROLES
    ==================================================

    ENTER  : comenzar desde el menu
    ARRIBA : subir angulo
    ABAJO  : bajar angulo
    DERECHA: subir velocidad
    IZQUIERDA: bajar velocidad
    ESPACIO: disparar
    R      : reiniciar ronda
    ESC    : salir

*/

#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

const int ANCHO_VENTANA = 1000;
const int ALTO_VENTANA = 650;
const int ALTURA_HUD = 88;
const int SUELO_Y = 590;
const int RADIO_JUGADOR = 15;
const double PI = 3.14159265358979323846;
const double GRAVEDAD = 360.0;
const int ID_TIMER = 1;
const int FRAME_MS = 16;
const double POTENCIA_INICIAL = 55.0;
const double POTENCIA_MINIMA = 1.0;
const double POTENCIA_MAXIMA = 100.0;
const double VELOCIDAD_BASE = 160.0;
const double VELOCIDAD_POR_POTENCIA = 7.2;

enum class EstadoJuego {
    Menu,
    Apuntando,
    Disparando,
    RondaTerminada
};

enum class ResultadoColision {
    Ninguna,
    Edificio,
    Jugador,
    FueraPantalla
};

struct Vector2 {
    double x;
    double y;
};

struct Edificio {
    RECT rect;
    COLORREF color;
};

struct Jugador {
    string nombre;
    Vector2 posicion;
    COLORREF color;
    int rondasGanadas;
    bool vivo;
};

struct Proyectil {
    Vector2 posicion;
    Vector2 velocidad;
    bool activo;
};

struct Juego {
    vector<Edificio> edificios;
    vector<Vector2> trayectoria;
    Jugador jugador1;
    Jugador jugador2;
    Proyectil proyectil;
    EstadoJuego estado;
    int turnoActual;
    int ronda;
    double angulo;
    double velocidad;
    string mensaje;
    DWORD tiempoAnterior;
    double radioExplosion;
};

Juego g_juego;

int numeroAleatorio(int minimo, int maximo) {
    return minimo + rand() % (maximo - minimo + 1);
}

double gradosARadianes(double grados) {
    return grados * PI / 180.0;
}

int limitarEntero(int valor, int minimo, int maximo) {
    if (valor < minimo) return minimo;
    if (valor > maximo) return maximo;
    return valor;
}

double limitarDouble(double valor, double minimo, double maximo) {
    if (valor < minimo) return minimo;
    if (valor > maximo) return maximo;
    return valor;
}

double obtenerVelocidadDisparo(double potencia) {
    return VELOCIDAD_BASE + potencia * VELOCIDAD_POR_POTENCIA;
}

wstring convertirAWString(const string& texto) {
    return wstring(texto.begin(), texto.end());
}

void dibujarTexto(HDC hdc, int x, int y, const string& texto, COLORREF color, int tamano, bool negrita = false) {
    HFONT fuente = CreateFontA(
        tamano, 0, 0, 0,
        negrita ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI"
    );

    HFONT fuenteAnterior = static_cast<HFONT>(SelectObject(hdc, fuente));
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, x, y, texto.c_str(), static_cast<int>(texto.size()));
    SelectObject(hdc, fuenteAnterior);
    DeleteObject(fuente);
}

void rellenarRectangulo(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH pincel = CreateSolidBrush(color);
    FillRect(hdc, &rect, pincel);
    DeleteObject(pincel);
}

void dibujarElipse(HDC hdc, int x, int y, int radio, COLORREF relleno, COLORREF borde) {
    HBRUSH pincel = CreateSolidBrush(relleno);
    HPEN lapiz = CreatePen(PS_SOLID, 2, borde);
    HGDIOBJ pincelAnterior = SelectObject(hdc, pincel);
    HGDIOBJ lapizAnterior = SelectObject(hdc, lapiz);

    Ellipse(hdc, x - radio, y - radio, x + radio, y + radio);

    SelectObject(hdc, pincelAnterior);
    SelectObject(hdc, lapizAnterior);
    DeleteObject(pincel);
    DeleteObject(lapiz);
}

vector<Edificio> generarEdificios() {
    vector<Edificio> edificios;
    int x = 0;

    // Generacion procedural: cada edificio recibe ancho, altura y color.
    while (x < ANCHO_VENTANA) {
        int ancho = numeroAleatorio(55, 95);
        int altura = numeroAleatorio(130, 360);

        if (x + ancho > ANCHO_VENTANA) {
            ancho = ANCHO_VENTANA - x;
        }

        Edificio edificio;
        edificio.rect.left = x;
        edificio.rect.right = x + ancho;
        edificio.rect.bottom = SUELO_Y;
        edificio.rect.top = SUELO_Y - altura;
        edificio.color = RGB(numeroAleatorio(60, 100), numeroAleatorio(70, 115), numeroAleatorio(110, 160));

        edificios.push_back(edificio);
        x += ancho;
    }

    return edificios;
}

Vector2 puntoSobreEdificio(const Edificio& edificio) {
    return {
        static_cast<double>((edificio.rect.left + edificio.rect.right) / 2),
        static_cast<double>(edificio.rect.top - RADIO_JUGADOR)
    };
}

void crearJugadores(Juego& juego) {
    int total = static_cast<int>(juego.edificios.size());
    int indiceIzquierdo = numeroAleatorio(1, total / 3);
    int indiceDerecho = numeroAleatorio((total * 2) / 3, total - 2);

    juego.jugador1.nombre = "Jugador 1";
    juego.jugador1.color = RGB(230, 80, 80);
    juego.jugador1.posicion = puntoSobreEdificio(juego.edificios[indiceIzquierdo]);
    juego.jugador1.vivo = true;

    juego.jugador2.nombre = "Jugador 2";
    juego.jugador2.color = RGB(70, 180, 255);
    juego.jugador2.posicion = puntoSobreEdificio(juego.edificios[indiceDerecho]);
    juego.jugador2.vivo = true;
}

Jugador& jugadorActivo(Juego& juego) {
    return (juego.turnoActual == 1) ? juego.jugador1 : juego.jugador2;
}

Jugador& jugadorObjetivo(Juego& juego) {
    return (juego.turnoActual == 1) ? juego.jugador2 : juego.jugador1;
}

const Jugador& jugadorActivo(const Juego& juego) {
    return (juego.turnoActual == 1) ? juego.jugador1 : juego.jugador2;
}

const Jugador& jugadorObjetivo(const Juego& juego) {
    return (juego.turnoActual == 1) ? juego.jugador2 : juego.jugador1;
}

int obtenerDireccionDisparo(const Juego& juego) {
    const Jugador& atacante = jugadorActivo(juego);
    const Jugador& objetivo = jugadorObjetivo(juego);
    return (atacante.posicion.x < objetivo.posicion.x) ? 1 : -1;
}

Vector2 obtenerPosicionDisparo(const Juego& juego) {
    const Jugador& atacante = jugadorActivo(juego);
    return {
        atacante.posicion.x,
        atacante.posicion.y - RADIO_JUGADOR - 4
    };
}

void reiniciarRonda(Juego& juego, bool reiniciarMarcador) {
    if (reiniciarMarcador) {
        juego.jugador1.rondasGanadas = 0;
        juego.jugador2.rondasGanadas = 0;
        juego.ronda = 1;
    }

    juego.edificios = generarEdificios();
    juego.trayectoria.clear();
    juego.proyectil.activo = false;
    juego.turnoActual = 1;
    juego.angulo = 45.0;
    juego.velocidad = POTENCIA_INICIAL;
    juego.estado = EstadoJuego::Apuntando;
    juego.mensaje = "Ajuste angulo y potencia. Los puntos muestran donde caera el disparo.";
    juego.radioExplosion = 0.0;

    crearJugadores(juego);
}

void inicializarJuego(Juego& juego) {
    juego.jugador1 = {"Jugador 1", {0, 0}, RGB(230, 80, 80), 0, true};
    juego.jugador2 = {"Jugador 2", {0, 0}, RGB(70, 180, 255), 0, true};
    juego.proyectil = {{0, 0}, {0, 0}, false};
    juego.ronda = 1;
    juego.estado = EstadoJuego::Menu;
    juego.turnoActual = 1;
    juego.angulo = 45.0;
    juego.velocidad = POTENCIA_INICIAL;
    juego.mensaje = "Presione ENTER para comenzar.";
    juego.tiempoAnterior = GetTickCount();
    juego.radioExplosion = 0.0;
}

void iniciarDisparo(Juego& juego) {
    int direccion = obtenerDireccionDisparo(juego);
    double radianes = gradosARadianes(juego.angulo);
    double velocidadDisparo = obtenerVelocidadDisparo(juego.velocidad);

    juego.proyectil.posicion = obtenerPosicionDisparo(juego);

    // vx y vy salen del angulo elegido. En GDI, Y positiva va hacia abajo,
    // por eso vy inicial es negativa cuando el proyectil sube.
    juego.proyectil.velocidad.x = cos(radianes) * velocidadDisparo * direccion;
    juego.proyectil.velocidad.y = -sin(radianes) * velocidadDisparo;
    juego.proyectil.activo = true;

    juego.trayectoria.clear();
    juego.estado = EstadoJuego::Disparando;
    juego.mensaje = "Proyectil en movimiento...";
    juego.tiempoAnterior = GetTickCount();
}

bool puntoEnRectangulo(const Vector2& punto, const RECT& rect) {
    return punto.x >= rect.left && punto.x <= rect.right &&
           punto.y >= rect.top && punto.y <= rect.bottom;
}

bool proyectilGolpeaJugador(const Proyectil& proyectil, const Jugador& jugador) {
    double dx = proyectil.posicion.x - jugador.posicion.x;
    double dy = proyectil.posicion.y - jugador.posicion.y;
    double distancia = sqrt(dx * dx + dy * dy);
    return distancia <= RADIO_JUGADOR + 6;
}

ResultadoColision detectarColisiones(const Juego& juego) {
    const Proyectil& proyectil = juego.proyectil;
    const Jugador& objetivo = jugadorObjetivo(juego);

    if (proyectil.posicion.x < 0 || proyectil.posicion.x > ANCHO_VENTANA ||
        proyectil.posicion.y < 0 || proyectil.posicion.y > ALTO_VENTANA) {
        return ResultadoColision::FueraPantalla;
    }

    if (proyectilGolpeaJugador(proyectil, objetivo)) {
        return ResultadoColision::Jugador;
    }

    for (const Edificio& edificio : juego.edificios) {
        if (puntoEnRectangulo(proyectil.posicion, edificio.rect)) {
            return ResultadoColision::Edificio;
        }
    }

    return ResultadoColision::Ninguna;
}

void cambiarTurno(Juego& juego) {
    juego.turnoActual = (juego.turnoActual == 1) ? 2 : 1;
    juego.estado = EstadoJuego::Apuntando;
}

void resolverColision(Juego& juego, ResultadoColision resultado) {
    Jugador& atacante = jugadorActivo(juego);
    Jugador& objetivo = jugadorObjetivo(juego);

    juego.proyectil.activo = false;
    juego.radioExplosion = 8.0;

    if (resultado == ResultadoColision::Jugador) {
        objetivo.vivo = false;
        atacante.rondasGanadas++;
        juego.estado = EstadoJuego::RondaTerminada;
        juego.mensaje = atacante.nombre + " gana la ronda. R: nueva ronda | ESC: salir.";
    } else if (resultado == ResultadoColision::Edificio) {
        juego.mensaje = "Impacto contra edificio. Cambia el turno.";
        cambiarTurno(juego);
    } else if (resultado == ResultadoColision::FueraPantalla) {
        juego.mensaje = "El proyectil salio de la pantalla. Cambia el turno.";
        cambiarTurno(juego);
    }
}

void actualizarProyectil(Juego& juego) {
    if (!juego.proyectil.activo) {
        return;
    }

    DWORD ahora = GetTickCount();
    double dt = (ahora - juego.tiempoAnterior) / 1000.0;
    juego.tiempoAnterior = ahora;

    if (dt <= 0.0 || dt > 0.1) {
        dt = FRAME_MS / 1000.0;
    }

    // Integracion simple de fisica por delta time.
    juego.proyectil.posicion.x += juego.proyectil.velocidad.x * dt;
    juego.proyectil.posicion.y += juego.proyectil.velocidad.y * dt;
    juego.proyectil.velocidad.y += GRAVEDAD * dt;

    juego.trayectoria.push_back(juego.proyectil.posicion);

    ResultadoColision resultado = detectarColisiones(juego);
    if (resultado != ResultadoColision::Ninguna) {
        resolverColision(juego, resultado);
    }
}

void actualizarExplosion(Juego& juego) {
    if (juego.radioExplosion > 0.0) {
        juego.radioExplosion += 2.2;
        if (juego.radioExplosion > 34.0) {
            juego.radioExplosion = 0.0;
        }
    }
}

void dibujarFondo(HDC hdc) {
    RECT cielo = {0, 0, ANCHO_VENTANA, SUELO_Y};
    RECT suelo = {0, SUELO_Y, ANCHO_VENTANA, ALTO_VENTANA};

    rellenarRectangulo(hdc, cielo, RGB(25, 30, 48));
    rellenarRectangulo(hdc, suelo, RGB(35, 55, 45));

    // Luna simple.
    dibujarElipse(hdc, 880, 70, 28, RGB(238, 230, 180), RGB(238, 230, 180));
}

void dibujarEdificios(HDC hdc, const vector<Edificio>& edificios) {
    for (const Edificio& edificio : edificios) {
        rellenarRectangulo(hdc, edificio.rect, edificio.color);

        HPEN borde = CreatePen(PS_SOLID, 2, RGB(20, 25, 35));
        HGDIOBJ bordeAnterior = SelectObject(hdc, borde);
        HGDIOBJ pincelAnterior = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, edificio.rect.left, edificio.rect.top, edificio.rect.right, edificio.rect.bottom);
        SelectObject(hdc, bordeAnterior);
        SelectObject(hdc, pincelAnterior);
        DeleteObject(borde);

        // Ventanas calculadas por coordenadas, no dibujadas como imagen fija.
        for (int y = edificio.rect.top + 18; y < edificio.rect.bottom - 15; y += 32) {
            for (int x = edificio.rect.left + 12; x < edificio.rect.right - 14; x += 24) {
                RECT ventana = {x, y, x + 9, y + 13};
                COLORREF colorVentana = ((x + y) % 3 == 0) ? RGB(245, 220, 110) : RGB(45, 60, 88);
                rellenarRectangulo(hdc, ventana, colorVentana);
            }
        }
    }
}

void dibujarJugador(HDC hdc, const Jugador& jugador) {
    int x = static_cast<int>(round(jugador.posicion.x));
    int y = static_cast<int>(round(jugador.posicion.y));

    COLORREF color = jugador.vivo ? jugador.color : RGB(120, 120, 120);
    dibujarElipse(hdc, x, y, RADIO_JUGADOR, color, RGB(10, 10, 10));

    RECT cuerpo = {x - 10, y + RADIO_JUGADOR - 2, x + 10, y + RADIO_JUGADOR + 25};
    rellenarRectangulo(hdc, cuerpo, color);

    HPEN lapiz = CreatePen(PS_SOLID, 3, RGB(10, 10, 10));
    HGDIOBJ anterior = SelectObject(hdc, lapiz);
    MoveToEx(hdc, x - 18, y + 22, nullptr);
    LineTo(hdc, x + 18, y + 22);
    MoveToEx(hdc, x - 6, y + 40, nullptr);
    LineTo(hdc, x - 17, y + 55);
    MoveToEx(hdc, x + 6, y + 40, nullptr);
    LineTo(hdc, x + 17, y + 55);
    SelectObject(hdc, anterior);
    DeleteObject(lapiz);
}

void dibujarTrayectoria(HDC hdc, const vector<Vector2>& trayectoria) {
    HBRUSH pincel = CreateSolidBrush(RGB(255, 235, 130));
    HGDIOBJ anterior = SelectObject(hdc, pincel);

    for (const Vector2& punto : trayectoria) {
        int x = static_cast<int>(round(punto.x));
        int y = static_cast<int>(round(punto.y));
        Ellipse(hdc, x - 2, y - 2, x + 2, y + 2);
    }

    SelectObject(hdc, anterior);
    DeleteObject(pincel);
}

void dibujarProyectil(HDC hdc, const Proyectil& proyectil) {
    if (!proyectil.activo) {
        return;
    }

    int x = static_cast<int>(round(proyectil.posicion.x));
    int y = static_cast<int>(round(proyectil.posicion.y));
    dibujarElipse(hdc, x, y, 6, RGB(255, 210, 80), RGB(255, 255, 255));
}

void dibujarExplosion(HDC hdc, const Juego& juego) {
    if (juego.radioExplosion <= 0.0 || juego.trayectoria.empty()) {
        return;
    }

    Vector2 centro = juego.trayectoria.back();
    int x = static_cast<int>(round(centro.x));
    int y = static_cast<int>(round(centro.y));
    int radio = static_cast<int>(round(juego.radioExplosion));

    HPEN lapiz = CreatePen(PS_SOLID, 3, RGB(255, 180, 60));
    HGDIOBJ anterior = SelectObject(hdc, lapiz);
    HGDIOBJ pincelAnterior = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, x - radio, y - radio, x + radio, y + radio);
    SelectObject(hdc, anterior);
    SelectObject(hdc, pincelAnterior);
    DeleteObject(lapiz);
}

vector<Vector2> calcularProyeccionDisparo(const Juego& juego) {
    vector<Vector2> puntos;
    Vector2 posicion = obtenerPosicionDisparo(juego);
    double radianes = gradosARadianes(juego.angulo);
    double velocidadDisparo = obtenerVelocidadDisparo(juego.velocidad);
    int direccion = obtenerDireccionDisparo(juego);

    Vector2 velocidad = {
        cos(radianes) * velocidadDisparo * direccion,
        -sin(radianes) * velocidadDisparo
    };

    const double dt = 0.04;
    const int maximoPasos = 260;

    for (int paso = 0; paso < maximoPasos; paso++) {
        posicion.x += velocidad.x * dt;
        posicion.y += velocidad.y * dt;
        velocidad.y += GRAVEDAD * dt;

        if (paso % 4 == 0) {
            puntos.push_back(posicion);
        }

        if (posicion.x < 0 || posicion.x > ANCHO_VENTANA ||
            posicion.y < ALTURA_HUD || posicion.y > ALTO_VENTANA) {
            break;
        }

        for (const Edificio& edificio : juego.edificios) {
            if (puntoEnRectangulo(posicion, edificio.rect)) {
                return puntos;
            }
        }

        if (proyectilGolpeaJugador({posicion, velocidad, true}, jugadorObjetivo(juego))) {
            return puntos;
        }
    }

    return puntos;
}

void dibujarProyeccionDisparo(HDC hdc, const Juego& juego) {
    if (juego.estado != EstadoJuego::Apuntando) {
        return;
    }

    vector<Vector2> puntos = calcularProyeccionDisparo(juego);
    if (puntos.empty()) {
        return;
    }

    HBRUSH pincel = CreateSolidBrush(RGB(150, 230, 255));
    HGDIOBJ anterior = SelectObject(hdc, pincel);

    for (size_t i = 0; i < puntos.size(); i++) {
        int x = static_cast<int>(round(puntos[i].x));
        int y = static_cast<int>(round(puntos[i].y));
        int radio = (i + 1 == puntos.size()) ? 5 : 3;
        Ellipse(hdc, x - radio, y - radio, x + radio, y + radio);
    }

    SelectObject(hdc, anterior);
    DeleteObject(pincel);
}

void dibujarMira(HDC hdc, const Juego& juego) {
    if (juego.estado != EstadoJuego::Apuntando) {
        return;
    }

    int direccion = obtenerDireccionDisparo(juego);
    double radianes = gradosARadianes(juego.angulo);
    double largoMira = 35.0 + juego.velocidad * 0.65;
    Vector2 origen = obtenerPosicionDisparo(juego);

    int x1 = static_cast<int>(round(origen.x));
    int y1 = static_cast<int>(round(origen.y));
    int x2 = x1 + static_cast<int>(cos(radianes) * largoMira * direccion);
    int y2 = y1 - static_cast<int>(sin(radianes) * largoMira);

    HPEN lapiz = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    HGDIOBJ anterior = SelectObject(hdc, lapiz);
    MoveToEx(hdc, x1, y1, nullptr);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, anterior);
    DeleteObject(lapiz);
}

void dibujarHUD(HDC hdc, const Juego& juego) {
    RECT hud = {0, 0, ANCHO_VENTANA, ALTURA_HUD};
    rellenarRectangulo(hdc, hud, RGB(15, 18, 28));

    const Jugador& activo = jugadorActivo(juego);
    ostringstream linea1;
    linea1 << "Ronda " << juego.ronda
           << "   |   Jugador 1: " << juego.jugador1.rondasGanadas
           << "   Jugador 2: " << juego.jugador2.rondasGanadas;

    ostringstream linea2;
    linea2 << "Turno: " << activo.nombre
           << "   Angulo: " << static_cast<int>(juego.angulo)
           << "   Potencia: " << static_cast<int>(juego.velocidad) << "%";

    dibujarTexto(hdc, 18, 12, "Gorilla.bas 2D - WinAPI/GDI", RGB(240, 240, 240), 22, true);
    dibujarTexto(hdc, 18, 42, linea1.str(), RGB(210, 220, 240), 18);
    dibujarTexto(hdc, 430, 42, linea2.str(), activo.color, 18, true);
    dibujarTexto(hdc, 18, 66, juego.mensaje, RGB(230, 230, 160), 16);
}

void dibujarMenu(HDC hdc) {
    dibujarFondo(hdc);
    dibujarTexto(hdc, 285, 180, "GORILLA.BAS 2D", RGB(255, 235, 150), 48, true);
    dibujarTexto(hdc, 270, 250, "Version grafica usando solo Windows API + GDI", RGB(235, 235, 235), 24);
    dibujarTexto(hdc, 290, 315, "ENTER: comenzar", RGB(180, 230, 255), 24, true);
    dibujarTexto(hdc, 290, 355, "Flechas: ajustar angulo y potencia", RGB(220, 220, 220), 20);
    dibujarTexto(hdc, 290, 385, "ESPACIO: disparar    R: reiniciar    ESC: salir", RGB(220, 220, 220), 20);
}

void dibujarJuego(HDC hdc, const Juego& juego) {
    if (juego.estado == EstadoJuego::Menu) {
        dibujarMenu(hdc);
        return;
    }

    dibujarFondo(hdc);
    dibujarEdificios(hdc, juego.edificios);
    dibujarTrayectoria(hdc, juego.trayectoria);
    dibujarProyeccionDisparo(hdc, juego);
    dibujarMira(hdc, juego);
    dibujarJugador(hdc, juego.jugador1);
    dibujarJugador(hdc, juego.jugador2);
    dibujarProyectil(hdc, juego.proyectil);
    dibujarExplosion(hdc, juego);
    dibujarHUD(hdc, juego);

    if (juego.estado == EstadoJuego::RondaTerminada) {
        RECT cartel = {260, 235, 740, 350};
        rellenarRectangulo(hdc, cartel, RGB(20, 22, 34));
        dibujarTexto(hdc, 300, 265, "RONDA TERMINADA", RGB(255, 235, 150), 32, true);
        dibujarTexto(hdc, 300, 315, "Presione R para otra ronda o ESC para salir.", RGB(235, 235, 235), 19);
    }
}

void procesarTecla(HWND hwnd, WPARAM tecla) {
    if (tecla == VK_ESCAPE) {
        PostQuitMessage(0);
        return;
    }

    if (g_juego.estado == EstadoJuego::Menu) {
        if (tecla == VK_RETURN) {
            reiniciarRonda(g_juego, true);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (tecla == 'R') {
        if (g_juego.estado == EstadoJuego::RondaTerminada) {
            g_juego.ronda++;
            reiniciarRonda(g_juego, false);
        } else {
            reiniciarRonda(g_juego, false);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_juego.estado != EstadoJuego::Apuntando) {
        return;
    }

    if (tecla == VK_UP) {
        g_juego.angulo = limitarDouble(g_juego.angulo + 1.0, 0.0, 90.0);
    } else if (tecla == VK_DOWN) {
        g_juego.angulo = limitarDouble(g_juego.angulo - 1.0, 0.0, 90.0);
    } else if (tecla == VK_RIGHT) {
        g_juego.velocidad = limitarDouble(g_juego.velocidad + 5.0, POTENCIA_MINIMA, POTENCIA_MAXIMA);
    } else if (tecla == VK_LEFT) {
        g_juego.velocidad = limitarDouble(g_juego.velocidad - 5.0, POTENCIA_MINIMA, POTENCIA_MAXIMA);
    } else if (tecla == VK_SPACE) {
        iniciarDisparo(g_juego);
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT mensaje, WPARAM wParam, LPARAM lParam) {
    switch (mensaje) {
        case WM_CREATE:
            SetTimer(hwnd, ID_TIMER, FRAME_MS, nullptr);
            return 0;

        case WM_TIMER:
            actualizarProyectil(g_juego);
            actualizarExplosion(g_juego);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_KEYDOWN:
            procesarTecla(hwnd, wParam);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Doble buffer manual para evitar parpadeos.
            HDC memoria = CreateCompatibleDC(hdc);
            HBITMAP bitmap = CreateCompatibleBitmap(hdc, ANCHO_VENTANA, ALTO_VENTANA);
            HGDIOBJ anterior = SelectObject(memoria, bitmap);

            dibujarJuego(memoria, g_juego);

            BitBlt(hdc, 0, 0, ANCHO_VENTANA, ALTO_VENTANA, memoria, 0, 0, SRCCOPY);

            SelectObject(memoria, anterior);
            DeleteObject(bitmap);
            DeleteDC(memoria);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, ID_TIMER);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, mensaje, wParam, lParam);
}

int ejecutarAplicacion(HINSTANCE instancia, int mostrar) {
    srand(static_cast<unsigned int>(time(nullptr)));
    inicializarJuego(g_juego);

    const char NOMBRE_CLASE[] = "GorillaBasWinAPI";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instancia;
    wc.lpszClassName = NOMBRE_CLASE;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassA(&wc);

    RECT rect = {0, 0, ANCHO_VENTANA, ALTO_VENTANA};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        NOMBRE_CLASE,
        "TP Gorilla.bas - Juego 2D con WinAPI",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instancia,
        nullptr
    );

    if (hwnd == nullptr) {
        return 1;
    }

    ShowWindow(hwnd, mostrar);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}

int WINAPI WinMain(HINSTANCE instancia, HINSTANCE, LPSTR, int mostrar) {
    return ejecutarAplicacion(instancia, mostrar);
}

int main() {
    return ejecutarAplicacion(GetModuleHandle(nullptr), SW_SHOW);
}
