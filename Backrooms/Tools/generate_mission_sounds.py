"""v4.11 : sons des mecanismes de mission (synthese procedurale, memes outils que generate_sounds.py).

Usage :  python Tools/generate_mission_sounds.py      (necessite numpy)
Sortie : RawAssets/Sounds/S_M_*.wav (mono 16 bits, 48 kHz). Les sons existants ne sont pas touches.

Chaque son a un equivalent visuel (mouvement, lampe) et textuel (sous-titre) dans le jeu : aucune enigme ne repose
seulement sur l'audition.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_sounds as gs  # noqa: E402

SR = gs.SR
RNG = np.random.default_rng(4110)


def click(n_ms=40, lo=800, hi=6000, decay=120.0):
    n = int(n_ms / 1000 * SR)
    t = np.arange(n) / SR
    return gs.fft_filter(RNG.standard_normal(n), lo=lo, hi=hi) * np.exp(-t * decay)


def thump(dur=0.3, f0=60, f1=160, decay=12.0):
    n = int(dur * SR)
    t = np.arange(n) / SR
    return np.sin(2 * np.pi * (f0 + f1 * np.exp(-t * 30)) * t) * np.exp(-t * decay)


def s_switch():
    """Levier ou disjoncteur : declic sec et choc du boitier"""
    n = int(0.45 * SR)
    x = np.zeros(n)
    gs.place(x, click(30, 1500, 9000, 160) * 1.2, 0, False)
    gs.place(x, thump(0.3, 70, 180, 18) * 0.9, int(0.012 * SR), False)
    gs.place(x, gs.metal_hit(0.4, 420)[: int(0.4 * SR)] * 0.25, int(0.01 * SR), False)
    gs.save("S_M_Switch", gs.reverb(x, 0.6, 0.25), 0.8)


def s_dial():
    """Cadran ou molette : crans successifs"""
    n = int(0.35 * SR)
    x = np.zeros(n)
    for k in range(3):
        gs.place(x, click(18, 2500, 9000, 260) * (1.0 - 0.2 * k), int(k * 0.07 * SR), False)
    gs.save("S_M_Dial", gs.reverb(x, 0.4, 0.2), 0.6)


def s_valve():
    """Vanne : grincement metallique et ecoulement"""
    dur = 1.1
    n = int(dur * SR)
    t = np.arange(n) / SR
    f = 520 + 140 * np.sin(2 * np.pi * 1.7 * t)
    squeal = gs.sine(f) * 0.35 + gs.sine(f * 2.01) * 0.12
    squeal *= gs.env(n, 0.05, 0.4, 0.4, 0.3, 0.3)
    flow = gs.fft_filter(RNG.standard_normal(n), lo=300, hi=2500) * gs.env(n, 0.2, 0.3, 0.6, 0.4, 0.2) * 0.5
    gs.save("S_M_Valve", gs.reverb(squeal + flow, 1.0, 0.3), 0.75)


def s_crank():
    """Manivelle, treuil, generateur : cliquet regulier (une seconde, rejoue tant qu'on tient)"""
    dur = 1.0
    n = int(dur * SR)
    x = np.zeros(n)
    for k in range(8):
        gs.place(x, click(22, 1200, 7000, 200) * (0.8 + 0.2 * RNG.random()), int(k * dur / 8 * SR), False)
    t = np.arange(n) / SR
    x += gs.fft_filter(RNG.standard_normal(n), lo=60, hi=400) * 0.25 * (0.6 + 0.4 * np.sin(2 * np.pi * 2 * t))
    gs.save("S_M_Crank", gs.reverb(x, 0.7, 0.25), 0.7)


def s_steam():
    """Fuite de vapeur : sifflement qui monte puis se calme"""
    dur = 1.8
    n = int(dur * SR)
    hiss = gs.fft_filter(RNG.standard_normal(n), lo=2500, hi=11000) * gs.env(n, 0.04, 0.3, 0.7, 1.0, 0.3)
    hiss += gs.fft_filter(RNG.standard_normal(n), lo=600, hi=2000) * gs.env(n, 0.1, 0.5, 0.3, 0.8, 0.2) * 0.4
    gs.save("S_M_Steam", gs.reverb(hiss, 1.2, 0.3), 0.7)


def s_relay():
    """Relais de puissance : claquement lourd puis ronflement du secteur"""
    dur = 1.4
    n = int(dur * SR)
    t = np.arange(n) / SR
    x = np.zeros(n)
    gs.place(x, thump(0.4, 45, 140, 9) * 1.3, 0, False)
    gs.place(x, click(35, 900, 5000, 90), 0, False)
    hum = (np.sin(2 * np.pi * 100 * t) * 0.4 + np.sin(2 * np.pi * 200 * t) * 0.2) * np.clip((t - 0.1) / 0.3, 0, 1) * np.exp(-np.maximum(t - 0.6, 0) * 3)
    gs.save("S_M_Relay", gs.reverb(x + hum * 0.6, 1.0, 0.3), 0.85)


def s_trip():
    """Disjonction : arc electrique et chute de tension"""
    dur = 1.6
    n = int(dur * SR)
    t = np.arange(n) / SR
    zap = gs.fft_filter(RNG.standard_normal(n), lo=1500, hi=9000) * np.exp(-t * 6)
    zap *= (np.sin(2 * np.pi * 37 * t) > 0.2) * 1.0 + 0.3
    f = 120 * np.exp(-t * 2.5) + 25
    whir = gs.saw(f) * 0.4 * np.exp(-t * 1.8)
    x = zap * 0.8 + whir
    gs.place(x, thump(0.3, 50, 130, 12) * 1.2, int(0.05 * SR), False)
    gs.save("S_M_Trip", gs.reverb(x, 1.6, 0.35), 0.95)


def s_lock():
    """Cle dans la serrure : cliquetis, rotation, pene qui glisse"""
    n = int(0.7 * SR)
    x = np.zeros(n)
    for k, d in enumerate((0.0, 0.05, 0.09)):
        gs.place(x, click(20, 2500, 9000, 220) * (0.7 + 0.15 * k), int(d * SR), False)
    gs.place(x, click(60, 600, 3000, 40) * 0.6, int(0.25 * SR), False)
    gs.place(x, thump(0.2, 90, 220, 25) * 0.7, int(0.42 * SR), False)
    gs.save("S_M_Lock", gs.reverb(x, 0.5, 0.25), 0.75)


def s_gate():
    """Porte lourde, grille ou passerelle qui se deplace"""
    dur = 2.4
    n = int(dur * SR)
    t = np.arange(n) / SR
    rumble = gs.fft_filter(RNG.standard_normal(n), lo=40, hi=500) * gs.env(n, 0.2, 0.5, 0.8, 0.8, 0.9)
    rattle = gs.fft_filter(RNG.standard_normal(n), lo=1200, hi=4000) * (np.sin(2 * np.pi * 11 * t) > 0.6) * 0.25 * gs.env(n, 0.2, 0.3, 0.7, 0.6, 1.0)
    x = rumble + rattle
    gs.place(x, gs.metal_hit(0.8, 140)[: int(0.8 * SR)] * 0.6, int(2.0 * SR), False)
    gs.save("S_M_Gate", gs.reverb(x, 1.8, 0.35), 0.85)


def s_beacon():
    """Balise mecanique : cliquetis de remontage et timbre quand elle s'allume"""
    dur = 1.2
    n = int(dur * SR)
    t = np.arange(n) / SR
    x = np.zeros(n)
    for k in range(6):
        gs.place(x, click(25, 1500, 8000, 180) * 0.7, int(k * 0.11 * SR), False)
    bell = sum(np.sin(2 * np.pi * 880 * r * t) * np.exp(-t * (2 + r)) / (1 + r) for r in (1.0, 2.4, 3.9))
    gs.place(x, bell[: n - int(0.7 * SR)] * 0.6, int(0.7 * SR), False)
    gs.save("S_M_Beacon", gs.reverb(x, 1.4, 0.35), 0.8)


def s_mill():
    """Moulin : bois qui grince, ailes qui tournent"""
    dur = 2.0
    n = int(dur * SR)
    t = np.arange(n) / SR
    creak = gs.resonate(RNG.standard_normal(n) * (np.sin(2 * np.pi * 3 * t) > 0.7), [240, 610, 1200], q=18) * 0.6
    whoosh = gs.fft_filter(RNG.standard_normal(n), lo=200, hi=1500) * (0.5 + 0.5 * np.sin(2 * np.pi * 1.2 * t)) * 0.5
    gs.save("S_M_Mill", gs.reverb(creak + whoosh, 1.5, 0.3), 0.75)


if __name__ == "__main__":
    print("Sons des mecanismes de mission dans", os.path.abspath(gs.OUT))
    for fn in (s_switch, s_dial, s_valve, s_crank, s_steam, s_relay, s_trip, s_lock, s_gate, s_beacon, s_mill):
        fn()
