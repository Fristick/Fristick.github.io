"""v4.13 : sons de la porte de service du Level 0 (synthese procedurale, memes outils que generate_sounds.py).

Usage :  python Tools/generate_story_sounds.py      (necessite numpy)
Sortie : RawAssets/Sounds/S_Draft.wav (boucle, ajoutee a loops.txt) et S_ServiceDoor.wav (mono 16 bits, 48 kHz).
Les autres sons ne sont pas touches.

- S_Draft : le courant d'air froid qui passe sous la porte : souffle etroit qui siffle un peu, rafales lentes. Il sert de
  repere a l'oreille (la porte n'a pas le bourdonnement des sorties) ; son equivalent visuel est le filet de lumiere
  froide sous la porte, son equivalent ecrit l'aide d'orientation (sous-titre).
- S_ServiceDoor : poignee qui cede (declic du pene), charnieres qui grincent, souffle d'air quand la porte s'ouvre.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_sounds as gs  # noqa: E402
import generate_mission_sounds as gm  # noqa: E402

SR = gs.SR
RNG = np.random.default_rng(4130)


def s_draft():
    """Courant d'air sous une porte : souffle filtre (bouclable), sifflement etroit, rafales lentes"""
    dur = 8.0
    t = gs.t_axis(dur)
    base = gs.wind(dur, lo=120, hi=900, gust=0.6)
    hiss = gs.fft_filter(gs.white(dur), lo=1800, hi=4200, circular=True)
    hiss /= np.std(hiss)
    whistle = gs.resonate(gs.fft_filter(gs.white(dur), lo=600, hi=2400, circular=True), [gs.loop_freq(1240, dur), gs.loop_freq(1860, dur)], q=60.0,
                          gains=[1.0, 0.5], circular=True)
    whistle /= np.std(whistle)
    gust = 0.55 + 0.45 * np.sin(2 * np.pi * gs.loop_freq(0.13, dur) * t + 1.3) * np.sin(2 * np.pi * gs.loop_freq(0.31, dur) * t + 0.4)
    x = base * 0.8 + hiss * 0.18 * gust + whistle * 0.12 * gust ** 2
    gs.save("S_Draft", x, 0.6, loop=True)


def s_service_door():
    """Porte de service qui s'ouvre : pene, poignee, charnieres, air froid"""
    dur = 1.8
    n = int(dur * SR)
    x = np.zeros(n)
    # Poignee abaissee et pene qui se retire (deux declics metalliques)
    gs.place(x, gm.click(25, 1800, 9000, 220) * 0.9, int(0.02 * SR), False)
    gs.place(x, gs.metal_hit(0.25, 640)[: int(0.25 * SR)] * 0.35, int(0.03 * SR), False)
    gs.place(x, gm.click(30, 1200, 7000, 180) * 1.1, int(0.16 * SR), False)
    gs.place(x, gm.thump(0.25, 55, 120, 20) * 0.6, int(0.17 * SR), False)
    # Charnieres : grincement qui glisse (frottement resonant), deux voix
    m = int(1.1 * SR)
    tt = np.arange(m) / SR
    f = 380 + 260 * np.sin(np.pi * tt / 1.1) + 40 * np.sin(2 * np.pi * 7.0 * tt)
    creak = gs.sine(f) * 0.25 + gs.sine(f * 2.03) * 0.12 + gs.sine(f * 3.1) * 0.05
    rough = 0.6 + 0.4 * np.abs(np.sin(2 * np.pi * 23.0 * tt + np.sin(2 * np.pi * 3.0 * tt)))
    creak *= rough * gs.env(m, 0.08, 0.3, 0.7, 0.3, 0.5)
    gs.place(x, creak, int(0.3 * SR), False)
    # Air froid qui entre
    w = int(1.4 * SR)
    whoosh = gs.fft_filter(RNG.standard_normal(w), lo=200, hi=2600) * gs.env(w, 0.35, 0.3, 0.5, 0.6, 0.3) * 0.45
    gs.place(x, whoosh, int(0.35 * SR), False)
    gs.save("S_ServiceDoor", gs.reverb(x, 0.9, 0.3), 0.85)


def add_loop(name):
    path = os.path.join(gs.OUT, "loops.txt")
    names = []
    if os.path.isfile(path):
        names = [l.strip() for l in open(path, encoding="utf-8") if l.strip()]
    if name not in names:
        names.append(name)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("\n".join(names) + "\n")


if __name__ == "__main__":
    print("Sons de la porte de service dans", os.path.abspath(gs.OUT))
    s_draft()
    s_service_door()
    add_loop("S_Draft")
