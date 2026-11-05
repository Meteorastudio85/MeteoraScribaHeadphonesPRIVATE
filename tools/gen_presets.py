"""Genera Source/HeadphonePresets.h dai file ParametricEQ.txt di AutoEq.
Uso: python gen_presets.py <cartella results/oratory1990/over-ear> <lista.txt> <output.h>"""
import re, sys, pathlib
root, lst, out = map(pathlib.Path, sys.argv[1:4])
names = [l.strip() for l in lst.read_text().splitlines() if l.strip()]
rx = re.compile(r"Filter \d+: ON (PK|LSC|HSC) Fc ([\d.]+) Hz Gain ([-\d.]+) dB Q ([\d.]+)")
types = {"PK": "Peak", "LSC": "LowShelf", "HSC": "HighShelf"}
lines = ["// Generato automaticamente da tools/gen_presets.py - non modificare a mano.",
         "// Dati: AutoEq (github.com/jaakkopasanen/AutoEq), misure oratory1990, target Harman over-ear.",
         "#pragma once", '#include "EqTypes.h"', "", "namespace scriba", "{",
         "inline const std::vector<HeadphoneProfile>& getHeadphonePresets()", "{",
         "    static const std::vector<HeadphoneProfile> presets {"]
for n in names:
    txt = (root / n / f"{n} ParametricEQ.txt").read_text()
    pre = float(re.search(r"Preamp: ([-\d.]+) dB", txt).group(1))
    fs = [f"{{ EqBand::{types[t]}, {float(f):.1f}f, {float(g):.2f}f, {float(q):.2f}f }}" for t, f, g, q in rx.findall(txt)]
    disp = n.replace("HIFIMAN", "HiFiMan")
    lines.append(f'        {{ "{disp}", "oratory1990", {pre:.1f}f, {{ {", ".join(fs)} }} }},')
lines += ["    };", "    return presets;", "}", "} // namespace scriba", ""]
out.write_text("\n".join(lines))
print(f"{len(names)} preset scritti")
