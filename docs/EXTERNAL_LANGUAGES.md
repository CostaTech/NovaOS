# Linguaggi esterni di NovaOS

Dentro NovaOS l'obiettivo e' avere TencleLang come linguaggio principale per l'utente.

Fuori NovaOS, nel progetto sorgente, possiamo usare piu' linguaggi per creare tool utili.

## Assembly

Ruolo: boot e ingresso nel kernel.

Non deve crescere troppo. Serve per parlare con la CPU all'inizio.

## C

Ruolo: driver e parti vicine all'hardware.

Esempi:

- VGA
- tastiera
- mouse
- porte I/O
- framebuffer basso livello

## C++

Ruolo: parti alte del sistema.

Esempi:

- desktop
- terminale
- filesystem
- app interne
- organizzazione kernel alta

## Python

Ruolo: officina esterna pratica.

Esempi:

- convertire immagini `.lnp`
- generare asset
- testare la ISO
- preparare file per initrd

Tool attuale:

```bash
python3 tools/lnp_tool.py sample assets/galaxy.lnp
```

## Ruby

Ruolo: script leggibili e generatori.

Esempi:

- creare scheletri di app TencleLang
- generare documentazione
- preparare changelog

Tool attuale:

```bash
ruby tools/ruby/new_tencle_app.rb Calculator
```

## Go

Ruolo: tool esterni semplici, veloci e distribuibili come binario singolo.

Esempi:

- controllare struttura progetto
- tool CLI per build/release
- packer semplici

Tool attuale:

```bash
go run tools/go/nova_doctor.go
```

## Rust

Ruolo: tool esterni robusti e sicuri.

Esempi:

- validare file binari
- packer filesystem
- converter piu' seri

Tool attuale:

```bash
rustc tools/rust/lnp_check.rs -o /tmp/lnp_check
/tmp/lnp_check assets/galaxy.lnp
```

## Regola importante

Ruby, Go, Rust e Python non sostituiscono C/C++ nel kernel adesso.

Per NovaOS oggi:

```text
Boot    = Assembly
Driver  = C
Sistema = C++
Utente  = TencleLang dentro NovaOS
Tool    = Python/Ruby/Go/Rust fuori NovaOS
```
