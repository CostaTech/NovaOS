/*
 * NovaOS — POKEY Audio Emulator
 * Emula il chip Atari POKEY (CO12294) via PC Speaker + PIT
 *
 * Il POKEY originale ha:
 *   - 4 canali audio semi-indipendenti
 *   - Registro frequenza (AUDF1-4): divisore N, clock 64KHz o 1.79MHz
 *   - Registro controllo (AUDC1-4): distorsione (bits 7-4) + volume (bits 3-0)
 *   - Registro AUDCTL: opzioni globali (clock 1.79MHz, canali 16-bit, filtri)
 *
 * Formula frequenza: F = clock / (2 * (AUDF + 1))
 *   clock = 63921 Hz (default) oppure 1789790 Hz (se bit AUDCTL attivo)
 *
 * Distorsioni POKEY (bits 6-4 di AUDCn):
 *   0 = 5-bit + 17-bit poly (rumore)
 *   1 = 5-bit poly
 *   2 = 5-bit + 4-bit poly
 *   3 = 5-bit poly
 *   4 = 17-bit poly
 *   5 = tono puro
 *   6 = 4-bit poly
 *   7 = tono puro
 *
 * Su x86 bare-metal emuliamo via PIT canale 2 + PC Speaker:
 *   - Canale 1 come canale principale (suona sul PC speaker)
 *   - Gli altri 3 canali sono tracciati in stato software
 *   - Funzioni per suonare melodie e sfx come il POKEY originale
 */

#include "nova.h"

// ── Porte hardware ─────────────────────────────────────────
#define PIT_CH2_DATA    0x42
#define PIT_CMD         0x43
#define PC_SPEAKER_PORT 0x61

// ── Clock POKEY ────────────────────────────────────────────
#define POKEY_CLOCK_64K    63921UL
#define POKEY_CLOCK_179M   1789790UL

// ── Registri POKEY (indirizzi originali Atari $D200-$D20F) ─
// Emulati in RAM
#define POKEY_AUDF1  0
#define POKEY_AUDC1  1
#define POKEY_AUDF2  2
#define POKEY_AUDC2  3
#define POKEY_AUDF3  4
#define POKEY_AUDC3  5
#define POKEY_AUDF4  6
#define POKEY_AUDC4  7
#define POKEY_AUDCTL 8
#define POKEY_STIMER 9
#define POKEY_SKCTL  15

// AUDCTL bits
#define AUDCTL_CH1_179M   (1 << 6)  // clock canale 1 a 1.79MHz
#define AUDCTL_CH3_179M   (1 << 5)  // clock canale 3 a 1.79MHz
#define AUDCTL_CH12_16BIT (1 << 4)  // combina canali 1+2 in 16-bit
#define AUDCTL_CH34_16BIT (1 << 3)  // combina canali 3+4 in 16-bit
#define AUDCTL_CH2_HPFLT  (1 << 2)  // filtro passa-alto ch2 su ch1
#define AUDCTL_CH4_HPFLT  (1 << 1)  // filtro passa-alto ch4 su ch3
#define AUDCTL_9BIT_POLY  (1 << 0)  // usa poly a 9-bit invece di 17

// AUDC bits
#define AUDC_VOLUME_ONLY  (1 << 4)  // ignora freq, output DC fisso
#define AUDC_DIST_MASK    0xE0       // bits 7-5: distorsione
#define AUDC_DIST_SHIFT   5
#define AUDC_VOL_MASK     0x0F       // bits 3-0: volume (0-15)

// ── Stato interno POKEY ────────────────────────────────────
typedef struct {
    u8  audf;        // registro frequenza (0-255)
    u8  audc;        // registro controllo
    u8  volume;      // volume estratto (0-15)
    u8  distortion;  // distorsione estratta (0-7)
    u8  volume_only; // flag volume-only
    u32 frequency;   // frequenza calcolata in Hz
    u8  active;      // canale attivo?
} pokey_channel_t;

typedef struct {
    pokey_channel_t ch[4];
    u8  audctl;
    u8  skctl;

    // Poly counters (per distorsione noise)
    u32 poly4;
    u32 poly5;
    u32 poly9;
    u32 poly17;

    u8  initialized;
} pokey_state_t;

static pokey_state_t pokey;

// ── Poly counter helpers ────────────────────────────────────
// LFSR a feedback lineare, come nel POKEY originale
static u8 poly4_table[15];
static u8 poly5_table[31];
static u8 poly17_out = 0;

static void build_poly_tables(void) {
    // 4-bit LFSR: tap bits 0 e 1 (polinomio x^4+x+1)
    u32 sr = 0xF;
    for (int i = 0; i < 15; i++) {
        poly4_table[i] = sr & 1;
        u32 bit = ((sr >> 0) ^ (sr >> 1)) & 1;
        sr = ((sr >> 1) | (bit << 3)) & 0xF;
    }

    // 5-bit LFSR: tap bits 0 e 2 (polinomio x^5+x^2+1)
    sr = 0x1F;
    for (int i = 0; i < 31; i++) {
        poly5_table[i] = sr & 1;
        u32 bit = ((sr >> 0) ^ (sr >> 2)) & 1;
        sr = ((sr >> 1) | (bit << 4)) & 0x1F;
    }
}

// Legge il bit corrente del poly scelto e avanza
static u8 poly_out(pokey_channel_t* ch) {
    u8 dist = ch->distortion;
    // dist 5 e 7 = tono puro (nessuna maschera noise)
    if (dist == 5 || dist == 7) return 1;

    u8 p5 = poly5_table[pokey.poly5 % 31];
    pokey.poly5 = (pokey.poly5 + 1) % 31;

    u8 p4  = poly4_table[pokey.poly4 % 15];
    pokey.poly4 = (pokey.poly4 + 1) % 15;

    // dist 6 = solo 4-bit poly
    if (dist == 6) return p4;
    // dist 1 o 3 = solo 5-bit poly
    if (dist == 1 || dist == 3) return p5;
    // dist 2 = 5-bit AND 4-bit
    if (dist == 2) return p5 & p4;
    // dist 0 = 5-bit AND 17-bit (qui usiamo poly17_out come noise generico)
    // dist 4 = solo 17-bit
    u8 p17 = poly17_out;
    poly17_out ^= (p5 & 1);  // approssimazione LFSR 17-bit
    if (dist == 4) return p17;
    return p5 & p17;  // dist 0
}

// ── Calcolo frequenza da registro AUDF ─────────────────────
static u32 calc_frequency(int ch_idx) {
    pokey_channel_t* ch = &pokey.ch[ch_idx];
    if (ch->volume_only || ch->volume == 0) return 0;

    u32 clock;
    // Canale 1 (idx 0) può usare 1.79MHz
    if (ch_idx == 0 && (pokey.audctl & AUDCTL_CH1_179M))
        clock = POKEY_CLOCK_179M;
    // Canale 3 (idx 2) può usare 1.79MHz
    else if (ch_idx == 2 && (pokey.audctl & AUDCTL_CH3_179M))
        clock = POKEY_CLOCK_179M;
    else
        clock = POKEY_CLOCK_64K;

    // Modalità 16-bit: canali 1+2 combinati
    if (ch_idx == 0 && (pokey.audctl & AUDCTL_CH12_16BIT)) {
        u32 audf16 = ((u32)pokey.ch[1].audf << 8) | pokey.ch[0].audf;
        return clock / (2 * (audf16 + 7));
    }
    if (ch_idx == 2 && (pokey.audctl & AUDCTL_CH34_16BIT)) {
        u32 audf16 = ((u32)pokey.ch[3].audf << 8) | pokey.ch[2].audf;
        return clock / (2 * (audf16 + 7));
    }

    u32 divisor = (u32)ch->audf + 1;
    if (divisor == 0) divisor = 1;
    return clock / (2 * divisor);
}

// ── Aggiorna un canale dopo scrittura registro ──────────────
static void update_channel(int idx) {
    pokey_channel_t* ch = &pokey.ch[idx];
    ch->volume      = ch->audc & AUDC_VOL_MASK;
    ch->distortion  = (ch->audc & AUDC_DIST_MASK) >> AUDC_DIST_SHIFT;
    ch->volume_only = (ch->audc & AUDC_VOLUME_ONLY) ? 1 : 0;
    ch->frequency   = calc_frequency(idx);
    ch->active      = (ch->volume > 0) ? 1 : 0;
}

// ── PC Speaker helper ───────────────────────────────────────
static void speaker_set_freq(u32 freq) {
    if (freq == 0 || freq > 20000) {
        u8 ctl = inb(PC_SPEAKER_PORT);
        outb(PC_SPEAKER_PORT, ctl & ~0x03);
        return;
    }
    u32 divisor = 1193180 / freq;
    outb(PIT_CMD, 0xB6);
    outb(PIT_CH2_DATA, (u8)(divisor & 0xFF));
    outb(PIT_CH2_DATA, (u8)((divisor >> 8) & 0xFF));
    u8 ctl = inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, ctl | 0x03);
}

static void speaker_stop(void) {
    u8 ctl = inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, ctl & ~0x03);
}

static void pokey_delay(u32 ms) {
    // Usa il PIT: porta 0x40 conta a ~1193180 Hz
    // Aspettiamo ms millisecondi leggendo il timer
    u32 ticks = ms * 1193;
    u16 start, current;

    // Leggi valore corrente del PIT canale 0
    outb(0x43, 0x00);
    start = inb(0x40);
    start |= ((u16)inb(0x40) << 8);

    u32 elapsed = 0;
    while (elapsed < ticks) {
        outb(0x43, 0x00);
        current = inb(0x40);
        current |= ((u16)inb(0x40) << 8);
        elapsed += (start - current) & 0xFFFF;
        start = current;
    }
}

// ============================================================
//  API PUBBLICA — Scrittura registri POKEY
// ============================================================

void pokey_init(void) {
    int i;
    for (i = 0; i < 4; i++) {
        pokey.ch[i].audf        = 0;
        pokey.ch[i].audc        = 0;
        pokey.ch[i].volume      = 0;
        pokey.ch[i].distortion  = 0;
        pokey.ch[i].volume_only = 0;
        pokey.ch[i].frequency   = 0;
        pokey.ch[i].active      = 0;
    }
    pokey.audctl = 0;
    pokey.skctl  = 3;  // inizializzazione richiesta dal POKEY reale
    pokey.poly4  = 0;
    pokey.poly5  = 0;
    pokey.poly9  = 0;
    pokey.poly17 = 0;
    poly17_out   = 0;
    pokey.initialized = 1;

    build_poly_tables();
    speaker_stop();
}

// Scrivi registro AUDF (frequenza) canale 1-4
void pokey_set_audf(int channel, u8 value) {
    if (channel < 1 || channel > 4) return;
    pokey.ch[channel - 1].audf = value;
    update_channel(channel - 1);
}

// Scrivi registro AUDC (controllo) canale 1-4
void pokey_set_audc(int channel, u8 value) {
    if (channel < 1 || channel > 4) return;
    pokey.ch[channel - 1].audc = value;
    update_channel(channel - 1);
}

// Scrivi AUDCTL
void pokey_set_audctl(u8 value) {
    pokey.audctl = value;
    int i;
    for (i = 0; i < 4; i++) update_channel(i);
}

// Scrivi SKCTL (reset/init)
void pokey_set_skctl(u8 value) {
    pokey.skctl = value;
}

// STIMER: forza i canali audio allo stato iniziale
void pokey_stimer(void) {
    // Nel POKEY reale resetta i divisori dei canali
    // Qui azzeriamo i poly counters
    pokey.poly4 = 0;
    pokey.poly5 = 0;
    pokey.poly17 = 0;
    poly17_out = 0;
}

// Leggi volume canale (0-15)
u8 pokey_get_volume(int channel) {
    if (channel < 1 || channel > 4) return 0;
    return pokey.ch[channel - 1].volume;
}

// Leggi frequenza calcolata canale (Hz)
u32 pokey_get_frequency(int channel) {
    if (channel < 1 || channel > 4) return 0;
    return pokey.ch[channel - 1].frequency;
}

// ── Attiva il canale più alto in priorità sul PC Speaker ───
static void pokey_update_speaker(void) {
    // Cerca il canale attivo con volume più alto
    int best = -1;
    u8  best_vol = 0;
    int i;
    for (i = 0; i < 4; i++) {
        if (pokey.ch[i].active && pokey.ch[i].volume > best_vol) {
            best_vol = pokey.ch[i].volume;
            best = i;
        }
    }
    if (best < 0) {
        speaker_stop();
    } else {
        u32 freq = pokey.ch[best].frequency;
        // Applica effetto distorsione: rumore → freq random attorno al valore
        u8 dist = pokey.ch[best].distortion;
        if (dist != 5 && dist != 7) {
            // Per noise: modula la frequenza ±10% usando poly5
            u8 p = poly_out(&pokey.ch[best]);
            if (!p) freq = freq * 9 / 10;
        }
        speaker_set_freq(freq);
    }
}

// Suona il canale corrente per duration_ms
void pokey_play(u32 duration_ms) {
    pokey_update_speaker();
    pokey_delay(duration_ms);
    speaker_stop();
}

// Ferma tutti i canali
void pokey_stop_all(void) {
    int i;
    for (i = 0; i < 4; i++) {
        pokey.ch[i].active = 0;
        pokey.ch[i].volume = 0;
        pokey.ch[i].audc   = 0;
    }
    speaker_stop();
}

// ── Helper: calcola AUDF da frequenza Hz ───────────────────
// Inverso di: F = clock / (2 * (AUDF + 1))
// Restituisce il valore AUDF da scrivere nel registro
u8 pokey_freq_to_audf(u32 freq_hz, int use_179mhz) {
    if (freq_hz == 0) return 0;
    u32 clock = use_179mhz ? POKEY_CLOCK_179M : POKEY_CLOCK_64K;
    u32 audf = clock / (2 * freq_hz);
    if (audf > 0) audf--;
    if (audf > 255) audf = 255;
    return (u8)audf;
}

// ============================================================
//  SUONI PREDEFINITI stile POKEY
// ============================================================

// Note musicali in Hz
#define POKEY_C3  131
#define POKEY_D3  147
#define POKEY_E3  165
#define POKEY_F3  175
#define POKEY_G3  196
#define POKEY_A3  220
#define POKEY_B3  247
#define POKEY_C4  262
#define POKEY_D4  294
#define POKEY_E4  330
#define POKEY_F4  349
#define POKEY_G4  392
#define POKEY_A4  440
#define POKEY_B4  494
#define POKEY_C5  523
#define POKEY_D5  587
#define POKEY_E5  659
#define POKEY_G5  784
#define POKEY_A5  880
#define POKEY_C6  1047

// Struttura nota
typedef struct {
    u32 freq;
    u32 ms;
    u8  dist;   // distorsione POKEY (0-7)
    u8  vol;    // volume (0-15)
} pokey_note_t;

// Suona una singola nota
void pokey_play_note(u32 freq, u32 ms, u8 distortion, u8 volume) {
    u8 audf = pokey_freq_to_audf(freq, 0);
    u8 audc = (u8)((distortion << 5) | (volume & 0x0F));
    pokey_set_audf(1, audf);
    pokey_set_audc(1, audc);
    pokey_play(ms);
    pokey_delay(15);
}

// Suona una sequenza di note
void pokey_play_sequence(const pokey_note_t* notes, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        if (notes[i].freq == 0) {
            // pausa
            speaker_stop();
            pokey_delay(notes[i].ms);
        } else {
            pokey_play_note(notes[i].freq, notes[i].ms,
                            notes[i].dist, notes[i].vol);
        }
    }
    pokey_stop_all();
}

// ── Jingle di boot stile Atari ─────────────────────────────
void pokey_boot_jingle(void) {
    static const pokey_note_t seq[] = {
        {POKEY_C4, 90,  5, 12},
        {POKEY_E4, 90,  5, 12},
        {POKEY_G4, 90,  5, 12},
        {POKEY_C5, 160, 5, 14},
        {0,        40,  0,  0},
        {POKEY_G4, 80,  5, 10},
        {POKEY_C5, 180, 5, 14},
    };
    pokey_play_sequence(seq, 7);
}

// ── Suono di errore stile POKEY (noise) ───────────────────
void pokey_error_sound(void) {
    static const pokey_note_t seq[] = {
        {220, 120, 0, 12},  // noise basso
        {0,    40, 0,  0},
        {180, 120, 0, 12},
        {0,    40, 0,  0},
        {140, 160, 0, 10},
    };
    pokey_play_sequence(seq, 5);
}

// ── Beep generico ──────────────────────────────────────────
void pokey_beep(void) {
    pokey_play_note(POKEY_A4, 80, 5, 10);
}

// ── Suono laser stile arcade Atari ─────────────────────────
void pokey_laser_sound(void) {
    u32 freq = 1200;
    u32 i;
    for (i = 0; i < 12; i++) {
        pokey_play_note(freq, 20, 4, 12);
        freq -= 80;
    }
    speaker_stop();
}

// ── Suono esplosione (noise) ────────────────────────────────
void pokey_explosion_sound(void) {
    static const pokey_note_t seq[] = {
        {180, 60, 0, 15},
        {140, 60, 0, 14},
        {100, 80, 0, 13},
        { 70, 80, 0, 11},
        { 50, 100, 0, 8},
    };
    pokey_play_sequence(seq, 5);
}

// ── Suono power-up ──────────────────────────────────────────
void pokey_powerup_sound(void) {
    static const pokey_note_t seq[] = {
        {POKEY_C4, 60, 5, 8},
        {POKEY_E4, 60, 5, 9},
        {POKEY_G4, 60, 5, 11},
        {POKEY_C5, 60, 5, 12},
        {POKEY_E5, 60, 5, 13},
        {POKEY_G5, 120, 5, 14},
    };
    pokey_play_sequence(seq, 6);
}

// ── Melodia Missile Command stile ─────────────────────────
void pokey_missile_command_theme(void) {
    static const pokey_note_t seq[] = {
        {POKEY_A4, 120, 5, 12}, {POKEY_A4, 60,  5, 10},
        {POKEY_E5, 180, 5, 13}, {0, 30, 0, 0},
        {POKEY_D5, 120, 5, 12}, {POKEY_C5, 120, 5, 11},
        {POKEY_A4, 240, 5, 12}, {0, 60, 0, 0},
        {POKEY_G4, 120, 5, 10}, {POKEY_A4, 120, 5, 11},
        {POKEY_E5, 300, 5, 13},
    };
    pokey_play_sequence(seq, 11);
}

// ============================================================
//  STATUS e DEBUG
// ============================================================

// Converte u32 in stringa decimale (uso interno)
static void u32_to_str(u32 n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = 0;
}

void pokey_status(void) {
    vga_writeln("POKEY Emulator Status:");
    char buf[16];
    int i;
    for (i = 0; i < 4; i++) {
        vga_write("  CH");
        buf[0] = '1' + i; buf[1] = 0;
        vga_write(buf);
        vga_write(": AUDF=");
        u32_to_str(pokey.ch[i].audf, buf);
        vga_write(buf);
        vga_write(" FREQ=");
        u32_to_str(pokey.ch[i].frequency, buf);
        vga_write(buf);
        vga_write("Hz VOL=");
        u32_to_str(pokey.ch[i].volume, buf);
        vga_write(buf);
        vga_write(" DIST=");
        u32_to_str(pokey.ch[i].distortion, buf);
        vga_write(buf);
        vga_writeln(pokey.ch[i].active ? " [ON]" : " [off]");
    }
}
