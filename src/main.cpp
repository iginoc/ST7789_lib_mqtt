

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Ethernet.h>
#include <EEPROM.h>
#define MQTT_MAX_PACKET_SIZE 128 // Reduce RAM usage
#include <PubSubClient.h>
#include <string.h>
#include "ST7789_AVR.h"
#include <Fonts/FreeSansBold12pt7b.h>

#define TFT_DC   7
//#define TFT_CS    9  // with CS
//#define TFT_RST  -1  // with CS
#define TFT_CS   9 // CS necessario per condividere SPI con Ethernet Shield
#define TFT_RST  8 

#define SCR_WD 240
#define SCR_HT 240
ST7789_AVR lcd = ST7789_AVR(TFT_DC, TFT_RST, TFT_CS);

#define NUM_TOPICS 5

uint16_t bgCol    = RGBto565(0, 0, 130); // Sfondo Blu Scuro

#ifndef YELLOW
#define YELLOW 0xFFE0
#endif

// Configurazione Ethernet e NTP
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Struttura per salvare la configurazione in EEPROM
struct Config {
  char mqttServer[32];
  char mqttUser[16];
  char mqttPass[16];
  char mqttTopic[32];
  char mqttTopic2[32]; // Nuovo topic
  char mqttTopic3[32]; // Nuovo topic
  char mqttTopic4[32]; // Nuovo topic
  char mqttTopic5[32]; // Nuovo topic
  byte valid; // Controllo validità dati
} conf;

EthernetServer server(80); // Web Server sulla porta 80
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

char lastMsgs[NUM_TOPICS][50]; // Buffer per i messaggi MQTT
bool msgChanged[NUM_TOPICS];   // Flag per ogni topic per aggiornamenti parziali
bool forceRedraw = true;       // Flag per forzare un ridisegno completo
bool screenOn = true; // Stato dello schermo
unsigned long lastActivityTime = 0; // Timer per l'inattività dello schermo

// Funzione per il reset software
void(* resetFunc) (void) = 0;

void setScreenState(bool on);

// Callback MQTT: eseguita quando arriva un messaggio
void callback(char* topic, byte* payload, unsigned int length) {
  unsigned int l = length;
  if (l > 49) l = 49; // Limita la lunghezza per evitare overflow

  int topicIndex = -1;
  if (strcmp(topic, conf.mqttTopic) == 0) {
    topicIndex = 0;
  } else if (strlen(conf.mqttTopic2) > 0 && strcmp(topic, conf.mqttTopic2) == 0) {
    topicIndex = 1;
  } else if (strlen(conf.mqttTopic3) > 0 && strcmp(topic, conf.mqttTopic3) == 0) {
    topicIndex = 2;
  } else if (strlen(conf.mqttTopic4) > 0 && strcmp(topic, conf.mqttTopic4) == 0) {
    topicIndex = 3;
  } else if (strlen(conf.mqttTopic5) > 0 && strcmp(topic, conf.mqttTopic5) == 0) {
    topicIndex = 4;
  }

  if (topicIndex != -1) {
    char newMsg[50];
    memcpy(newMsg, payload, l);
    newMsg[l] = 0;

    // Aggiorna solo se il messaggio è cambiato per evitare sfarfallio
    if (strcmp(lastMsgs[topicIndex], newMsg) != 0) {
      strcpy(lastMsgs[topicIndex], newMsg);
      msgChanged[topicIndex] = true;
    }
  }

  lastActivityTime = millis(); // Reset del timer di inattività
  if (!screenOn) { // Se lo schermo era spento, riaccendilo
    screenOn = true;
    setScreenState(true);
    forceRedraw = true; // Forza un ridisegno completo al risveglio
  }
}

void setScreenState(bool on) {
  // Configura SPI per ST7789 (Mode 3 è standard per questi display)
  // Usiamo beginTransaction per non interferire con la Ethernet Shield
  if (on) {
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE3));
    digitalWrite(TFT_CS, LOW);
    digitalWrite(TFT_DC, LOW); // Modalità Comando
    SPI.transfer(0x11); // SLPOUT (Sleep Out)
    digitalWrite(TFT_DC, HIGH); // Torna a modalità dati
    digitalWrite(TFT_CS, HIGH);
    SPI.endTransaction();
    
    delay(120); // Attesa necessaria dopo SLPOUT
    
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE3));
    digitalWrite(TFT_CS, LOW);
    digitalWrite(TFT_DC, LOW);
    SPI.transfer(0x29); // DISPON (Display On)
    digitalWrite(TFT_DC, HIGH); // Torna a modalità dati
  } else {
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE3));
    digitalWrite(TFT_CS, LOW);
    digitalWrite(TFT_DC, LOW);
    SPI.transfer(0x28); // DISPOFF (Display Off)
    SPI.transfer(0x10); // SLPIN (Sleep In)
    digitalWrite(TFT_DC, HIGH); // Torna a modalità dati
  }
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
}

void setup() 
{
  // Disabilita Ethernet e SD per liberare il bus SPI per il display senza CS
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH); // Disabilita SD Card
  
  // 1. Inizializza il display
  lcd.init(SCR_WD, SCR_HT);
  lcd.fillScreen(BLACK);
  lcd.setTextColor(WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(10, 10);
  lcd.println("Avvio Sistema...");

  // Carica configurazione
  EEPROM.get(0, conf);
  if (conf.valid != 0xAA) {
    strcpy(conf.mqttServer, "192.168.1.10");
    strcpy(conf.mqttUser, "admin");
    strcpy(conf.mqttPass, "password");
    strcpy(conf.mqttTopic, "home/msg");
    strcpy(conf.mqttTopic2, ""); // Inizializza a stringa vuota
    strcpy(conf.mqttTopic3, ""); // Inizializza a stringa vuota
    strcpy(conf.mqttTopic4, ""); // Inizializza a stringa vuota
    strcpy(conf.mqttTopic5, ""); // Inizializza a stringa vuota
    conf.valid = 0xAA;
    EEPROM.put(0, conf);
  }

  // Inizializza i buffer dei messaggi
  for (int i = 0; i < NUM_TOPICS; i++) {
    strcpy(lastMsgs[i], "In attesa...");
  }

  // Inizializza Ethernet
  // Configurazione con IP Statico per risparmiare memoria FLASH.
  // La chiamata a Ethernet.begin(mac) che attiva il DHCP è stata rimossa.
  lcd.println("Uso IP Statico...");
  IPAddress ip(10,42,0,242);
  IPAddress dns(192,168,188,41);
  IPAddress gateway(10,42,0,1);
  IPAddress subnet(255, 255, 255, 0);
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  
  server.begin(); // Avvia il Web Server

  // Configura MQTT
  mqttClient.setServer(conf.mqttServer, 1883);
  mqttClient.setCallback(callback);

  lcd.fillScreen(bgCol);
  lastActivityTime = millis(); // Inizializza il timer
}

unsigned long lastMqttReconnect = 0;

// Funzione helper per decodificare i caratteri URL (in-place, per risparmiare memoria)
void urlDecode(char* str) {
  char* p = str;
  char* q = str;
  while (*p) {
    if (*p == '+') {
      *q++ = ' ';
      p++;
    } else if (*p == '%') {
      p++;
      if (p[0] && p[1]) {
        char hex[3] = { p[0], p[1], 0 };
        *q++ = (char)strtol(hex, NULL, 16);
        p += 2;
      }
    } else {
      *q++ = *p++;
    }
  }
  *q = '\0';
}

void loop()
{
  // --- Gestione Web Server ---
  EthernetClient client = server.available();
  if (client) {
    char line_buf[80];
    uint8_t line_idx = 0;
    bool currentLineIsBlank = true;
    bool isPost = false;
    bool isReset = false;
    bool isToggle = false;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (line_idx < sizeof(line_buf) - 1) {
          line_buf[line_idx++] = c;
        }

        if (c == '\n' && currentLineIsBlank) {
          // Fine degli header HTTP
          if (isPost) {
            char body_buf[256];
            uint16_t body_idx = 0;
            while(client.available() && body_idx < sizeof(body_buf) - 1) {
                body_buf[body_idx++] = client.read();
            }
            body_buf[body_idx] = '\0';
            
            // Parsing del corpo con strtok
            char* pair = strtok(body_buf, "&");
            while (pair != NULL) {
                char* eq = strchr(pair, '=');
                if (eq) {
                    *eq = '\0'; // split key and value
                    char* key = pair;
                    char* val = eq + 1;
                    urlDecode(val); // decode the value in-place

                    if (strcmp(key, "s") == 0) strncpy(conf.mqttServer, val, 31);
                    else if (strcmp(key, "u") == 0) strncpy(conf.mqttUser, val, 15);
                    else if (strcmp(key, "p") == 0) strncpy(conf.mqttPass, val, 15);
                    else if (strcmp(key, "t") == 0) strncpy(conf.mqttTopic, val, 31);
                    else if (strcmp(key, "t2") == 0) strncpy(conf.mqttTopic2, val, 31);
                    else if (strcmp(key, "t3") == 0) strncpy(conf.mqttTopic3, val, 31);
                    else if (strcmp(key, "t4") == 0) strncpy(conf.mqttTopic4, val, 31);
                    else if (strcmp(key, "t5") == 0) strncpy(conf.mqttTopic5, val, 31);
                }
                pair = strtok(NULL, "&");
            }
            // Assicura terminazione
            conf.mqttServer[31] = '\0';
            conf.mqttUser[15] = '\0';
            conf.mqttPass[15] = '\0';
            conf.mqttTopic[31] = '\0';
            conf.mqttTopic2[31] = '\0';
            conf.mqttTopic3[31] = '\0';
            conf.mqttTopic4[31] = '\0';
            conf.mqttTopic5[31] = '\0';
            EEPROM.put(0, conf); // Salva in EEPROM
          }

          if (isReset) {
             client.println(F("HTTP/1.1 200 OK"));
             client.println(F("Content-Type: text/html"));
             client.println();
             client.println(F("<h1>Riavvio...</h1>"));
             client.stop();
             delay(100);
             resetFunc(); // Riavvia Arduino
          }

          if (isToggle) {
             screenOn = !screenOn;
             setScreenState(screenOn);
             lastActivityTime = millis(); // Reset del timer
             
             client.println(F("HTTP/1.1 302 Found"));
             client.println(F("Location: /"));
             client.println();
             client.stop();
             break;
          }

          // Invia la pagina HTML
          client.println(F("HTTP/1.1 200 OK"));
          client.println(F("Content-Type: text/html"));
          client.println(F("Connection: close"));
          client.println();
          // HTML compattato per risparmiare memoria Flash
          client.print(F("<html><form method=POST><h3>MQTT "));
          client.print(mqttClient.connected() ? F("<font color=green>&#9679;</font>") : F("<font color=red>&#9679;</font>"));
          client.print(F("</h3>S:<br><input name=s value='"));
          client.print(conf.mqttServer);
          client.print(F("'><br>U:<br><input name=u value='"));
          client.print(conf.mqttUser);
          client.print(F("'><br>P:<br><input type=password name=p value='"));
          client.print(conf.mqttPass);
          client.print(F("'><br>T:<br><input name=t value='"));
          client.print(conf.mqttTopic);
          client.print(F("'><br>T2:<br><input name=t2 value='"));
          client.print(conf.mqttTopic2);
          client.print(F("'><br>T3:<br><input name=t3 value='"));
          client.print(conf.mqttTopic3);
          client.print(F("'><br>T4:<br><input name=t4 value='"));
          client.print(conf.mqttTopic4);
          client.print(F("'><br>T5:<br><input name=t5 value='"));
          client.print(conf.mqttTopic5);
          client.print(F("'><br><br><input type=submit value=Salva><br><br>"));
          client.print(F("<a href=/toggle><button>Schermo</button></a><br><br>"));
          client.print(F("<a href=/reset>Reset</a></form></html>"));
          break;
        }
        if (c == '\n') {
          line_buf[line_idx] = '\0';
          if (strstr(line_buf, "POST") != NULL) isPost = true;
          if (strstr(line_buf, "GET /reset") != NULL) isReset = true;
          if (strstr(line_buf, "GET /toggle") != NULL) isToggle = true;
          currentLineIsBlank = true;
          line_idx = 0;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);
    client.stop();
  }

  // --- Gestione MQTT ---
  if (!mqttClient.connected()) {
    if (millis() - lastMqttReconnect > 5000) {
      lastMqttReconnect = millis();
      if (mqttClient.connect("ArduinoClient", conf.mqttUser, conf.mqttPass)) {
        mqttClient.subscribe(conf.mqttTopic);
        if (strlen(conf.mqttTopic2) > 0) mqttClient.subscribe(conf.mqttTopic2);
        if (strlen(conf.mqttTopic3) > 0) mqttClient.subscribe(conf.mqttTopic3);
        if (strlen(conf.mqttTopic4) > 0) mqttClient.subscribe(conf.mqttTopic4);
        if (strlen(conf.mqttTopic5) > 0) mqttClient.subscribe(conf.mqttTopic5);
      }
    }
  }
  mqttClient.loop();

  // --- Controllo Inattività Schermo ---
  if (screenOn && (millis() - lastActivityTime > 300000UL)) { // 5 minuti = 300000 ms
    screenOn = false;
    setScreenState(false);
  }

  // Se lo schermo è spento, non eseguire il codice di disegno
  if (!screenOn) {
    return;
  }

  bool anyChange = forceRedraw;
  if (!anyChange) {
    for (int i = 0; i < NUM_TOPICS; i++) {
      if (msgChanged[i]) {
        anyChange = true;
        break;
      }
    }
  }

  // Disegna solo se qualcosa è cambiato
  if (anyChange) {
    const char* topics[] = { conf.mqttTopic, conf.mqttTopic2, conf.mqttTopic3, conf.mqttTopic4, conf.mqttTopic5 };

    for (int i = 0; i < NUM_TOPICS; i++) {
      if (strlen(topics[i]) > 0) {
        // Stima lo spazio minimo e interrompi se non c'è
        // Altezza font: size*8 pixel. Spazio extra: 15 pixel.
        int required_space = (4 * 8) + 15; // valore(size 4) + spazio
        if (lcd.getCursorY() > SCR_HT - required_space) {
          break;
        }

        lcd.setCursor(10, lcd.getCursorY()); // Allinea il valore del messaggio
        lcd.setTextSize(4); // Aumentato da 3 per maggiore leggibilità
        lcd.setTextColor(YELLOW);
        lcd.println(lastMsgs[i]);
        
        // Aggiungi uno spazio maggiore e posizionati per il prossimo blocco
        lcd.setCursor(10, lcd.getCursorY() + 15); // Aumentato per distanziare
      }
    }
    msgChanged = false; // Reset del flag
  }

}