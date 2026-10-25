#include <WiFi.h>
#include <WebServer.h>

// Configuración de la Red Wi-Fi AP
const char* ssid = "LNR_Sports";
const char* claveRed = "voley1234";

WebServer servidor(80);

// Variables de Juego y Modos (Vóley)
int puntosLocal = 0;
int puntosVisitante = 0;
int setsLocal = 0;
int setsVisitante = 0;
int modalidadSets = 3; // 3 para Mejor de 3, 5 para Mejor de 5
int modoOperacion = 0; // 0 = Vóley, 1 = Reloj (Hora), 2 = Libre, 3 = Laureles

// Variables independientes para el Modo Libre (0 a 99 por lado)
int puntosLibreIzq = 0;
int puntosLibreDer = 0;

// Variables para el Modo Reloj
int horaReloj = 12;
int minutoReloj = 0;

// Variables para el desplazamiento del texto "LaUrElES"
unsigned long ultimoDesplazamiento = 0;
int indiceScroll = 0;
const int velocidadScroll = 350; 

// ==========================================
// ASIGNACIÓN DE PINES (según tu imagen)
// ==========================================
const int segLocal[7] = {13, 12, 14, 27, 26, 25, 33};       // a, b, c, d, e, f, g (Local)
const int segvisitante[7] = {23, 22, 21, 19, 18, 5, 17};   // a, b, c, d, e, f, g (Visitante)

const int digLocal[2] = {32, 15};                          // [0] = Decena Local, [1] = Unidad Local
const int digvisitante[2] = {16, 4};                       // [0] = Decena Visitante, [1] = Unidad Visitante

// Tabla de números (0-9) para 7 segmentos
const byte numeros[10][7] = {
  {1, 1, 1, 1, 1, 1, 0}, // 0
  {0, 1, 1, 0, 0, 0, 0}, // 1
  {1, 1, 0, 1, 1, 0, 1}, // 2
  {1, 1, 1, 1, 0, 0, 1}, // 3
  {0, 1, 1, 0, 0, 1, 1}, // 4
  {1, 0, 1, 1, 0, 1, 1}, // 5
  {1, 0, 1, 1, 1, 1, 1}, // 6
  {1, 1, 1, 0, 0, 0, 0}, // 7
  {1, 1, 1, 1, 1, 1, 1}, // 8
  {1, 1, 1, 1, 0, 1, 1}  // 9
};

// Declaración de funciones
void responderPagina();
void handleRoot();
void barrerDisplays();
void mostrarSegmentosLocal(int num);
void mostrarSegmentosVisitante(int num);
void apagarTodo();
void verificarSetAutomatico();
void obtenerSegmentosCaracter(char c, byte segs[7]);
String paginaWeb();

void setup() {
  Serial.begin(115200);

  // Configurar pines de segmentos como salida
  for (int i = 0; i < 7; i++) {
    pinMode(segLocal[i], OUTPUT);
    pinMode(segvisitante[i], OUTPUT);
  }

  // Configurar pines de activación de dígitos como salida
  pinMode(digLocal[0], OUTPUT);
  pinMode(digLocal[1], OUTPUT);
  pinMode(digvisitante[0], OUTPUT);
  pinMode(digvisitante[1], OUTPUT);

  apagarTodo();

  // Crear Red Wi-Fi en modo Access Point (AP)
  WiFi.softAP(ssid, claveRed);
  Serial.print("Punto de acceso creado. IP: ");
  Serial.println(WiFi.softAPIP());

  // Definición de rutas del Servidor Web
  servidor.on("/", handleRoot);
  
  // Controles Vóley - Local
  servidor.on("/incL", []() { 
    int metaSets = (modalidadSets == 3) ? 2 : 3;
    if (setsLocal < metaSets && setsVisitante < metaSets) {
      puntosLocal = constrain(puntosLocal + 1, 0, 99);
      verificarSetAutomatico();
    }
    responderPagina(); 
  });
  servidor.on("/decL", []() { puntosLocal = constrain(puntosLocal - 1, 0, 99); responderPagina(); });
  
  // Controles Vóley - Visitante
  servidor.on("/incV", []() { 
    int metaSets = (modalidadSets == 3) ? 2 : 3;
    if (setsLocal < metaSets && setsVisitante < metaSets) {
      puntosVisitante = constrain(puntosVisitante + 1, 0, 99);
      verificarSetAutomatico();
    }
    responderPagina(); 
  });
  servidor.on("/decV", []() { puntosVisitante = constrain(puntosVisitante - 1, 0, 99); responderPagina(); });

  // Controles Independientes - Modo Libre
  servidor.on("/incLibreIzq", []() { puntosLibreIzq = constrain(puntosLibreIzq + 1, 0, 99); responderPagina(); });
  servidor.on("/decLibreIzq", []() { puntosLibreIzq = constrain(puntosLibreIzq - 1, 0, 99); responderPagina(); });
  servidor.on("/incLibreDer", []() { puntosLibreDer = constrain(puntosLibreDer + 1, 0, 99); responderPagina(); });
  servidor.on("/decLibreDer", []() { puntosLibreDer = constrain(puntosLibreDer - 1, 0, 99); responderPagina(); });

  servidor.on("/setModalidad3", []() { modalidadSets = 3; responderPagina(); });
  servidor.on("/setModalidad5", []() { modalidadSets = 5; responderPagina(); });

  servidor.on("/modoVoley", []() { modoOperacion = 0; responderPagina(); });
  servidor.on("/modoReloj", []() { modoOperacion = 1; responderPagina(); });
  servidor.on("/modoLibre", []() { modoOperacion = 2; responderPagina(); });
  servidor.on("/modoLaureles", []() { modoOperacion = 3; responderPagina(); });

  servidor.on("/reset", []() {
    puntosLocal = 0; puntosVisitante = 0;
    setsLocal = 0; setsVisitante = 0;
    puntosLibreIzq = 0; puntosLibreDer = 0;
    responderPagina();
  });

  servidor.on("/sincronizarHora", []() {
    if (servidor.hasArg("h") && servidor.hasArg("m")) {
      horaReloj = servidor.arg("h").toInt();
      minutoReloj = servidor.arg("m").toInt();
    }
    servidor.send(200, "text/plain", "OK");
  });

  servidor.begin();
  Serial.println("Servidor web iniciado en puerto 80");
}

void loop() {
  servidor.handleClient();
  barrerDisplays();
}

void verificarSetAutomatico() {
  if (puntosLocal >= 25 && (puntosLocal - puntosVisitante) >= 2) {
    setsLocal++;
    puntosLocal = 0;
    puntosVisitante = 0;
  } else if (puntosVisitante >= 25 && (puntosVisitante - puntosLocal) >= 2) {
    setsVisitante++;
    puntosLocal = 0;
    puntosVisitante = 0;
  }
}

void handleRoot() {
  responderPagina();
}

void responderPagina() {
  servidor.send(200, "text/html", paginaWeb());
}

// Diccionario personalizado con los segmentos exactos pedidos
void obtenerSegmentosCaracter(char c, byte segs[7]) {
  for(int i=0; i<7; i++) segs[i] = 0;
  
  if (c >= '0' && c <= '9') {
    int num = c - '0';
    for(int i=0; i<7; i++) segs[i] = numeros[num][i];
    return;
  }
  
  switch (c) {
    case 'L': case 'l': // L Mayúscula (d, e, f)
      segs[3]=1; segs[4]=1; segs[5]=1; break;
      
    case 'a': // a minúscula con palito arriba (a, b, c, d, e, g)
      segs[0]=1; segs[1]=1; segs[2]=1; segs[3]=1; segs[4]=1; segs[6]=1; break;
      
    case 'U': case 'u': // U grande (b, c, d, e, f)
      segs[1]=1; segs[2]=1; segs[3]=1; segs[4]=1; segs[5]=1; break;
      
    case 'r': case 'R': // r usando segmentos a, e, f
      segs[0]=1; segs[4]=1; segs[5]=1; break;
      
    case 'e': case 'E': // e usando segmentos a, b, d, e, f, g
      segs[0]=1; segs[1]=1; segs[3]=1; segs[4]=1; segs[5]=1; segs[6]=1; break;
      
    case 'S': case 's': // S Mayúscula (a, c, d, f, g)
      segs[0]=1; segs[2]=1; segs[3]=1; segs[5]=1; segs[6]=1; break;
      
    default:
      break; 
  }
}

void barrerDisplays() {
  if (modoOperacion == 3) {
    String mensaje = "   LaUrElES   "; 
    int largo = mensaje.length();

    if (millis() - ultimoDesplazamiento > velocidadScroll) {
      ultimoDesplazamiento = millis();
      indiceScroll = (indiceScroll + 1) % largo;
    }

    char c0 = mensaje[(indiceScroll + 0) % largo];
    char c1 = mensaje[(indiceScroll + 1) % largo];
    char c2 = mensaje[(indiceScroll + 2) % largo];
    char c3 = mensaje[(indiceScroll + 3) % largo];

    byte segs0[7], segs1[7], segs2[7], segs3[7];
    obtenerSegmentosCaracter(c0, segs0);
    obtenerSegmentosCaracter(c1, segs1);
    obtenerSegmentosCaracter(c2, segs2);
    obtenerSegmentosCaracter(c3, segs3);

    // 1. Decena Local
    apagarTodo();
    for(int i=0; i<7; i++) digitalWrite(segLocal[i], segs0[i]);
    digitalWrite(digLocal[0], HIGH);
    delay(3);

    // 2. Unidad Local
    apagarTodo();
    for(int i=0; i<7; i++) digitalWrite(segLocal[i], segs1[i]);
    digitalWrite(digLocal[1], HIGH);
    delay(3);

    // 3. Decena Visitante
    apagarTodo();
    for(int i=0; i<7; i++) digitalWrite(segvisitante[i], segs2[i]);
    digitalWrite(digvisitante[0], HIGH);
    delay(3);

    // 4. Unidad Visitante
    apagarTodo();
    for(int i=0; i<7; i++) digitalWrite(segvisitante[i], segs3[i]);
    digitalWrite(digvisitante[1], HIGH);
    delay(3);

    return;
  }

  int valDecLocal = 0, valUnLocal = 0;
  int valDecVisitante = 0, valUnVisitante = 0;

  if (modoOperacion == 0) { // Vóley
    valDecLocal = puntosLocal / 10;
    valUnLocal = puntosLocal % 10;
    valDecVisitante = puntosVisitante / 10;
    valUnVisitante = puntosVisitante % 10;
  } 
  else if (modoOperacion == 2) { // Libre (Independiente)
    valDecLocal = puntosLibreIzq / 10;
    valUnLocal = puntosLibreIzq % 10;
    valDecVisitante = puntosLibreDer / 10;
    valUnVisitante = puntosLibreDer % 10;
  }
  else if (modoOperacion == 1) { // Reloj
    valDecLocal = horaReloj / 10;
    valUnLocal = horaReloj % 10;
    valDecVisitante = minutoReloj / 10;
    valUnVisitante = minutoReloj % 10;
  }

  // 1. Decena Local
  apagarTodo();
  mostrarSegmentosLocal(valDecLocal);
  digitalWrite(digLocal[0], HIGH);
  delay(3);

  // 2. Unidad Local
  apagarTodo();
  mostrarSegmentosLocal(valUnLocal);
  digitalWrite(digLocal[1], HIGH);
  delay(3);

  // 3. Decena Visitante
  apagarTodo();
  mostrarSegmentosVisitante(valDecVisitante);
  digitalWrite(digvisitante[0], HIGH);
  delay(3);

  // 4. Unidad Visitante
  apagarTodo();
  mostrarSegmentosVisitante(valUnVisitante);
  digitalWrite(digvisitante[1], HIGH);
  delay(3);
}

void mostrarSegmentosLocal(int num) {
  for (int i = 0; i < 7; i++) digitalWrite(segLocal[i], numeros[num][i]);
}

void mostrarSegmentosVisitante(int num) {
  for (int i = 0; i < 7; i++) digitalWrite(segvisitante[i], numeros[num][i]);
}

void apagarTodo() {
  digitalWrite(digLocal[0], LOW);
  digitalWrite(digLocal[1], LOW);
  digitalWrite(digvisitante[0], LOW);
  digitalWrite(digvisitante[1], LOW);
  for (int i = 0; i < 7; i++) {
    digitalWrite(segLocal[i], LOW);
    digitalWrite(segvisitante[i], LOW);
  }
}

String paginaWeb() {
  String ptr = "";
  ptr += "<!DOCTYPE html><html><head>";
  ptr += "<meta charset=\"UTF-8\">";
  ptr += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  ptr += "<style>";
  ptr += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background-color: #0b0f19; color: #f8fafc; margin: 0; padding: 15px; }";
  ptr += "h1 { font-size: 1.6rem; color: #38bdf8; margin-bottom: 12px; font-weight: 800; letter-spacing: 1px; }";
  ptr += ".card { background: #1e293b; padding: 18px; margin: 12px auto; border-radius: 14px; width: 95%; max-width: 420px; box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.4); border: 1px solid #334155; }";
  ptr += "h2 { font-size: 1.5rem; margin: 8px 0; color: #fbbf24; font-weight: 700; }";
  ptr += ".score-num { font-size: 2.2rem; font-weight: 900; color: #38bdf8; margin: 5px 0; }";
  ptr += "p { font-size: 1.1rem; margin: 8px 0; color: #cbd5e1; }";
  ptr += ".btn-group { display: flex; gap: 8px; justify-content: center; margin-top: 10px; flex-wrap: wrap; }";
  ptr += ".btn { color: white; padding: 16px 0; width: 48%; border: none; border-radius: 12px; cursor: pointer; font-size: 1.2rem; font-weight: bold; text-decoration: none; display: inline-block; box-shadow: 0 4px 12px rgba(0,0,0,0.3); transition: transform 0.1s, filter 0.2s; }";
  ptr += ".btn:active { transform: scale(0.95); }";
  ptr += ".btn-plus { background: linear-gradient(135deg, #16a34a, #15803d); }";
  ptr += ".btn-minus { background: linear-gradient(135deg, #dc2626, #b91c1c); }";
  ptr += ".btn-mode { background: linear-gradient(135deg, #475569, #334155); font-size: 0.85rem; padding: 10px 8px; width: 23%; border-radius: 8px; }";
  ptr += ".active { background: linear-gradient(135deg, #f59e0b, #d97706) !important; color: #000; font-weight: 900; }";
  ptr += ".btn-reset { background: linear-gradient(135deg, #78350f, #451a03); border: 1px solid #b45309; color: #fde68a; width: 100%; margin-top: 15px; padding: 14px; font-size: 1.1rem; border-radius: 12px; }";
  ptr += ".winner-banner { background: linear-gradient(135deg, #16a34a, #15803d); color: white; padding: 20px; border-radius: 14px; font-size: 1.4rem; font-weight: 900; margin: 15px auto; max-width: 420px; box-shadow: 0 0 25px rgba(22, 163, 74, 0.6); animation: pulse 1.5s infinite; }";
  ptr += "@keyframes pulse { 0% { transform: scale(1); } 50% { transform: scale(1.02); } 100% { transform: scale(1); } }";
  ptr += "</style>";
  
  ptr += "<script>";
  ptr += "document.addEventListener('click', function(e) {";
  ptr += "  var target = e.target.closest('a');";
  ptr += "  if (target && target.getAttribute('href') && target.getAttribute('href').startsWith('/')) {";
  ptr += "    e.preventDefault();";
  ptr += "    fetch(target.getAttribute('href'))";
  ptr += "      .then(response => response.text())";
  ptr += "      .then(html => {";
  ptr += "        var parser = new DOMParser();";
  ptr += "        var doc = parser.parseFromString(html, 'text/html');";
  ptr += "        document.body.innerHTML = doc.body.innerHTML;";
  ptr += "      });";
  ptr += "  }";
  ptr += "});";

  if (modoOperacion == 1) {
    ptr += "function enviarHora() {";
    ptr += "  var d = new Date();";
    ptr += "  fetch('/sincronizarHora?h=' + d.getHours() + '&m=' + d.getMinutes());";
    ptr += "}";
    ptr += "setInterval(enviarHora, 3000);";
    ptr += "window.onload = enviarHora;";
  }
  ptr += "</script>";

  ptr += "</head><body>";
  ptr += "<h1>⚡ LNR SPORTS ⚡</h1>";

  ptr += "<div class=\"card\" style=\"padding: 10px;\">";
  ptr += "<div class=\"btn-group\">";
  ptr += "<a class=\"btn btn-mode " + String(modoOperacion == 0 ? "active" : "") + "\" href=\"/modoVoley\">Vóley</a>";
  ptr += "<a class=\"btn btn-mode " + String(modoOperacion == 1 ? "active" : "") + "\" href=\"/modoReloj\">Reloj</a>";
  ptr += "<a class=\"btn btn-mode " + String(modoOperacion == 2 ? "active" : "") + "\" href=\"/modoLibre\">Libre</a>";
  ptr += "<a class=\"btn btn-mode " + String(modoOperacion == 3 ? "active" : "") + "\" href=\"/modoLaureles\">Laureles</a>";
  ptr += "</div></div>";

  if (modoOperacion == 0) { 
    int metaSets = (modalidadSets == 3) ? 2 : 3;
    
    ptr += "<div class=\"card\" style=\"padding: 10px;\"><p style=\"margin:0; font-size:0.95rem;\">Modalidad: <b>Mejor de " + String(modalidadSets) + " Sets</b></p>";
    ptr += "<div class=\"btn-group\" style=\"margin-top:8px;\">";
    ptr += "<a class=\"btn btn-mode " + String(modalidadSets == 3 ? "active" : "") + "\" style=\"width:48%\" href=\"/setModalidad3\">Mejor de 3</a>";
    ptr += "<a class=\"btn btn-mode " + String(modalidadSets == 5 ? "active" : "") + "\" style=\"width:48%\" href=\"/setModalidad5\">Mejor de 5</a>";
    ptr += "</div></div>";

    if (setsLocal >= metaSets) {
      ptr += "<div class=\"winner-banner\">🏆 ¡EQUIPO LOCAL GANADOR DEL PARTIDO! 🏆</div>";
    } else if (setsVisitante >= metaSets) {
      ptr += "<div class=\"winner-banner\">🏆 ¡EQUIPO VISITANTE GANADOR DEL PARTIDO! 🏆</div>";
    }

    // Tarjeta Local
    ptr += "<div class=\"card\">";
    ptr += "<h2>LOCAL</h2>";
    ptr += "<div class=\"score-num\">Puntos: " + String(puntosLocal) + "</div>";
    ptr += "<p>Sets Ganados: <b style=\"color:#fbbf24; font-size:1.3rem;\">" + String(setsLocal) + "</b></p>";
    if (setsLocal < metaSets && setsVisitante < metaSets) {
      ptr += "<div class=\"btn-group\">";
      ptr += "<a class=\"btn btn-minus\" href=\"/decL\">Restar</a>";
      ptr += "<a class=\"btn btn-plus\" href=\"/incL\">Sumar</a>";
      ptr += "</div>";
    }
    ptr += "</div>";

    // Tarjeta Visitante
    ptr += "<div class=\"card\">";
    ptr += "<h2>VISITANTE</h2>";
    ptr += "<div class=\"score-num\">Puntos: " + String(puntosVisitante) + "</div>";
    ptr += "<p>Sets Ganados: <b style=\"color:#fbbf24; font-size:1.3rem;\">" + String(setsVisitante) + "</b></p>";
    if (setsLocal < metaSets && setsVisitante < metaSets) {
      ptr += "<div class=\"btn-group\">";
      ptr += "<a class=\"btn btn-minus\" href=\"/decV\">Restar</a>";
      ptr += "<a class=\"btn btn-plus\" href=\"/incV\">Sumar</a>";
      ptr += "</div>";
    }
    ptr += "</div>";
  } 
  else if (modoOperacion == 2) { 
    // Contadores Libres independientes (izq y der de 0 a 99)
    ptr += "<div class=\"card\">";
    ptr += "<h2>CONTADOR LIBRE - IZQUIERDA</h2>";
    ptr += "<div class=\"score-num\">" + String(puntosLibreIzq) + "</div>";
    ptr += "<div class=\"btn-group\">";
    ptr += "<a class=\"btn btn-minus\" href=\"/decLibreIzq\">Restar</a>";
    ptr += "<a class=\"btn btn-plus\" href=\"/incLibreIzq\">Sumar</a>";
    ptr += "</div>";
    ptr += "</div>";

    ptr += "<div class=\"card\">";
    ptr += "<h2>CONTADOR LIBRE - DERECHA</h2>";
    ptr += "<div class=\"score-num\">" + String(puntosLibreDer) + "</div>";
    ptr += "<div class=\"btn-group\">";
    ptr += "<a class=\"btn btn-minus\" href=\"/decLibreDer\">Restar</a>";
    ptr += "<a class=\"btn btn-plus\" href=\"/incLibreDer\">Sumar</a>";
    ptr += "</div>";
    ptr += "</div>";
  } 
  else if (modoOperacion == 1) { 
    ptr += "<div class=\"card\" style=\"padding: 30px 15px;\">";
    ptr += "<h2>MODO RELOJ ACTIVO ⏰</h2>";
    ptr += "<p style=\"font-size:1.1rem; margin-top:15px;\">Mostrando hora actual sincronizada desde el celular (HH:MM).</p>";
    ptr += "</div>";
  } 
  else { 
    ptr += "<div class=\"card\" style=\"padding: 30px 15px;\">";
    ptr += "<h2>MODO LAURELES 🏆</h2>";
    ptr += "<p style=\"font-size:1.1rem; margin-top:15px;\">Mostrando la palabra LaUrElES con el diseño tipográfico personalizado.</p>";
    ptr += "</div>";
  }

  ptr += "<div style=\"max-width: 420px; margin: 0 auto;\">";
  ptr += "<a class=\"btn btn-reset\" href=\"/reset\">🔄 Reiniciar Tablero / Partidos</a>";
  ptr += "</div>";

  ptr += "</body></html>";
  return ptr;
}