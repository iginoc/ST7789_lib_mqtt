# ST7789 MQTT Message Display

![Project Image](https://i.imgur.com/your-image-url.png) <!-- Sostituisci con un URL di un'immagine del tuo progetto -->

Questo progetto per Arduino utilizza un display IPS ST7789 per visualizzare messaggi ricevuti da un broker MQTT. Include anche un orologio sincronizzato via NTP e un'interfaccia web per la configurazione del dispositivo.

È stato progettato per essere eseguito su una scheda Arduino con uno shield Ethernet W5100, condividendo il bus SPI con il display.

<table width="100%">
<tr>
<td valign="top" width="50%">

### 🚀 Funzionalità

<ul>
    <li><b>Display MQTT</b>: Visualizza i messaggi ricevuti su un topic MQTT specifico.</li>
    <li><b>Orologio e Data</b>: Mostra l'ora e la data correnti, sincronizzate tramite un server NTP. L'ora include un indicatore dei secondi lampeggiante.</li>
    <li><b>Interfaccia Web</b>:
        <ul>
            <li>Configura l'indirizzo del server MQTT, utente, password e topic.</li>
            <li>Pulsante per accendere/spegnere il display.</li>
            <li>Pulsante per il riavvio software del dispositivo.</li>
        </ul>
    </li>
    <li><b>Salvataggio in EEPROM</b>: La configurazione MQTT viene salvata permanentemente nella memoria EEPROM.</li>
    <li><b>Risparmio Energetico</b>: Lo schermo si spegne automaticamente dopo 5 minuti di inattività e si riaccende alla ricezione di un nuovo messaggio.</li>
    <li><b>Connessione Stabile</b>: Utilizza un indirizzo IP statico per la connessione Ethernet per risparmiare memoria e garantire stabilità.</li>
</ul>
</td>
<td valign="top" width="50%">

### 🛠️ Hardware Necessario
<ul>
    <li>Scheda Arduino (es. Arduino UNO, Diecimila/Duemilanove con ATmega328).</li>
    <li>Shield Ethernet basato su W5100.</li>
    <li>Display IPS ST7789 (il codice è testato con un 240x240, ma supporta anche altre risoluzioni).</li>
    <li>Alimentatore (consigliato 3.3V per il display).</li>
</ul>
</td>
</tr>
</table>

## 🔌 Cablaggio

Il display e lo shield Ethernet condividono il bus SPI (pin D11, D13). È **essenziale** utilizzare il pin `CS` del display per evitare conflitti.

Ecco la configurazione dei pin utilizzata nel file `src/main.cpp`:

| Pin Display | Nome Pin | Pin Arduino | Note                               |
| :---------- | :------- | :---------- | :--------------------------------- |
| GND         | GND      | GND         | Massa comune                       |
| VCC         | VCC      | 3.3V        | **Usare solo 3.3V!**               |
| SCL         | SCK      | D13         | SPI Clock                          |
| SDA         | MOSI     | D11         | SPI Data                           |
| RES         | RESET    | D8          | Reset del display                  |
| DC          | D/C      | D7          | Data/Command                       |
| CS          | CS       | D9          | Chip Select (necessario per SPI)   |
| BLK         | Backlight| VCC (3.3V)  | Retroilluminazione (sempre accesa) |

**Note Importanti:**
- Lo shield Ethernet utilizza il pin **D10** per il suo Chip Select (CS).
- La scheda SD (se presente sullo shield) utilizza il pin **D4** per il CS e viene disabilitata nel codice.

## 📦 Software e Librerie

Il progetto è configurato per essere compilato con PlatformIO. Le dipendenze vengono gestite automaticamente.

Le librerie utilizzate sono:
```ini
lib_deps =
    adafruit/Adafruit GFX Library
    https://github.com/cbm80amiga/ST7789_AVR
    arduino-libraries/Ethernet
    arduino-libraries/NTPClient
    PaulStoffregen/Time
    knolleary/PubSubClient
```

## ⚙️ Installazione e Configurazione

1.  **Clona il Repository**
    ```bash
    git clone https://github.com/tuo-username/tuo-repository.git
    ```

2.  **Apri con PlatformIO**
    Apri la cartella del progetto con Visual Studio Code e l'estensione PlatformIO.

3.  **Configura l'IP Statico (Opzionale)**
    Se necessario, modifica l'indirizzo IP statico nel file `src/main.cpp`:
    ```cpp
    IPAddress ip(10, 42, 0, 242);
    IPAddress dns(192, 168, 188, 41);
    IPAddress gateway(10, 42, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    Ethernet.begin(mac, ip, dns, gateway, subnet);
    ```

4.  **Compila e Carica**
    Usa i comandi di PlatformIO per compilare e caricare il firmware sulla tua scheda Arduino.

## 🌐 Utilizzo

1.  **Primo Avvio**
    Al primo avvio, il display mostrerà "Avvio Sistema..." seguito dall'indirizzo IP assegnato.

2.  **Configurazione MQTT**
    - Apri un browser web e naviga fino all'indirizzo IP del dispositivo.
    - Verrà visualizzata una pagina di configurazione.
    - Inserisci i dettagli del tuo broker MQTT:
        - **S:** Indirizzo del server
        - **U:** Nome utente
        - **P:** Password
        - **T:** Topic a cui sottoscriversi
    - Clicca su "Salva". La configurazione verrà memorizzata in EEPROM e utilizzata anche dopo un riavvio.

3.  **Visualizzazione dei Messaggi**
    Pubblica un messaggio sul topic MQTT configurato. Il messaggio apparirà sul display.

    **Esempio con `mosquitto_pub`:**
    ```bash
    mosquitto_pub -h TUO_SERVER_MQTT -u TUO_UTENTE -P TUA_PASSWORD -t "home/msg" -m "Ciao Mondo!"
    ```

4.  **Controlli Web**
    - **SCHERMO ON/OFF**: Accende o spegne il display manualmente.
    - **RESET**: Riavvia il dispositivo.

## 🙏 Crediti

- Demo originale "Amiga Boing Ball" da cui il codice base del display ha preso ispirazione: (c) 2019-24 Pawel A. Hernik.
- Librerie utilizzate: Adafruit, PubSubClient, Paul Stoffregen e altri contributori della comunità Arduino.

---
*Questo README è stato generato da Gemini Code Assist.*