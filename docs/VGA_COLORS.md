# Colori VGA per NovaOS

Questo file serve solo come appunto per noi. Non viene mostrato dentro NovaOS.

## Come funziona il colore

In funzioni come:

```cpp
vga_write_at(27, 5, "Testo", 0x1F);
```

il valore `0x1F` indica colore sfondo + colore testo.

```text
0x1F
  1 = sfondo
  F = testo
```

Quindi:

```text
0x1F = sfondo blu, testo bianco brillante
```

## Tabella colori base

```text
0 = nero
1 = blu
2 = verde
3 = ciano
4 = rosso
5 = viola/magenta
6 = marrone/giallo scuro
7 = grigio chiaro
8 = grigio scuro
9 = blu chiaro
A = verde chiaro
B = ciano chiaro
C = rosso chiaro
D = magenta chiaro
E = giallo
F = bianco brillante
```

## Esempi solo testo su sfondo nero

```cpp
vga_write_at(2, 2, "Bianco", 0x0F);
vga_write_at(2, 3, "Verde chiaro", 0x0A);
vga_write_at(2, 4, "Verde scuro", 0x02);
vga_write_at(2, 5, "Giallo", 0x0E);
vga_write_at(2, 6, "Rosso", 0x0C);
vga_write_at(2, 7, "Ciano", 0x0B);
vga_write_at(2, 8, "Blu chiaro", 0x09);
```

## Esempi con sfondo blu

```cpp
vga_write_at(2, 2, "Bianco su blu", 0x1F);
vga_write_at(2, 3, "Verde su blu", 0x1A);
vga_write_at(2, 4, "Giallo su blu", 0x1E);
vga_write_at(2, 5, "Ciano su blu", 0x1B);
```

## Esempi per NovaOS

Titolo importante:

```cpp
vga_write_at(2, 1, "NovaOS", 0x1E);
```

Testo normale nel desktop:

```cpp
vga_write_at(27, 5, "Welcome to NovaOS", 0x1F);
```

Testo verde nel terminale:

```cpp
vga_write_at(4, 16, "nova> ", 0x0A);
```

Errore:

```cpp
vga_write_at(4, 10, "Error", 0x0C);
```

Avviso:

```cpp
vga_write_at(4, 10, "Warning", 0x0E);
```

OK/successo:

```cpp
vga_write_at(4, 10, "OK", 0x0A);
```

## Nota importante

Lo schermo testuale ha circa queste coordinate:

```text
X: 0 - 79
Y: 0 - 24
```

Se il testo esce dallo schermo, aumenta o diminuisci la X.
