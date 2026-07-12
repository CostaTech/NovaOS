# Analisi NovaOS

**Data analisi:** 12 luglio 2026  
**Percorso:** `/home/costatechdev/Scrivania/NovaOS`  
**Repository:** https://github.com/CostaTech/NovaOS.git  
**Autore:** CostaTech

## Verifica build

Analisi validata con build pulita:

```bash
make clean && make iso   # OK — nessun errore
```

| Artefatto | Dimensione | Note |
|-----------|------------|------|
| `build/NovaOS.kernel` | 75 KB | Kernel Multiboot i386 |
| `NovaOS.iso` | 22 MB | ISO ibrida GRUB |
| Sorgenti (LOC) | ~3 464 righe | 38 file fuori da `.git/` e `build/` |

---

## Cos'è NovaOS

NovaOS è un **OS live bootabile** costruito da zero, pensato come sistema grafico/semi-grafico stabile su **QEMU, VirtualBox e PC reale**. Il terminale è un'app separata dal desktop.

Filosofia chiave (da `NOVAOS_ROADMAP.md`):

- Stabilità prima delle feature
- Test obbligatorio su hardware reale
- Driver tastiera e mouse **separati**
- Percorso verso **self-hosting** tramite linguaggio NovaC

---

## Architettura software

```
GRUB Multiboot
    └── boot/entry.asm (stack, passa magic + mbi a C++)
            └── kernel/main.cpp
                    ├── interrupts, framebuffer*, keyboard, mouse, storage
                    ├── ramfs_init(), novac_init()
                    ├── login_screen()
                    └── desktop_loop()
                            ├── graphics/desktop.cpp
                            ├── shell/shell.cpp
                            └── languages/novac/novac.cpp

* framebuffer.c è placeholder — non attivo
```

### Ruolo dei linguaggi

| Layer | Linguaggio | Responsabilità |
|-------|------------|----------------|
| Boot | Assembly (NASM) | Entry Multiboot, stack, passaggio a C++ |
| Driver | C | VGA, PS/2, ATA storage, porte I/O |
| Sistema | C++ (freestanding) | Desktop, shell, RAMFS, NovaC |
| App | NovaC (`.nc`) | Script e mini-app interpretate |
| Tool host | Python, Go, Rust, Ruby | LNP, doctor, scaffolding NovaC |

Header centrale: `include/nova.h` — API condivisa tra C e C++.

---

## Flusso di avvio

1. GRUB carica il kernel (`boot/grub.cfg` → `NovaOS.kernel`)
2. `boot/entry.asm` imposta lo stack e chiama `kernel_main(magic, mbi_addr)`
3. `kernel/main.cpp` inizializza: interrupt → framebuffer (placeholder) → tastiera → mouse → storage → RAMFS → NovaC
4. Schermata login (username, default `guest`)
5. Desktop loop VGA testuale 80×25

---

## Stato attuale vs roadmap

### Implementato

| Area | Stato | Dettaglio |
|------|-------|-----------|
| Boot Multiboot + ISO | Fatto | `make iso` / `make run` |
| Kernel panic | Fatto | `kernel/panic.cpp` |
| Desktop VGA | Fatto | 4 temi, dock 10 app, pannelli UI |
| Login base | Parziale | Solo username, no password |
| Terminale separato | Fatto | `shell/shell.cpp` (~700 righe) |
| RAM filesystem | Fatto | 64 slot, cartelle standard |
| File protetti | Fatto | App `.nc` ufficiali non cancellabili |
| Driver tastiera | Fatto | Ignora byte AUX/mouse (`status & 0x20`) |
| Driver mouse PS/2 | Fatto | Polling, click sul dock |
| Storage ATA | Parziale | NovaFS su LBA fisso |
| NovaC interpreter | Fatto | var, input, print, if/while/for speciali |
| Power | Parziale | Reboot 8042, shutdown porte QEMU |
| Reboot/shutdown UI | Fatto | Menu desktop + comandi shell |

### Non implementato

| Area | Stato |
|------|-------|
| Framebuffer grafico | Placeholder in `drivers/framebuffer.c` |
| Formato `.lnp` in OS | Solo metadata in RAMFS |
| Initrd nella ISO | File demo solo in RAM all'avvio |
| Login con password persistente | Fase 10 |
| Finestre grafiche, font bitmap | Fase 3 |
| Compilatore NovaC → Assembly | Fase 6–8 |
| Self-hosting | Fase 8 |
| Cartella `releases/` | Non presente |

---

## Componenti principali

### Desktop (`graphics/desktop.cpp`)

- UI VGA testuale, 4 temi: Blue Galaxy, Nebula Green, Violet Orbit, Deep Space
- Launch Dock: Files, Terminal, Settings, Calc, Notes, Paint, Docs, Tour, Hello, Games
- Navigazione tastiera (frecce/W-S) e mouse (release click)
- Power menu con conferma Y/N

### Shell (`shell/shell.cpp`)

- **Core:** `help`, `about`, `storage`, `clear`, `desktop`, `again`
- **Filesystem:** `pwd`, `ls`, `tree`, `cd`, `mkdir`, `create`, `cat`, `edit`, `rename`, `rm`, `formatfs`, `savefs`, `loadfs`, `lnpinfo`
- **NovaC:** `apps`, `runNC`, `newnc`, `novac`
- **Power:** `reboot`, `shutdown`

### RAMFS (`fs/ramfs.cpp`)

- Albero in memoria: max 64 entry, file max 1024 byte
- ~15 app NovaC in `/apps`, game `nova_quest.nc` in `/games`
- Persistenza opzionale: `savefs`/`loadfs` → LBA 32768

### NovaC (`languages/novac/novac.cpp`)

Interprete line-based con sintassi custom:

- `var x = 5`
- `int << func >>(msg)` — output
- `input(name)` — input
- If/while/for con sintassi speciale documentata in `/docs/novac.txt`

### Driver input

- **Tastiera:** scancode set 1, filtro byte mouse dal 8042
- **Mouse:** PS/2 polling, cursore `>`, posizione 1–78 × 1–23

### Storage (`drivers/storage.c`)

- Bus ATA primario, area NovaFS 144 settori (~72 KB)
- Modalità safe, `formatfs` con conferma

---

## Punti di forza

1. Architettura chiara — Assembly / C driver / C++ userland
2. Roadmap dettagliata con criteri di stabilità
3. Desktop usabile con app, temi, mouse e tastiera
4. RAMFS ricco con file demo e app protette
5. NovaC funzionante (calculator, adventure game)
6. Commit recenti su stabilità hardware reale
7. Documentazione: roadmap, LNP, colori VGA

---

## Limiti e debito tecnico

1. Nessuna stdlib — tutto custom
2. Framebuffer disabilitato — solo VGA testuale
3. RAMFS limitato — 64 entry, 1 KB/file, no initrd in ISO
4. Login incompleto — no password, no persistenza
5. NovaC solo interprete — no compilazione
6. Storage fragile — LBA fisso
7. README duplicato (righe 36–37)
8. `NOVA STUDENT AREA` citata nel README ma assente nel codice
9. Single-threaded — nessun scheduler
10. Nessuna memoria dinamica / paging

---

## Valutazione

NovaOS è nella **Fase 1 completata**, con elementi delle **Fasi 2, 4 e 6** parzialmente avanzati. È un OS demo funzionante ben oltre un tipico kernel didattico.

### Prossimi passi (roadmap)

1. Framebuffer + renderer pixel (Fase 3)
2. Initrd reale nella ISO (Fase 4)
3. Storage persistente sicuro su PC reale (Fase 4.5)
4. Login/account (Fase 10)

---

## Struttura directory

```text
NovaOS/
├── boot/           entry.asm, linker.ld, grub.cfg
├── kernel/         main, panic, power, interrupts
├── drivers/        vga, keyboard, mouse, framebuffer, storage, ports
├── graphics/       desktop.cpp
├── shell/          shell.cpp
├── fs/             ramfs.cpp
├── languages/novac/novac.cpp
├── include/        nova.h
├── docs/           documentazione progetto
├── tools/          lnp, doctor, scaffolding
├── iso/            struttura ISO GRUB
├── assets/         asset grafici (host)
├── Makefile
├── NOVAOS_ROADMAP.md
└── NovaOS.iso
```
