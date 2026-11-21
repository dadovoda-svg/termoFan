# termoFan

Firmware per NodeMCU 1.0 (ESP8266) per la regolazione di tre ventole PWM in base a tre sonde DS18B20. La versione attuale si collega al Wi-Fi e a un broker MQTT e compare in Home Assistant con due entità diagnostiche, **Indirizzo IP** e **Segnale Wi-Fi**. Il prossimo collaudo riguarda il cablaggio delle ventole. Il firmware conta gli impulsi tachimetrici e stampa una stima dei giri sulla seriale ogni 5 secondi. La regolazione PWM, i PID e la termoregolazione sono fasi successive. [Avanzamento e collegamenti proposti](progress.md)

## Assegnazione dei pin

| Funzione | Pin NodeMCU |
| --- | --- |
| PWM ventole 1, 2 e 3 | D1 / GPIO5, D2 / GPIO4, D0 / GPIO16 |
| TACH ventole 1, 2 e 3 | D5 / GPIO14, D6 / GPIO12, D7 / GPIO13 |
| Bus OneWire per tre DS18B20 | RX / GPIO3, riservato per una fase successiva |

La misura dei tre TACH assume due impulsi per giro. Il firmware lascia rilasciate le linee PWM e stampa gli RPM sulla seriale, ma non regola ancora le ventole. Il bus OneWire su RX è previsto e non è ancora implementato. Le assegnazioni e il comportamento durante avvio, programmazione e funzionamento con i sensori collegati richiedono una verifica sulla scheda.

## Compilare e caricare

Aprire la cartella che contiene `platformio.ini` in VS Code con PlatformIO. Collegare la NodeMCU via USB e usare **Build**, **Upload** e **Serial Monitor**, oppure eseguire dalla stessa cartella:

```sh
pio run -e nodemcuv2
pio run -e nodemcuv2 -t upload
pio device monitor -b 115200
```

PlatformIO usa il framework Arduino e genera `.pio/build/nodemcuv2/firmware.bin`. Alla prima compilazione crea anche `.termofan/ConfigKey.h`. Conservare questa chiave privata e trasferirla se si cambia macchina di build. Una build con una chiave diversa richiede la riconfigurazione delle credenziali sulla scheda.

## Primo accesso e recupero

1. Aprire il monitor seriale a 115200 baud e riavviare la scheda. Al primo avvio collegarsi alla rete Wi-Fi `termoFan-…` usando la password temporanea stampata sulla seriale.
2. Aprire il portale captive oppure visitare `http://10.77.0.1/`. Le credenziali iniziali della pagina sono utente `admin` e password `default`. La password amministrativa si cambia dal portale.
3. Selezionare la rete domestica o digitare l’SSID, inserire la password e scegliere **Verifica e salva rete**. Quando ottiene un IP, la scheda salva la configurazione, chiude l’access point e stampa l’indirizzo LAN sulla seriale.
4. Ricollegarsi alla LAN e aprire il portale al nuovo indirizzo. Per riaprire l’access point di emergenza, tenere premuto **FLASH per 5 secondi a scheda già avviata** e leggere la nuova password Wi-Fi sulla seriale.

Il LED lampeggia rapidamente quando il portale captive è attivo. Durante la riconnessione emette brevi impulsi. Quando il Wi-Fi è collegato emette un lampo di circa 50 ms ogni 20 secondi.

## MQTT e Home Assistant

Nel portale LAN, alla voce **Broker MQTT**, inserire indirizzo, porta (predefinita `1883`) ed eventuali credenziali. Selezionare **Salva broker** e verificare che lo stato passi a **Connesso**. La scheda salva i parametri Wi-Fi, MQTT e amministrativi su LittleFS con ChaCha20-Poly1305. HTTP e MQTT viaggiano senza TLS. La chiave di cifratura è incorporata nel firmware.

Configurare l’integrazione MQTT di Home Assistant sullo stesso broker, con Discovery attivo e prefisso `homeassistant`. In **Impostazioni → Dispositivi e servizi → MQTT → Dispositivi**, cercare `termoFan <chip-id>`. La scheda comunica direttamente con il broker via Wi-Fi, anche in una rete dove Zigbee2MQTT usa lo stesso broker.

Per la diagnostica, sostituire `<id>` con `termofan_` seguito dalle sei cifre esadecimali del chip. Il client ID che compare nel log Mosquitto ha invece la forma `termoFan-<chip-id>`.

| Topic | Uso da parte del firmware |
| --- | --- |
| `homeassistant/sensor/<id>/ip/config` e `homeassistant/sensor/<id>/rssi/config` | Pubblica gli annunci Discovery con retain. |
| `termofan/<id>/state` | Pubblica IP e RSSI in dBm alla connessione e ogni 60 s. |
| `termofan/<id>/availability` | Pubblica la disponibilità. In caso di caduta della connessione il broker invia il Last Will `offline`. |
| `homeassistant/status` | Riceve `online` da Home Assistant e ripubblica annunci e stato. |
