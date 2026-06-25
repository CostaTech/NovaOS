# Roadmap NovaOS

Questa roadmap e' per noi, per guidare lo sviluppo di NovaOS senza ripetere il caos dei vecchi OS.

Questo file non fa parte dell'OS avviato. E' solo un documento del progetto.

## Visione

NovaOS deve diventare un sistema operativo live, grafico, ordinato e stabile.

Obiettivi principali:

- avvio da ISO e USB
- funzionamento in QEMU
- funzionamento in VirtualBox
- funzionamento obbligatorio su PC reale
- desktop grafico/semi-grafico
- terminale separato dal desktop
- filesystem con file e app predefinite
- kernel in C/C++ con boot in Assembly
- supporto futuro a NovaC
- percorso verso self-hosting

NovaOS non deve essere solo un terminale. Il terminale e' un'app, non tutto il sistema.

## Regole fondamentali

- Deve funzionare anche su PC reale.
- Ogni feature importante va testata su QEMU, VirtualBox e PC reale.
- Desktop e Terminale devono essere separati.
- Tastiera e mouse devono avere driver separati.
- Il mouse non deve mai sporcare l'input della tastiera.
- Niente tema hacking.
- Niente modifiche enormi tutte insieme.
- Se una build funziona su PC reale, va salvata subito.
- Non sovrascrivere mai una ISO stabile.
- Prima stabilita', poi feature.

## Linguaggi

### Assembly

Assembly serve per:

- entry point del kernel
- boot iniziale
- parti bassissimo livello
- codice dove C/C++ non bastano

Assembly deve rimanere piccolo e isolato.

### C

C serve per:

- driver hardware
- porte I/O
- tastiera
- mouse
- framebuffer
- interrupt
- timer
- primitive memoria
- funzioni basse del kernel

C e' il livello vicino all'hardware.

### C++

C++ serve per:

- desktop
- finestre
- app
- filesystem
- terminale
- settings
- file manager
- API interne dell'OS

C++ serve per organizzare bene il sistema.

### NovaC

NovaC verra' integrato dopo che il kernel e' stabile.

Servira' per:

- script
- piccole app
- esperimenti di compilazione
- generazione Assembly
- strumenti interni a NovaOS
- percorso verso self-hosting

All'inizio NovaC non deve controllare il kernel. Prima deve essere un'app/strumento stabile.

## Obiettivo hardware reale

NovaOS deve necessariamente funzionare su PC reale.

Questo significa:

- boot da chiavetta USB
- niente caratteri casuali
- niente input corrotto
- tastiera stabile
- mouse solo quando il driver e' davvero sicuro
- fallback tastiera-only sempre disponibile
- niente dipendenze da file `.vdi`
- file ufficiali inclusi nella ISO o initrd

Se funziona solo in QEMU ma non su PC reale, non e' considerato stabile.

## Fase 1: Base stabile

Obiettivo: NovaOS si avvia e mostra il desktop.

Da fare:

- boot con GRUB Multiboot
- entry Assembly minimale
- kernel C/C++
- Kernel Panic per errori veri
- desktop iniziale
- terminale separato apribile con `T`
- app base: Files, Settings, About
- testi sempre dentro lo schermo
- niente caratteri speciali rischiosi

Stabile quando:

- parte in QEMU
- parte in VirtualBox
- parte su PC reale da USB
- non mostra caratteri strani

## Fase 2: Input sicuro

Obiettivo: tastiera e mouse sicuri.

Da fare:

- driver tastiera separato
- driver mouse separato
- event queue per input
- desktop usa eventi mouse
- terminale usa eventi tastiera
- fallback senza mouse

Regola importantissima:

Il driver tastiera non deve mai leggere pacchetti mouse.

Stabile quando:

- si scrive nel terminale senza caratteri casuali
- il mouse non rompe la tastiera
- il sistema funziona su PC reale

## Fase 3: Grafica

Obiettivo: passare da VGA testuale a grafica vera.

Da fare:

- framebuffer da GRUB
- renderer pixel
- font bitmap
- finestre
- cursore mouse disegnato
- icone
- desktop piu' bello
- supporto immagini Nova Picture con estensione `.lnp`
- viewer immagini `.lnp` dentro il desktop
- comando shell `view image.lnp` per aprire immagini
- app desktop Images/Gallery per vedere immagini salvate in `/images`
- libreria futura NovaC OS per mostrare immagini: `image_open()` e `image_draw()`

Stabile quando:

- il desktop si vede bene in QEMU
- si vede bene in VirtualBox
- si vede bene su PC reale

## Fase 4: Filesystem

Obiettivo: NovaOS deve avere file e app predefinite senza dipendere da VDI.

Da fare:

- RAM filesystem
- initrd o archivio dentro la ISO
- cartelle base:
  - `/system`
  - `/apps`
  - `/home`
  - `/docs`
- comando `ls`
- comando `cat`
- app Files collegata al filesystem
- editor testo base
- file demo e documentazione per formato immagini `.lnp`

Stabile quando:

- Files mostra file veri
- Terminale puo' leggere file
- i file ufficiali ci sono anche su PC reale live USB

## Fase 4.5: Storage persistente sicuro

Obiettivo: salvare dati senza rischiare di rovinare dischi reali.

Ordine obbligatorio:

- rilevare bus/storage in modalita' sicura
- comando `storage` per diagnostica
- nessuna scrittura finche' non esiste un target sicuro
- leggere settori in modo controllato
- scrivere solo su disco dati NovaOS o immagine dedicata
- salvare `/home` e impostazioni utente
- login con username/password persistenti

Stabile quando:

- QEMU salva e ricarica `/home`
- VirtualBox salva e ricarica `/home`
- PC reale non viene mai scritto se non confermato e riconosciuto

## Fase 5: Desktop e app

Obiettivo: NovaOS deve sembrare un OS, non un terminale colorato.

App base:

- Files
- Terminal
- Settings
- About
- Text Editor
- System Info
- App Launcher
- Images/Gallery

## Fase 5.5: Glow up desktop

Obiettivo: rendere il desktop bello e utile senza perdere stabilita'.

Da fare:

- mostrare utente loggato
- mostrare stato storage
- icone piu' ordinate
- finestre migliori
- app cliccabili con mouse reale
- status bar piu' chiara
- tema Nova/galassia piu' riconoscibile
- NovaC Runner
- anteprime immagini `.lnp` nel file manager

Desktop:

- app separate
- barra inferiore o superiore
- navigazione da tastiera
- mouse in futuro
- finestre in futuro

Stabile quando:

- l'utente puo' usare NovaOS dal desktop
- Terminal resta un'app separata

## Fase 6: NovaC

Obiettivo: integrare NovaC in modo ordinato.

Da fare:

- cartella `languages/novac`
- runner NovaC
- esempi `.nc`
- errori leggibili
- compilazione verso Assembly o bytecode
- app NovaC Runner

Stabile quando:

- NovaOS esegue un piccolo script NovaC
- NovaC non rompe il kernel

## Fase 7: Strumenti di sviluppo dentro NovaOS

Obiettivo: iniziare la strada self-hosting.

Da fare:

- editor testo
- file manager
- terminale
- runner NovaC
- viewer sorgenti
- log build
- progetto/app manager

Piu' avanti:

- assembler dentro NovaOS
- linker semplice
- compilatore NovaC dentro NovaOS
- build di app NovaOS dall'interno di NovaOS

## Fase 8: Self-hosting

Self-hosting significa che NovaOS puo' costruire parti di se stesso dall'interno.

Percorso corretto:

1. NovaOS esegue script NovaC.
2. NovaOS modifica file NovaC.
3. NovaOS compila NovaC.
4. NovaOS costruisce app interne.
5. NovaOS costruisce strumenti userland.
6. Solo molto dopo NovaOS costruisce parti del kernel.

Regola:

Self-hosting non parte dal kernel. Parte dalle app e dagli strumenti.

## Fase 9: Stabilita' e release

Ogni volta che NovaOS funziona su PC reale:

- copiare la ISO in `releases/`
- salvare hash SHA256
- scrivere cosa e' stato testato
- fare commit Git
- non sovrascrivere la ISO stabile

Struttura consigliata:

```text
releases/
  NovaOS-working-YYYY-MM-DD.iso
  NovaOS-working-YYYY-MM-DD.sha256
  NovaOS-working-YYYY-MM-DD-notes.txt
```


## Fase 10: Registrazione e login

Obiettivo: NovaOS deve avere un sistema account semplice ma stabile.

Funzioni da aggiungere:

- schermata di prima registrazione
- scelta username
- scelta password
- login all'avvio
- cambio password
- cambio username
- eventuale password hint
- logout futuro

Regole importanti:

- Il login deve funzionare anche su PC reale.
- Niente caratteri casuali nel nome utente.
- Username e password devono accettare solo caratteri sicuri all'inizio.
- Non salvare dati in modo fragile o casuale.
- Prima usare RAM/config temporanea.
- Poi salvare su filesystem/initrd/disk solo quando lo storage e' stabile.
- Se la config e' corrotta, NovaOS deve mostrare errore chiaro o tornare alla registrazione.

Percorso consigliato:

1. Login temporaneo in RAM.
2. Registrazione iniziale senza salvataggio su disco.
3. Validazione username/password.
4. Salvataggio nel filesystem NovaOS quando il filesystem e' stabile.
5. Schermata login piu' grafica dentro il desktop/boot flow.

Stabile quando:

- puoi creare un account
- puoi accedere con quell'account
- su PC reale non appaiono caratteri strani
- se sbagli password non si rompe niente


## Fase 11: Reboot e shutdown

Obiettivo: NovaOS deve poter riavviare e spegnere il sistema in modo controllato.

Da aggiungere:

- comando `reboot` nel Terminale
- comando `shutdown` nel Terminale
- pulsante/opzione Reboot nel desktop
- pulsante/opzione Shutdown nel desktop
- conferma prima dello spegnimento
- schermata finale chiara

Implementazione tecnica futura:

- reboot tramite controller tastiera/8042 o metodo ACPI quando disponibile
- shutdown tramite ACPI quando avremo una base stabile
- fallback: mostrare messaggio "It is now safe to power off"

Regole:

- Deve funzionare in QEMU.
- Deve funzionare in VirtualBox.
- Deve essere sicuro su PC reale.
- Se lo shutdown vero non e' supportato, NovaOS deve mostrarlo chiaramente.

## Prossimi passi consigliati

1. Migliorare desktop senza complicare il kernel.
3. Creare RAM filesystem vero.
4. Collegare Files app al RAM filesystem.
5. Tenere mouse disattivato finche' non esiste un driver separato sicuro.
6. Testare presto su PC reale.
7. Salvare subito la prima build che funziona bene su PC reale.
