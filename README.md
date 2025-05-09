# 🧵 MarketThreads — Simulatore di Supermercato Multithread  
> Progetto accademico per il corso di Sistemi Operativi e Laboratorio (SOL), A.A. 2019/2020

MarketThreads è una simulazione realistica del comportamento di un supermercato in cui clienti, casse e direttori interagiscono in un ambiente multithread, utilizzando primitive di sincronizzazione POSIX (`pthread`, `mutex`, `condition variables`) e strutture dati condivise.

## 🔧 Funzionalità principali

- Gestione concorrente di clienti che entrano, fanno acquisti e si mettono in coda.  
- Casse multiple, con logica di apertura/chiusura automatica.  
- Statistiche su tempo in coda, prodotti acquistati, cambi di cassa.  
- Log delle operazioni e report finali.  
- Completamente configurabile tramite file `config.txt`.

## 📂 Struttura del progetto

```
MarketThreads/
├── bin/                         # Eseguibili compilati
├── headers/                     # Header files (.h)
├── src/                         # Sorgenti C (.c)
├── tester.sh                    # Script di test automatico
├── analisi.sh                   # Script per analisi output
├── config.txt                   # File di configurazione
└── descrizione-progettosol.pdf  # Specifiche del progetto
```

## ⚙️ Compilazione

Richiede:  
- GCC  
- Make  
- Ambiente Linux con supporto a pthread

Per compilare il progetto, eseguire:

```
make
```

L’eseguibile verrà generato nella cartella `bin/marketThreads`.

## 🚀 Esecuzione

Per avviare la simulazione, utilizzare:

```
./bin/marketThreads config.txt
```

Assicurati che il file `config.txt` sia presente nella stessa directory o nel percorso specificato.

## 🧪 File di configurazione (`config.txt`)

| Parametro                | Descrizione                             |
|--------------------------|------------------------------------------|
| `K`                      | Numero iniziale di casse aperte         |
| `C`                      | Numero totale di clienti                |
| `SO_SCHEDULING_INTERVAL` | Intervallo controllo casse              |
| `SO_SIMULATION_TIME`     | Durata della simulazione in secondi     |

Il file `config.txt` consente di personalizzare il comportamento della simulazione secondo i parametri richiesti dal progetto.

## 🧵 Aspetti tecnici

- Thread separati per clienti, casse e direttore.  
- Sincronizzazione tramite:
  - `pthread_mutex_t`
  - `pthread_cond_t`
- Coda FIFO per i clienti.  
- Logging su file con accesso sincronizzato.  
- Script per analisi automatica dei risultati (`analisi.sh`).

## 📊 Output

Al termine della simulazione vengono generati report testuali e CSV che includono:

- Numero clienti serviti per cassa  
- Tempo medio di servizio  
- Prodotti acquistati  
- Numero di cambi cassa per cliente

Questi risultati possono essere analizzati con gli script inclusi o visualizzati direttamente.

## 📘 Documentazione

Consulta il file `descrizione-progettosol.pdf` per i requisiti originali forniti dal corso.  
Il codice è organizzato in moduli, con header dedicati per la gestione dei thread, la logica delle casse, il logging e il caricamento della configurazione.

## 📜 Licenza

Distribuito per scopi didattici.  
Puoi aggiungere una licenza MIT o altra, se desideri condividere pubblicamente il progetto.
