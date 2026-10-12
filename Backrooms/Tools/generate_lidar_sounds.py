"""v4.13 : sons originaux du scanner LiDAR et de la mission du Level 6 (synthese procedurale, memes outils que
generate_sounds.py ; rien n'est repris d'un enregistrement ni de la video de reference).

Usage :  python Tools/generate_lidar_sounds.py      (necessite numpy)
Sortie (RawAssets/Sounds, mono 16 bits, 48 kHz) :
- S_LidarPulse : impulsion large : montee breve d'un balayage, cliquetis de l'obturateur, souffle qui s'eteint.
- S_LidarFocus : scan focalise maintenu : bourdonnement de moteur fin et grenaille de mesures (boucle).
- S_LidarStatic : parasites de l'emetteur du dernier passage : crepitement radio, porteuse instable (boucle, positionnel).
- S_LidarDeny : refus d'une porte du passage : salve de parasites, claquement du pene qui ne cede pas.
- S_LidarPost : poste de confinement remis sous tension : contacteur, montee d'un transformateur, ronflement court.
- S_LidarCalib : calibrage termine : trois bips montants (sans parole, compris dans toutes les langues).
Les boucles sont ajoutees a loops.txt.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_sounds as gs  # noqa: E402
import generate_mission_sounds as gm  # noqa: E402

SR = gs.SR
RNG = np.random.default_rng(6130)


def chirp(dur, f0, f1):
    n = int(dur * SR)
    t = np.arange(n) / SR
    f = f0 + (f1 - f0) * (t / dur) ** 1.6
    return np.sin(2 * np.pi * np.cumsum(f) / SR)


def s_pulse():
    dur = 1.1
    n = int(dur * SR)
    x = np.zeros(n)
    sweep = chirp(0.18, 700, 4200) * gs.env(int(0.18 * SR), 0.02, 0.2, 0.6, 0.6, 0.3) * 0.35
    gs.place(x, sweep, 0, False)
    gs.place(x, gm.click(18, 2500, 11000, 300) * 0.8, int(0.17 * SR), False)
    tail_n = int(0.9 * SR)
    tail = gs.fft_filter(RNG.standard_normal(tail_n), lo=1500, hi=7000) * gs.env(tail_n, 0.01, 0.1, 0.3, 0.85, 0.2) * 0.25
    gs.place(x, tail, int(0.18 * SR), False)
    gs.save("S_LidarPulse", gs.reverb(x, 0.6, 0.25), 0.7)


def s_focus():
    dur = 4.0
    t = gs.t_axis(dur)
    motor = gs.sine(np.full(len(t), gs.loop_freq(118, dur))) * 0.25 + gs.sine(np.full(len(t), gs.loop_freq(236, dur))) * 0.12
    grit = gs.fft_filter(gs.white(dur), lo=3000, hi=9000, circular=True)
    grit /= np.std(grit)
    ticks = (np.sin(2 * np.pi * gs.loop_freq(24.0, dur) * t) > 0.92).astype(float)
    x = motor + grit * 0.08 * (0.6 + 0.4 * ticks)
    gs.save("S_LidarFocus", x, 0.45, loop=True)


def s_static():
    dur = 6.0
    t = gs.t_axis(dur)
    hiss = gs.fft_filter(gs.white(dur), lo=400, hi=6500, circular=True)
    hiss /= np.std(hiss)
    # Crepitement : impulsions aleatoires breves (bouclable : rien ne depasse la fin)
    crack = np.zeros(len(t))
    for _ in range(int(dur * 28)):
        p = int(RNG.uniform(0, len(t) - 600))
        crack[p:p + 400] += RNG.uniform(0.3, 1.0) * np.exp(-np.arange(400) / 40.0) * RNG.choice([-1, 1])
    carrier = gs.sine(np.full(len(t), gs.loop_freq(1020, dur))) * (0.5 + 0.5 * np.sin(2 * np.pi * gs.loop_freq(0.7, dur) * t)) * 0.08
    wobble = 0.6 + 0.4 * np.sin(2 * np.pi * gs.loop_freq(0.23, dur) * t + 0.8)
    x = hiss * 0.35 * wobble + crack * 0.5 + carrier
    gs.save("S_LidarStatic", x, 0.6, loop=True)


def s_deny():
    dur = 1.3
    n = int(dur * SR)
    x = np.zeros(n)
    burst_n = int(0.7 * SR)
    burst = gs.fft_filter(RNG.standard_normal(burst_n), lo=300, hi=7000) * gs.env(burst_n, 0.01, 0.1, 0.8, 0.4, 0.4)
    gate = (RNG.random(burst_n) > 0.35).astype(float)
    gate = np.convolve(gate, np.ones(240) / 240, mode="same")
    gs.place(x, burst * gate * 0.6, 0, False)
    gs.place(x, gm.click(30, 900, 6000, 160) * 1.0, int(0.05 * SR), False)
    gs.place(x, gm.thump(0.3, 70, 140, 25) * 0.7, int(0.06 * SR), False)
    gs.place(x, gs.metal_hit(0.4, 420)[: int(0.4 * SR)] * 0.25, int(0.07 * SR), False)
    gs.save("S_LidarDeny", gs.reverb(x, 0.7, 0.3), 0.8)


def s_post():
    dur = 2.2
    n = int(dur * SR)
    x = np.zeros(n)
    gs.place(x, gm.click(40, 600, 5000, 120) * 1.0, 0, False)
    gs.place(x, gm.thump(0.3, 50, 110, 20) * 0.8, int(0.01 * SR), False)
    m = int(1.8 * SR)
    tt = np.arange(m) / SR
    f = 50 + 70 * np.clip(tt / 0.6, 0, 1)
    hum = (gs.sine(f) * 0.4 + gs.sine(f * 2) * 0.25 + gs.sine(f * 3) * 0.1) * gs.env(m, 0.15, 0.2, 0.7, 0.5, 0.4)
    gs.place(x, hum * 0.6, int(0.1 * SR), False)
    gs.save("S_LidarPost", gs.reverb(x, 0.8, 0.3), 0.75)


def s_calib():
    dur = 0.9
    n = int(dur * SR)
    x = np.zeros(n)
    for i, f in enumerate((880, 1175, 1568)):
        m = int(0.12 * SR)
        b = gs.sine(np.full(m, float(f))) * gs.env(m, 0.01, 0.1, 0.7, 0.4, 0.3)
        gs.place(x, b * 0.35, int((0.04 + i * 0.2) * SR), False)
    gs.save("S_LidarCalib", x, 0.6)


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
    print("Sons du scanner LiDAR dans", os.path.abspath(gs.OUT))
    s_pulse()
    s_focus()
    s_static()
    s_deny()
    s_post()
    s_calib()
    add_loop("S_LidarFocus")
    add_loop("S_LidarStatic")
