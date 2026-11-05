# SCRIBA Headphones — Meteora Plugins

Correzione della risposta delle cuffie (curve AutoEq, target Harman) + crossfeed + utility di ascolto.
VST3 (Windows/macOS) e AU (macOS), JUCE 8.

## Catena audio
1. **Monitor**: Swap L/R, Mono, Side (Mono e Side si escludono)
2. **Listen On**
   - *Casse da studio* (con il dispositivo Studio): MIX = Classic NF, Cube (mono), Modern NF;
     MASTERING = Reference, Big Mains, Hi-Fi. Ogni cassa = risposta tipica + angolo/Amount/Room consigliati.
   - *Dispositivi*: Phone, BT Speaker, Laptop, Earbuds (mono/stereo stretto, EQ, compressione, saturazione).
3. **Virtual Monitors** (modello della testa Brown & Duda + ITD di Woodworth su tutta la banda, in mid/side):
   - *Width* = angolo tra le casse (0–90°, 0° = mono) · *Amount* = segnale all'orecchio opposto (0–100 %, default 70 %)
   - *Room* = prime riflessioni + coda diffusa FDN (RT60 0,15–0,35 s), bassi sempre mono (passa-alto 250 Hz)
   - tarato su CLA Nx e HoRNet VHS: Room 35 % ~ −12 dB di stanza, Room 100 % ~ 0 dB (come Nx)
4. **Correzione cuffia**: fino a 16 filtri AutoEq del preset scelto, scalati da Intensity, più il preamp
5. **Auto Gain** (default ON): riallinea il volume pesato (stile K) all'ingresso, ±18 dB, finestra ~3 s
6. **Compare A/B** e **Output** ±12 dB

Latenza zero, nessuna allocazione sul thread audio.

## Compilare
```
cmake -B build -DCMAKE_BUILD_TYPE=Release        # scarica JUCE 8.0.10 da GitHub
cmake --build build --config Release
```
Con una copia locale di JUCE: `-DJUCE_DIR=/percorso/JUCE`.
Su GitHub la build parte da sola (`.github/workflows/build.yml`): i plugin si scaricano dagli Artifacts.

## Aggiungere o aggiornare cuffie
I preset stanno in `Source/HeadphonePresets.h`, generato da `tools/gen_presets.py` a partire dai file
`ParametricEQ.txt` di AutoEq (`results/oratory1990/over-ear/<nome cuffia>/`):
```
python tools/gen_presets.py <AutoEq>/results/oratory1990/over-ear cuffie.txt Source/HeadphonePresets.h
```
`cuffie.txt` = un nome di cartella per riga; la prima riga è il preset predefinito (HiFiMan HE400se).
Dal plugin si può anche importare qualsiasi `ParametricEQ.txt` con **Importa curva AutoEq**: la curva viene salvata nel progetto della DAW.

## Da verificare prima della vendita
- `PLUGIN_MANUFACTURER_CODE` in `CMakeLists.txt` (ora `Mtra`): metti lo stesso codice produttore degli altri plugin Meteora.
- Licenza delle misure: i preset usano le misure di oratory1990 tramite AutoEq; il loro uso in un prodotto a pagamento
  richiede il suo permesso. In alternativa si possono rigenerare i preset da altre fonti cambiando cartella nello script.
