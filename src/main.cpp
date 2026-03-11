// ST7789 library example
// Amiga Boing Ball Demo
// (c) 2019-24 Pawel A. Hernik
// YT video: https://youtu.be/KwtkfmglT-c
// Added support for LCD height 320,280,240
// Optimized ball refresh from 61 to 39ms

/*
ST7789 240x240 1.3" IPS (without CS pin) - only 4+2 wires required:
 #01 GND -> GND
 #02 VCC -> VCC (3.3V only!)
 #03 SCL -> D13/SCK
 #04 SDA -> D11/MOSI
 #05 RES -> D9 /PA0 or any digital (HW RESET is required to properly initialize LCD without CS)
 #06 DC  -> D10/PA1 or any digital
 #07 BLK -> NC

ST7789 240x280 1.69" IPS - only 4+2 wires required:
 #01 GND -> GND
 #02 VCC -> VCC (3.3V only!)
 #03 SCL -> D13/SCK
 #04 SDA -> D11/MOSI
 #05 RES -> optional
 #06 DC  -> D10 or any digital
 #07 CS  -> D9 or any digital
 #08 BLK -> VCC

ST7789 170x320 1.9" IPS - only 4+2 wires required:
 #01 GND -> GND
 #02 VCC -> VCC (3.3V only!)
 #03 SCL -> D13/SCK
 #04 SDA -> D11/MOSI
 #05 RES -> optional
 #06 DC  -> D10 or any digital
 #07 CS  -> D9 or any digital
 #08 BLK -> VCC

ST7789 240x320 2.0" IPS - only 4+2 wires required:
 #01 GND -> GND
 #02 VCC -> VCC (3.3V only!)
 #03 SCL -> D13/SCK
 #04 SDA -> D11/MOSI
 #05 RES -> optional
 #06 DC  -> D10 or any digital
 #07 CS  -> D9 or any digital
*/

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
    msgChanged[i] = false;
  }

  // Inizializza Ethernet
  // Configurazione con IP Statico per risparmiare memoria FLASH.
  // La chiamata a Ethernet.begin(mac) che attiva il DHCP è stata rimossa.
  IPAddress ip(192,168,188,199);
  IPAddress dns(192,168,188,41); // Usa il gateway come DNS
  IPAddress gateway(192,168,188,1);
  IPAddress subnet(255, 255, 255, 0);
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  
  server.begin(); // Avvia il Web Server

  // Configura MQTT
  mqttClient.setServer(conf.mqttServer, 1883);
  mqttClient.setCallback(callback);

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
    const char* topics[] = {conf.mqttTopic, conf.mqttTopic2, conf.mqttTopic3, conf.mqttTopic4, conf.mqttTopic5};

    // Definizioni del layout
    const int16_t box_spacing = 5;
    const int16_t small_box_w = (SCR_WD - 3 * box_spacing) / 2;
    const int16_t small_box_h = 60;
    const int16_t row1_y = box_spacing;
    const int16_t row2_y = row1_y + small_box_h + box_spacing;
    const int16_t row3_y = row2_y + small_box_h + box_spacing;
    const int16_t large_box_h = SCR_HT - row3_y - box_spacing;

    // Coordinate dei box [x, y, w, h]
    const int16_t box_coords[NUM_TOPICS][4] = {
        {box_spacing, row1_y, small_box_w, small_box_h},                                 // Box 1
        {2 * box_spacing + small_box_w, row1_y, small_box_w, small_box_h},                // Box 2
        {box_spacing, row2_y, small_box_w, small_box_h},                                 // Box 3
        {2 * box_spacing + small_box_w, row2_y, small_box_w, small_box_h},                // Box 4
        {box_spacing, row3_y, SCR_WD - 2 * box_spacing, large_box_h} // Box 5
    };

    if (forceRedraw) {
      lcd.fillScreen(bgCol);
      for (int i = 0; i < NUM_TOPICS; i++) {
        if (strlen(topics[i]) > 0) {
          lcd.drawRect(box_coords[i][0], box_coords[i][1], box_coords[i][2], box_coords[i][3], YELLOW);
          msgChanged[i] = true; // Forza l'aggiornamento del contenuto
        }
      }
      forceRedraw = false;
    }

    lcd.setFont(&FreeSansBold12pt7b);
    lcd.setTextSize(1);
    lcd.setTextColor(YELLOW);

    for (int i = 0; i < NUM_TOPICS; i++) {
      if (msgChanged[i] && strlen(topics[i]) > 0) {
        int16_t box_x = box_coords[i][0];
        int16_t box_y = box_coords[i][1];
        int16_t box_w = box_coords[i][2];
        int16_t box_h = box_coords[i][3];

        // Pulisci l'area interna del riquadro
        lcd.fillRect(box_x + 1, box_y + 1, box_w - 2, box_h - 2, bgCol);

        if (i < 4) { // Sensori 1-4
          // Centra e disegna il nuovo testo (singola riga)
          int16_t x1, y1;
          uint16_t w, h;
          lcd.getTextBounds(lastMsgs[i], 0, 0, &x1, &y1, &w, &h);
          int16_t text_x = box_x + (box_w - w) / 2 - x1;
          int16_t text_y = box_y + (box_h - h) / 2 - y1;

          lcd.setCursor(text_x, text_y);
          lcd.print(lastMsgs[i]);
        } else { // Sensore 5 (con word wrap)
          char buffer[50];
          strncpy(buffer, lastMsgs[i], 49);
          buffer[49] = '\0';

          char *words[10]; // Max 10 words
          int word_count = 0;
          char *word = strtok(buffer, " ");
          while (word != NULL && word_count < 10) {
            words[word_count++] = word;
            word = strtok(NULL, " ");
          }

          char line1[50] = "";
          char line2[50] = "";
          int current_line = 1;
          int16_t x1, y1;
          uint16_t w, h;

          for (int j = 0; j < word_count; j++) {
            char temp_line[50];
            if (current_line == 1) strcpy(temp_line, line1);
            else strcpy(temp_line, line2);

            if (strlen(temp_line) > 0) strcat(temp_line, " ");
            strcat(temp_line, words[j]);

            lcd.getTextBounds(temp_line, 0, 0, &x1, &y1, &w, &h);

            if (w > (uint16_t)(box_w - 4)) { // -4 per padding
              if (current_line == 1) {
                current_line = 2;
                if (strlen(words[j]) < 50) strcpy(line2, words[j]);
              }
            } else {
              if (current_line == 1) { if (strlen(temp_line) < 50) strcpy(line1, temp_line); } 
              else { if (strlen(temp_line) < 50) strcpy(line2, temp_line); }
            }
          }

          // Disegna le righe centrate
          lcd.getTextBounds("A", 0, 0, &x1, &y1, &w, &h);
          uint16_t line_height = h;
          int16_t text_block_top;

          if (strlen(line2) > 0) text_block_top = box_y + (box_h - (2 * line_height + 4)) / 2;
          else text_block_top = box_y + (box_h - line_height) / 2;

          lcd.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
          lcd.setCursor(box_x + (box_w - w) / 2 - x1, text_block_top - y1);
          lcd.print(line1);

          if (strlen(line2) > 0) {
            lcd.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
            lcd.setCursor(box_x + (box_w - w) / 2 - x1, text_block_top - y1 + line_height + 4);
            lcd.print(line2);
          }
        }

        msgChanged[i] = false;
      }
    }
  }
}