# termoFan — avanzamento firmware

Ultimo aggiornamento: 21 novembre 2026. Aggiornare questo file su richiesta. Per compilazione e configurazione consultare il [README](README.md).

## Stato delle fasi

| Fase | Obiettivo | Stato |
| --- | --- | --- |
| 1 | Progetto PlatformIO e verifica della compilazione | Completata per NodeMCU 1.0 (ESP-12E), framework Arduino in VS Code. |
| 2 | Wi-Fi e connessione al broker MQTT | Configurazione dal portale e connessione Wi-Fi/DHCP provate sulla scheda. Mosquitto ha confermato autenticazione e connessione. |
| 3 | Riconoscimento del dispositivo in Home Assistant | MQTT Discovery verificato in Home Assistant. Visibili il dispositivo, l’IP e il segnale Wi-Fi. |
| 4 | Tre ventole PC PWM a quattro fili | Conteggio tachimetrico su tre canali implementato e compilato, da verificare al banco. PWM, PID e monitoraggio da Home Assistant da implementare. |
| 5 | Tre sensori di temperatura DS18B20 | Da integrare su bus OneWire comune. Resta da definire come combinare le temperature per regolare le ventole. |

## Decisioni e comportamento del firmware

- **Build e diagnostica.** Ambiente PlatformIO `nodemcuv2`, piattaforma `espressif8266@4.2.1`, monitor seriale a 115200 baud. Il logger supporta le stampe in stile `printf` con soglia modificabile a runtime. I livelli sono `NONE`, `ERROR`, `INFO` e `DEBUG`. La soglia iniziale è `INFO`.
- **Primo avvio.** La scheda crea la rete `termoFan-…` e stampa la password temporanea dell’access point sulla seriale. Il portale captive è raggiungibile anche da `http://10.77.0.1/`. Consente di selezionare una rete rilevata o digitare un SSID nascosto.
- **Cambio rete.** Il firmware prova le credenziali per un massimo di 30 secondi e le salva dopo aver ottenuto un IP. Se la prova fallisce, conserva la configurazione precedente. Dopo il successo spegne l’access point e rende disponibile il portale all’indirizzo LAN indicato sulla seriale.
- **Amministrazione.** Il portale richiede autenticazione HTTP Digest. L’unico utente è `admin`, con password iniziale `default` modificabile dal portale. Il trasporto HTTP è privo di TLS.
- **Recupero e LED.** Tenere premuto FLASH (GPIO0) per 5 secondi a scheda già avviata riapre il portale. Il firmware usa il LED attivo basso su GPIO2: lampeggio rapido in modalità captive, breve impulso periodico durante la riconnessione, lampo di circa 50 ms ogni 20 s a connessione stabilita.
- **Credenziali su flash.** SSID e password Wi-Fi e amministrativa sono protetti in LittleFS con ChaCha20-Poly1305. La persistenza Wi-Fi dell’SDK è disabilitata. La chiave di build è generata localmente in `.termofan/ConfigKey.h` ed esclusa da Git. Va conservata per rileggere la configurazione dopo gli aggiornamenti. Chi dispone del binario può estrarre la chiave.

## Verifiche e punto di ripresa

- Build Discovery compilata per NodeMCU. Il portale configura anche host, porta, utente e password MQTT, salvati con ChaCha20-Poly1305. Il client riprova la connessione e pubblica disponibilità e diagnostica.
- Il broker ha registrato il client `termoFan-594de7` su porta 1883. L’utente ha verificato IP e segnale Wi-Fi nel dispositivo MQTT di Home Assistant. Il firmware aggiorna le misure ogni 60 s.
- Restano da verificare sul banco il recupero con FLASH, il ripristino dopo credenziali Wi-Fi errate e il cambio password amministrativa.

## Fase 4 — collegamenti proposti

Le ventole previste sono tre **ARCTIC P8 Slim PWM PST** (ACFAN00295A), ciascuna da 12 V DC e 0,095 A nominali. L’assorbimento nominale complessivo è 0,285 A. ARCTIC dichiara 300–3000 RPM e arresto sotto il 5% PWM. A comando di arresto, zero giri è quindi uno stato previsto per il futuro monitoraggio. [Scheda tecnica ARCTIC e disegno del connettore](https://www.arctic.de/media/c7/68/77/1717150129/Spec_Sheet_P8_Slim_PWM_PST_EN.pdf)

Usare il connettore principale di ciascuna ventola e lasciare libere le prese di rilancio PST per pilotare i tre motori in modo indipendente. Individuare i pin dalla chiave del connettore nel disegno ARCTIC.

| Pin ventola | Ventola 1 | Ventola 2 | Ventola 3 |
| --- | --- | --- | --- |
| 1 · GND | Massa comune con NodeMCU | Stessa massa | Stessa massa |
| 2 · Alimentazione | +12 V esterni | Stesso alimentatore | Stesso alimentatore |
| 3 · TACH | D5 / GPIO14 | D6 / GPIO12 | D7 / GPIO13 |
| 4 · PWM | D1 / GPIO5 tramite NPN | D2 / GPIO4 tramite NPN | D0 / GPIO16 tramite NPN |

- **Schema da correggere.** Nell'immagine esaminata R8 e R9 sono entrambe collegate a Tach2, mentre Tach1 non ha pull-up. Collegare R9 a Tach1, mantenere R8 su Tach2 e R7 su Tach3. Ogni TACH avrà il proprio pull-up da 4,7 kΩ a 3,3 V.
- **PWM.** Lo schema usa tre 2SC1815 con resistenze da 2,2 kΩ verso le basi e da 10 kΩ tra ciascun GPIO e massa, sul lato GPIO della resistenza da 2,2 kΩ. Emettitori a massa e collettori ai rispettivi pin PWM. Il comando è invertito. La regolazione a 25 kHz è prevista e richiede una prova con Wi-Fi attivo.
- **Firmware attuale.** Le tre uscite sono inizializzate basse e rilasciano le linee PWM. Le ventole possono quindi girare al massimo. Gli ingressi TACH contano i fronti di discesa e il firmware stampa gli RPM sulla seriale ogni 5 secondi. Il calcolo assume due impulsi per giro, da verificare. PWM variabile, PID e pubblicazione degli RPM su Home Assistant restano da implementare.
- **TACH.** L'uscita a collettore aperto è un'ipotesi di lavoro. Verificare al banco il circuito, il livello alto e gli impulsi per giro prima di collegare i GPIO.
- **Sonde.** RX / GPIO3 è riservato al futuro bus OneWire comune dei tre DS18B20. La ricezione seriale sarà occupata e la trasmissione dei log resta disponibile. Il bus non è ancora implementato.

La compilazione del firmware a tre canali è riuscita. Il circuito e i valori di RPM restano da verificare sulla NodeMCU con modulo ESP-12F. Provare una ventola per volta con alimentazione a 12 V e massa comune, PWM rilasciato e TACH inizialmente scollegato dal GPIO. Verificare il segnale, poi collaudare la misura dei giri. Verificare sperimentalmente PWM3 su D0 durante l'avvio e, dopo l'integrazione delle sonde, l'upload e la comunicazione OneWire su RX con i sensori collegati. Il successivo test a duty fisso richiede l'implementazione della generazione PWM.
