# Nova Picture (.lnp)

`.lnp` significa **Nova Picture**. E' il formato immagine pensato per NovaOS.

Per ora e' un formato esterno al kernel: lo creiamo con Python sul PC di sviluppo. Piu' avanti NovaOS imparera' a leggerlo e mostrarlo nel desktop.

## Formato LNP1

```text
byte 0..3   magic: LNP1
byte 4..5   width  uint16 little-endian
byte 6..7   height uint16 little-endian
byte 8..    pixel RGB: R G B R G B ...
```

Ogni pixel usa 3 byte:

```text
R = rosso
G = verde
B = blu
```

## Tool Python

Creare una immagine galassia di prova:

```bash
python3 tools/lnp_tool.py sample assets/galaxy.lnp
```

Vedere informazioni:

```bash
python3 tools/lnp_tool.py info assets/galaxy.lnp
```

Convertire da PPM a LNP:

```bash
python3 tools/lnp_tool.py from-ppm image.ppm image.lnp
```

Convertire da LNP a PPM:

```bash
python3 tools/lnp_tool.py to-ppm image.lnp image.ppm
```

Convertire da LNP a PNG, piu' comodo da aprire:

```bash
python3 tools/lnp_tool.py to-png image.lnp image.png
```

## Perche' Python?

Python non controlla l'hardware. Qui serve come officina esterna: prepara immagini e file mentre sviluppiamo NovaOS su Linux/Windows.

Dentro NovaOS, in futuro, sara' il codice C/C++ o TencleLang a leggere `.lnp`.
