#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include <ArduinoJson.h>
#include <vector>
#include <LittleFS.h>
#include <TimeLib.h>

String ssid = "";
String password = "";

void lerCredenciais() {
    File file = LittleFS.open("/credi.txt", "r");
    if (!file) {
        Serial.println("Falha ao abrir credi.txt");
        return;
    }

    while (file.available()) {
        String linha = file.readStringUntil('\n');
        linha.trim();  // Remove espaços e quebras de linha
        if (linha.startsWith("SSID=")) {
            ssid = linha.substring(5);
        } else if (linha.startsWith("SENHA=")) {
            password = linha.substring(6);
        }
    }
    file.close();
}

#define RELAY_PIN 5

AsyncWebServer server(80);
Ticker timer;
std::vector<String> horariosLigados;
bool estadoCafeteira = false;
unsigned long tempoInicio = 0;

// Controle do relé
void ligarCafeteira() {
    digitalWrite(RELAY_PIN, HIGH);
    estadoCafeteira = true;
    tempoInicio = millis();
}

void desligarCafeteira() {
    digitalWrite(RELAY_PIN, LOW);
    estadoCafeteira = false;
}

// Verifica horários programados
void checarHorarios() {
    String horaAtual = String(hour()) + ":" + String(minute());
    for (auto& horario : horariosLigados) {
        if (horaAtual == horario) {
            ligarCafeteira();
        }
    }

    // Desliga automaticamente após 2 horas
    if (estadoCafeteira && (millis() - tempoInicio >= 2 * 60 * 60 * 1000)) {
        desligarCafeteira();
    }
}

// Adicionar um horário
bool adicionarHorario(String horario) {
    if (horariosLigados.size() < 4) {
        horariosLigados.push_back(horario);
        return true;
    }
    return false;
}

// Remover um horário
void removerHorario(String horario) {
    horariosLigados.erase(std::remove(horariosLigados.begin(), horariosLigados.end(), horario), horariosLigados.end());
}

void setup() {
    Serial.begin(115200);

    // Inicializa o sistema de arquivos LittleFS
    if (!LittleFS.begin()) {
        Serial.println("Falha ao montar LittleFS!");
        return;
    }

    lerCredenciais();
    Serial.println("SSID: " + ssid);
    Serial.println("SENHA: " + password);

    pinMode(RELAY_PIN, OUTPUT);
    desligarCafeteira();

    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi conectado! IP: " + WiFi.localIP().toString());

    // Servir página HTML salva no LittleFS
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    // Adicionar horário
    server.on("/addTime", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("time", true)) {
            String time = request->getParam("time", true)->value();
            if (adicionarHorario(time)) {
                request->send(200, "text/plain", "Horário adicionado!");
            } else {
                request->send(400, "text/plain", "Limite de horários atingido!");
            }
        }
    });

    // Remover horário
    server.on("/removeTime", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("time", true)) {
            String time = request->getParam("time", true)->value();
            removerHorario(time);
            request->send(200, "text/plain", "Horário removido!");
        }
    });

    // Listar horários
    server.on("/listTimes", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "[";
        for (size_t i = 0; i < horariosLigados.size(); i++) {
            json += "\"" + horariosLigados[i] + "\"";
            if (i < horariosLigados.size() - 1) json += ",";
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    server.begin();

    // Timer para verificar horários a cada 1 minuto
    timer.attach(60, checarHorarios);
}

void loop() {
    // Servidor assíncrono, loop fica vazio
}
