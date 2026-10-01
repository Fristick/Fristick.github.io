"""
Synthese procedurale de tous les sons du jeu (ambiances bouclables, pas, entites...).

Usage :  python generate_sounds.py      (necessite numpy)
Sortie : ../RawAssets/Sounds/*.wav  (mono 16 bits)

Les sons "S_Amb_*", "S_Hum", "S_Heartbeat", "S_Breath", "S_Chase", "S_ExitHum" et
"S_Moth", "S_Underwater" sont des boucles parfaites : ils sont filtres dans le domaine de Fourier
de facon circulaire, donc sans "clic" au raccord.
"""
import os
import wave
import numpy as np

SR = 32000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "RawAssets", "Sounds")
os.makedirs(OUT, exist_ok=True)
RNG = np.random.default_rng(1234)

# Liste des sons bouclables (utilisee aussi par le script d'import Unreal)
LOOPS = set()


# ---------------------------------------------------------------------------
# Outils DSP
# ---------------------------------------------------------------------------
def t_axis(dur):
    return np.arange(int(dur * SR)) / SR


def white(dur, rng=RNG):
    return rng.standard_normal(int(dur * SR))


def fft_filter(x, lo=None, hi=None, slope=4.0, circular=False, tilt=0.0):
    """Filtre passe-bande doux dans le domaine frequentiel.
    circular=True conserve la periodicite (pour les boucles)."""
    n = len(x)
    pad = 0 if circular else min(n, SR)
    y = np.concatenate([x, np.zeros(pad)])
    F = np.fft.rfft(y)
    f = np.fft.rfftfreq(len(y), 1 / SR)
    g = np.ones_like(f)
    if lo:
        g *= 1 / (1 + (lo / np.maximum(f, 1e-3)) ** slope)
    if hi:
        g *= 1 / (1 + (f / hi) ** slope)
    if tilt:
        g *= (np.maximum(f, 20) / 1000.0) ** tilt
    out = np.fft.irfft(F * g, len(y))
    if circular:
        return out
    return out[:n] + np.concatenate([out[n:n + pad], np.zeros(max(0, n - pad))])[:n] * 0  # queue ignoree


def resonate(x, freqs, q=12.0, gains=None, circular=False):
    """Banc de resonances (formants)"""
    n = len(x)
    pad = 0 if circular else SR // 2
    y = np.concatenate([x, np.zeros(pad)])
    F = np.fft.rfft(y)
    f = np.fft.rfftfreq(len(y), 1 / SR)
    g = np.zeros_like(f)
    for i, fc in enumerate(freqs):
        bw = fc / q
        a = 1.0 if gains is None else gains[i]
        g += a / (1 + ((f - fc) / bw) ** 2)
    out = np.fft.irfft(F * g, len(y))
    return out[:n] if not circular else out


def reverb(x, t60=1.2, wet=0.35, pre=0.01, rng=RNG):
    n_ir = int(t60 * SR)
    t = np.arange(n_ir) / SR
    ir = rng.standard_normal(n_ir) * np.exp(-6.9 * t / t60)
    ir = fft_filter(ir, hi=5000, slope=2)
    ir[: int(pre * SR)] = 0
    ir /= np.sqrt(np.sum(ir ** 2)) + 1e-9
    L = len(x) + n_ir
    y = np.fft.irfft(np.fft.rfft(x, L) * np.fft.rfft(ir, L), L)
    out = np.concatenate([x, np.zeros(n_ir)]) * (1 - wet) + y * wet * 0.6
    return out


def reverb_loop(x, t60=1.0, wet=0.3):
    """Reverb circulaire (pour les boucles)"""
    n = len(x)
    n_ir = min(int(t60 * SR), n)
    t = np.arange(n_ir) / SR
    ir = RNG.standard_normal(n_ir) * np.exp(-6.9 * t / t60)
    ir /= np.sqrt(np.sum(ir ** 2)) + 1e-9
    irp = np.zeros(n)
    irp[:n_ir] = ir
    y = np.fft.irfft(np.fft.rfft(x) * np.fft.rfft(irp), n)
    return x * (1 - wet) + y * wet * 0.6


def env(n, a=0.005, d=0.1, s=0.0, r=0.0, sus_t=0.0):
    out = np.zeros(n)
    ia, idd, isus, ir = int(a * SR), int(d * SR), int(sus_t * SR), int(r * SR)
    i = 0
    seg = min(ia, n - i)
    out[i:i + seg] = np.linspace(0, 1, max(seg, 1))[:seg]
    i += seg
    seg = min(idd, n - i)
    out[i:i + seg] = np.linspace(1, s, max(seg, 1))[:seg]
    i += seg
    seg = min(isus, n - i)
    out[i:i + seg] = s
    i += seg
    seg = min(ir, n - i)
    out[i:i + seg] = np.linspace(s, 0, max(seg, 1))[:seg]
    return out


def exp_env(n, tau):
    return np.exp(-np.arange(n) / SR / tau)


def saw(freq_arr):
    ph = np.cumsum(freq_arr / SR) % 1.0
    return 2 * ph - 1


def sine(freq_arr, phase=0.0):
    return np.sin(2 * np.pi * np.cumsum(np.broadcast_to(freq_arr, freq_arr.shape) / SR) + phase)


def place(buf, snd, start, circular=True):
    n = len(buf)
    idx = (np.arange(len(snd)) + start)
    if circular:
        np.add.at(buf, idx % n, snd)
    else:
        m = idx < n
        buf[idx[m]] += snd[m]


def normalize(x, peak=0.89):
    m = np.max(np.abs(x)) + 1e-9
    return x / m * peak


def fade_edges(x, ms=8):
    n = int(ms / 1000 * SR)
    if len(x) > 2 * n:
        x[:n] *= np.linspace(0, 1, n)
        x[-n:] *= np.linspace(1, 0, n)
    return x


# Frequences d'enregistrement possibles : un son sans aigus est stocke a une frequence plus basse
# (fichier 2 fois plus petit, aucune difference audible). Seuil : energie au-dessus de la nouvelle
# frequence de Nyquist inferieure a -60 dB de l'energie totale.
STORE_RATES = (16000, 22050)
HF_LIMIT_DB = -60.0


def store_rate(x, sr=SR):
    spec = np.abs(np.fft.rfft(x)) ** 2
    freqs = np.fft.rfftfreq(len(x), 1.0 / sr)
    total = spec.sum() + 1e-20
    for rate in STORE_RATES:
        if rate < sr and 10 * np.log10(spec[freqs > rate / 2].sum() / total + 1e-20) < HF_LIMIT_DB:
            return rate
    return sr


def resample(x, sr_from, sr_to):
    """Reechantillonnage par FFT (circulaire : une boucle reste parfaite si sa duree tombe juste)"""
    if sr_from == sr_to:
        return x
    n_to = int(round(len(x) * sr_to / sr_from))
    spec = np.fft.rfft(x)[: n_to // 2 + 1]
    return np.fft.irfft(spec, n_to) * (n_to / len(x))


def write_wav(path, x, sr):
    data = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(data.tobytes())


def save(name, x, peak=0.89, loop=False):
    x = normalize(np.asarray(x, dtype=np.float64), peak)
    if not loop:
        x = fade_edges(x)
    else:
        LOOPS.add(name)
    rate = store_rate(x)
    write_wav(os.path.join(OUT, name + ".wav"), resample(x, SR, rate), rate)
    print("  ->", name, f"{len(x) / SR:.1f}s", f"{rate} Hz", "(boucle)" if loop else "")


def loop_freq(f, dur):
    """Arrondit une frequence pour qu'elle boucle parfaitement sur 'dur'"""
    return round(f * dur) / dur


# ---------------------------------------------------------------------------
# Ambiances
# ---------------------------------------------------------------------------
def s_hum():
    dur = 4.0
    t = t_axis(dur)
    f0 = 120.0
    x = np.zeros_like(t)
    amps = [1.0, 0.55, 0.42, 0.25, 0.22, 0.12, 0.10, 0.07, 0.05, 0.04]
    for k, a in enumerate(amps, 1):
        x += a * np.sin(2 * np.pi * f0 * k * t + RNG.random() * 6.28)
    x += 0.35 * np.sin(2 * np.pi * 60 * t)
    # buzz haute frequence module a 120 Hz
    buzz = fft_filter(white(dur), lo=2500, hi=7000, circular=True)
    buzz *= 0.5 + 0.5 * np.sin(2 * np.pi * 120 * t) ** 8
    x = x / 3 + 0.25 * buzz / np.std(buzz)
    # leger tremblement d'amplitude (frequences entieres -> boucle parfaite)
    x *= 1 + 0.06 * np.sin(2 * np.pi * loop_freq(1.3, dur) * t) + 0.03 * np.sin(2 * np.pi * loop_freq(3.7, dur) * t)
    save("S_Hum", x, 0.7, loop=True)


def s_amb_l0():
    dur = 10.0
    t = t_axis(dur)
    room = fft_filter(white(dur), lo=30, hi=300, circular=True, slope=2)
    room /= np.std(room)
    hum = sum(a * np.sin(2 * np.pi * 120 * k * t) for k, a in [(2, 0.2), (3, 0.08), (4, 0.06)])
    air = fft_filter(white(dur), lo=800, hi=3000, circular=True)
    air /= np.std(air)
    x = 0.6 * room + hum + 0.05 * air
    save("S_Amb_L0", x, 0.5, loop=True)


def drips(buf, count, dur, pitch=(1200, 2600), circular=True, gain=0.5):
    for _ in range(count):
        n = int(0.12 * SR)
        f = RNG.uniform(*pitch)
        tt = np.arange(n) / SR
        d = np.sin(2 * np.pi * (f + 900 * np.exp(-tt * 40)) * tt) * np.exp(-tt * 35)
        place(buf, d * gain * RNG.uniform(0.4, 1.0), int(RNG.uniform(0, dur) * SR), circular)


def s_amb_industrial():
    dur = 12.0
    t = t_axis(dur)
    rumble = fft_filter(white(dur), lo=20, hi=160, circular=True)
    rumble /= np.std(rumble)
    tone = 0.25 * np.sin(2 * np.pi * loop_freq(55, dur) * t) + 0.12 * np.sin(2 * np.pi * loop_freq(110.5, dur) * t)
    x = 0.8 * rumble + tone
    drips(x, 9, dur, gain=0.35)
    x = reverb_loop(x, 1.6, 0.4)
    save("S_Amb_Industrial", x, 0.55, loop=True)


def metal_hit(dur=1.2, base=180):
    n = int(dur * SR)
    tt = np.arange(n) / SR
    ratios = [1.0, 2.76, 5.40, 8.93, 13.34]
    s = sum(np.sin(2 * np.pi * base * r * tt + RNG.random() * 6) * np.exp(-tt * (3 + r)) / (1 + r * 0.3) for r in ratios)
    return s


def s_amb_machinery():
    dur = 8.0
    t = t_axis(dur)
    x = 0.4 * fft_filter(white(dur), lo=30, hi=250, circular=True)
    x /= np.std(x)
    x *= 0.5
    x += 0.3 * np.sin(2 * np.pi * 50 * t) + 0.15 * np.sin(2 * np.pi * 100 * t)
    for i in range(8):  # coups reguliers
        n = int(0.4 * SR)
        tt = np.arange(n) / SR
        thump = np.sin(2 * np.pi * (45 + 60 * np.exp(-tt * 20)) * tt) * np.exp(-tt * 9)
        place(x, thump * 1.2, int(i * SR))
    for _ in range(5):
        place(x, metal_hit(1.0, RNG.uniform(150, 420)) * 0.25, int(RNG.uniform(0, dur) * SR))
    x = reverb_loop(x, 1.4, 0.35)
    save("S_Amb_Machinery", x, 0.6, loop=True)


def s_amb_hotel():
    dur = 16.0
    t = t_axis(dur)
    x = np.zeros_like(t)
    for f, a in [(110, 0.5), (130.8, 0.35), (164.8, 0.3), (196, 0.15)]:
        for det in (-0.4, 0.4):
            ff = loop_freq(f + det, dur)
            x += a * np.sin(2 * np.pi * ff * t + RNG.random() * 6)
    x *= 0.55 + 0.45 * np.sin(2 * np.pi * loop_freq(0.125, dur) * t)
    x = fft_filter(x, hi=900, circular=True)
    room = fft_filter(white(dur), lo=40, hi=400, circular=True)
    x = x / np.std(x) * 0.5 + room / np.std(room) * 0.25
    x = reverb_loop(x, 2.5, 0.5)
    save("S_Amb_Hotel", x, 0.45, loop=True)


def whisper_burst(dur=0.6):
    n = int(dur * SR)
    src = RNG.standard_normal(n)
    vowels = [[300, 870, 2240], [530, 1840, 2480], [400, 2000, 2550], [640, 1190, 2390]]
    v = vowels[RNG.integers(len(vowels))]
    s = resonate(src, v, q=8, gains=[1, 0.7, 0.4])
    s += 0.4 * fft_filter(src, lo=3000, hi=8000)
    e = np.sin(np.linspace(0, np.pi, n)) ** 1.5
    return s * e


def s_amb_dark():
    dur = 12.0
    t = t_axis(dur)
    x = fft_filter(white(dur), lo=18, hi=90, circular=True)
    x /= np.std(x)
    x *= 0.7 * (0.7 + 0.3 * np.sin(2 * np.pi * loop_freq(0.25, dur) * t))
    for _ in range(4):
        w = whisper_burst(RNG.uniform(0.4, 0.9))
        place(x, w / (np.std(w) + 1e-9) * 0.06, int(RNG.uniform(0, dur) * SR))
    x = reverb_loop(x, 2.0, 0.4)
    save("S_Amb_Dark", x, 0.6, loop=True)


def wind(dur, lo=150, hi=1200, gust=0.5):
    t = t_axis(dur)
    x = np.zeros_like(t)
    bands = [(lo, lo * 2), (lo * 2, lo * 4), (lo * 4, hi)]
    for i, (a, b) in enumerate(bands):
        n = fft_filter(white(dur), lo=a, hi=b, circular=True)
        n /= np.std(n)
        lfo = 0.5 + 0.5 * np.sin(2 * np.pi * loop_freq(0.1 + 0.07 * i, dur) * t + RNG.random() * 6)
        x += n * (1 - gust + gust * lfo) / (1 + i)
    return x


def s_amb_cave():
    dur = 12.0
    x = wind(12.0, 80, 600, 0.6) * 0.5
    drips(x, 12, dur, (900, 2200), gain=0.6)
    x = reverb_loop(x, 2.8, 0.55)
    save("S_Amb_Cave", x, 0.55, loop=True)


def s_amb_night():
    dur = 8.0
    t = t_axis(dur)
    x = wind(dur, 100, 900, 0.4) * 0.25
    for _ in range(3):
        f = RNG.uniform(4200, 5200)
        rate = RNG.uniform(14, 22)
        chirp = np.sin(2 * np.pi * f * t) * (np.sin(2 * np.pi * loop_freq(rate, dur) * t) > 0.6)
        gate = (np.sin(2 * np.pi * loop_freq(RNG.uniform(0.3, 0.6), dur) * t + RNG.random() * 6) > 0.1)
        x += chirp * gate * RNG.uniform(0.05, 0.12)
    save("S_Amb_Night", x, 0.5, loop=True)


def s_amb_wind():
    x = wind(12.0, 180, 2500, 0.7)
    rustle = fft_filter(white(12.0), lo=3000, hi=9000, circular=True)
    x = x + 0.15 * rustle / np.std(rustle) * (0.5 + 0.5 * x / np.max(np.abs(x)))
    save("S_Amb_Wind", x, 0.6, loop=True)


def s_amb_city():
    dur = 12.0
    t = t_axis(dur)
    x = fft_filter(white(dur), lo=25, hi=350, circular=True)
    x /= np.std(x)
    x *= 0.6 + 0.4 * np.sin(2 * np.pi * loop_freq(0.17, dur) * t)
    x += wind(dur, 200, 1500, 0.5) * 0.25
    save("S_Amb_City", x, 0.5, loop=True)


def s_amb_pool():
    dur = 12.0
    t = t_axis(dur)
    lap = fft_filter(white(dur), lo=150, hi=900, circular=True)
    lap /= np.std(lap)
    mod = np.zeros_like(t)
    for k in range(5):
        mod += np.maximum(0, np.sin(2 * np.pi * loop_freq(RNG.uniform(0.2, 0.7), dur) * t + RNG.random() * 6)) ** 3
    x = lap * (0.2 + mod / 5)
    drips(x, 10, dur, (700, 1600), gain=1.2)
    x = reverb_loop(x, 3.5, 0.65)
    save("S_Amb_Pool", x, 0.5, loop=True)


# ---------------------------------------------------------------------------
# Pas
# ---------------------------------------------------------------------------
def step(kind, i):
    dur = 0.35
    n = int(dur * SR)
    tt = np.arange(n) / SR
    src = RNG.standard_normal(n)
    if kind == "Carpet":
        thump = fft_filter(src, lo=40, hi=500) * np.exp(-tt * 30)
        squish = fft_filter(RNG.standard_normal(n), lo=400, hi=1800) * env(n, 0.02, 0.15)
        crackle = (RNG.random(n) < 0.004) * RNG.standard_normal(n) * np.exp(-tt * 12)
        x = thump * 1.0 + squish * 0.5 + fft_filter(crackle, lo=1500, hi=5000) * 0.6
    elif kind == "Hard":
        click = fft_filter(src, lo=900, hi=5000) * np.exp(-tt * 70)
        thump = np.sin(2 * np.pi * (90 + 80 * np.exp(-tt * 40)) * tt) * np.exp(-tt * 35)
        x = click * 0.8 + thump * 0.6
        x = reverb(x, 0.6, 0.25)[:n]
    elif kind == "Water":
        splash = fft_filter(src, lo=400, hi=4000) * env(n, 0.01, 0.25)
        bub = sum(np.sin(2 * np.pi * RNG.uniform(400, 1200) * (1 + tt * 3) * tt) * np.exp(-tt * RNG.uniform(15, 30)) for _ in range(4))
        x = splash + bub * 0.3
    else:  # Grass
        x = fft_filter(src, lo=2000, hi=8000) * env(n, 0.03, 0.2) * (0.6 + 0.4 * (RNG.random(n) > 0.5))
        x += fft_filter(RNG.standard_normal(n), lo=60, hi=300) * np.exp(-tt * 25) * 0.6
    save(f"S_Step_{kind}_{i}", x, 0.8)


# ---------------------------------------------------------------------------
# Effets
# ---------------------------------------------------------------------------
def s_flicker():
    dur = 0.6
    n = int(dur * SR)
    tt = np.arange(n) / SR
    buzz = saw(np.full(n, 120.0)) * 0.5 + fft_filter(RNG.standard_normal(n), lo=2000, hi=8000) * 0.4
    gate = (RNG.random(n // 400 + 1) > 0.45).repeat(400)[:n]
    crack = (RNG.random(n) < 0.002) * RNG.standard_normal(n) * 6
    save("S_Flicker", buzz * gate + crack, 0.7)


def s_flashlight():
    n = int(0.25 * SR)
    x = np.zeros(n)
    for off, f in [(0, 3200), (int(0.07 * SR), 2600)]:
        m = int(0.02 * SR)
        tt = np.arange(m) / SR
        c = fft_filter(RNG.standard_normal(m), lo=f, hi=f * 2.5) * np.exp(-tt * 300)
        x[off:off + m] += c
    save("S_Flashlight", x, 0.7)


def s_pickup():
    n = int(0.4 * SR)
    tt = np.arange(n) / SR
    rustle = fft_filter(RNG.standard_normal(n), lo=1500, hi=6000) * env(n, 0.01, 0.2)
    tone = np.sin(2 * np.pi * 660 * tt) * np.exp(-tt * 10) * 0.3 + np.sin(2 * np.pi * 990 * tt) * np.exp(-tt * 12) * 0.2
    save("S_Pickup", rustle * 0.5 + tone, 0.6)


def s_drink():
    dur = 1.6
    x = np.zeros(int(dur * SR))
    for i in range(4):
        m = int(0.22 * SR)
        tt = np.arange(m) / SR
        g = resonate(RNG.standard_normal(m), [180, 420, 900], q=6) * env(m, 0.03, 0.18)
        place(x, g, int((0.1 + i * 0.33) * SR), circular=False)
    save("S_Drink", x, 0.7)


def s_battery():
    n = int(0.5 * SR)
    x = np.zeros(n)
    for off in (0, int(0.18 * SR), int(0.3 * SR)):
        m = int(0.03 * SR)
        tt = np.arange(m) / SR
        place(x, fft_filter(RNG.standard_normal(m), lo=1500, hi=6000) * np.exp(-tt * 200), off, False)
    save("S_Battery", x, 0.7)


def s_noclip():
    dur = 2.6
    n = int(dur * SR)
    tt = np.arange(n) / SR
    swell = fft_filter(RNG.standard_normal(n), lo=200, hi=6000) * (tt / dur) ** 2
    sweep = np.sin(2 * np.pi * np.cumsum(80 + 900 * (tt / dur) ** 3) / SR) * (tt / dur)
    glitch = np.sign(np.sin(2 * np.pi * np.cumsum(RNG.choice([220, 440, 880, 1760], n // 1600 + 1).repeat(1600)[:n]) / SR))
    glitch *= (RNG.random(n // 800 + 1) > 0.5).repeat(800)[:n] * (tt > dur * 0.5)
    x = swell * 0.6 + sweep * 0.5 + glitch * 0.2
    x = np.round(x * 16) / 16  # "bitcrush"
    x[int(n * 0.97):] = 0
    save("S_Noclip", reverb(x, 1.5, 0.3), 0.85)


def s_door():
    dur = 1.6
    n = int(dur * SR)
    tt = np.arange(n) / SR
    f = 260 + 140 * np.sin(2 * np.pi * 1.3 * tt) + 40 * RNG.standard_normal(n).cumsum() / SR
    creak = saw(np.abs(f)) * (RNG.random(n) * 0.5 + 0.5)
    creak = resonate(creak, [700, 1500, 2600], q=10) * env(n, 0.15, 1.2)
    latch = np.zeros(n)
    m = int(0.04 * SR)
    place(latch, fft_filter(RNG.standard_normal(m), lo=800, hi=4000) * np.exp(-np.arange(m) / SR * 120) * 3, 0, False)
    save("S_Door", reverb(creak + latch, 0.9, 0.25), 0.8)


def s_death():
    dur = 2.2
    n = int(dur * SR)
    tt = np.arange(n) / SR
    x = np.zeros(n)
    for k in range(6):
        f = (300 + 120 * k) * (1 + 0.6 * np.exp(-tt * 2)) * (1 + 0.02 * np.sin(2 * np.pi * 7 * tt))
        x += saw(f) / (k + 1)
    x += RNG.standard_normal(n) * 0.8
    x = np.tanh(x * 3)
    x = resonate(x, [800, 1300, 2800], q=4) + 0.3 * x
    x *= env(n, 0.005, 0.1, 0.8, 1.2, 0.8)
    save("S_Death", reverb(x, 1.2, 0.3), 0.95)


def s_hurt():
    n = int(0.5 * SR)
    tt = np.arange(n) / SR
    thud = np.sin(2 * np.pi * (60 + 100 * np.exp(-tt * 30)) * tt) * np.exp(-tt * 12)
    x = thud + fft_filter(RNG.standard_normal(n), lo=200, hi=2000) * np.exp(-tt * 20) * 0.6
    save("S_Hurt", x, 0.85)


def s_heartbeat():
    dur = 1.6
    x = np.zeros(int(dur * SR))
    for off, a in [(0.0, 1.0), (0.28, 0.7)]:
        m = int(0.25 * SR)
        tt = np.arange(m) / SR
        b = np.sin(2 * np.pi * (45 + 30 * np.exp(-tt * 25)) * tt) * np.exp(-tt * 18)
        place(x, b * a, int(off * SR))
    save("S_Heartbeat", x, 0.9, loop=True)


def s_breath():
    dur = 3.0
    t = t_axis(dur)
    src = fft_filter(white(dur), lo=300, hi=4000, circular=True)
    e_in = np.maximum(0, np.sin(2 * np.pi * t / dur * 2)) ** 1.5
    x = resonate(src, [600, 1400, 2600], q=3, circular=True) * e_in
    save("S_Breath", x, 0.6, loop=True)


def s_whisper():
    dur = 2.5
    x = np.zeros(int(dur * SR))
    for i in range(5):
        w = whisper_burst(RNG.uniform(0.25, 0.5))
        place(x, w, int((0.1 + i * 0.42) * SR), False)
    save("S_Whisper", reverb(x, 1.5, 0.4), 0.6)


def s_distant_steps():
    dur = 4.0
    x = np.zeros(int(dur * SR))
    for i in range(7):
        m = int(0.2 * SR)
        tt = np.arange(m) / SR
        s = fft_filter(RNG.standard_normal(m), lo=60, hi=600) * np.exp(-tt * 30)
        place(x, s * (0.6 + 0.4 * i / 7), int((0.2 + i * 0.5) * SR), False)
    save("S_DistantSteps", reverb(fft_filter(x, hi=900), 2.0, 0.6), 0.6)


def s_chase():
    dur = 4.0
    t = t_axis(dur)
    x = np.zeros_like(t)
    for f in (55, 58.3, 82.4, 87.3):
        x += saw(np.full(len(t), loop_freq(f, dur))) * 0.25
    x = fft_filter(x, hi=1500, circular=True)
    x *= 0.6 + 0.4 * np.sin(2 * np.pi * 8 * t) ** 2
    for i in range(8):
        m = int(0.3 * SR)
        tt = np.arange(m) / SR
        place(x, np.sin(2 * np.pi * (40 + 50 * np.exp(-tt * 30)) * tt) * np.exp(-tt * 10) * 1.5, int(i * 0.5 * SR))
    save("S_Chase", x, 0.75, loop=True)


def s_exit_hum():
    dur = 3.0
    t = t_axis(dur)
    st = fft_filter(white(dur), lo=1000, hi=9000, circular=True)
    st /= np.std(st)
    tone = np.sin(2 * np.pi * loop_freq(220, dur) * t + 3 * np.sin(2 * np.pi * loop_freq(5, dur) * t))
    x = 0.35 * st * (0.6 + 0.4 * np.sin(2 * np.pi * loop_freq(11, dur) * t)) + 0.4 * tone
    save("S_ExitHum", x, 0.6, loop=True)


def s_alert():
    dur = 2.5
    n = int(dur * SR)
    tt = np.arange(n) / SR
    x = np.zeros(n)
    for f in (233, 247, 330, 349, 466):
        x += saw(np.full(n, f) * (1 + 0.01 * np.sin(2 * np.pi * 6 * tt))) * 0.2
    x = fft_filter(x, hi=3000) * exp_env(n, 0.6)
    x += np.sin(2 * np.pi * (40 + 80 * np.exp(-tt * 20)) * tt) * np.exp(-tt * 4) * 1.2
    save("S_Alert", reverb(x, 1.8, 0.35), 0.9)


# ---------------------------------------------------------------------------
# Entites
# ---------------------------------------------------------------------------
def voice(dur, f0_curve, vowel_seq, breath=0.15, rough=0.0):
    n = int(dur * SR)
    tt = np.arange(n) / SR
    f0 = f0_curve(tt) * (1 + rough * RNG.standard_normal(n).cumsum() / np.sqrt(n) * 0.5)
    src = saw(np.abs(f0)) + breath * RNG.standard_normal(n)
    seg = n // len(vowel_seq)
    out = np.zeros(n)
    for i, v in enumerate(vowel_seq):
        a, b = i * seg, min(n, (i + 1) * seg + seg // 3)
        part = resonate(src[a:b], v, q=9, gains=[1.0, 0.6, 0.35])
        w = np.sin(np.linspace(0, np.pi, b - a)) ** 0.7
        out[a:b] += part * w
    return out


VOW = {"a": [730, 1090, 2440], "e": [530, 1840, 2480], "i": [270, 2290, 3010], "o": [570, 840, 2410],
       "u": [300, 870, 2240], "eh": [660, 1700, 2400]}


def s_smiler():
    dur = 2.4
    x = np.zeros(int(dur * SR))
    for i in range(9):
        v = voice(0.13, lambda t: 420 + 200 * RNG.random() + 0 * t, [VOW["i"]], breath=0.3)
        place(x, v, int((0.1 + i * 0.16 + RNG.uniform(0, 0.04)) * SR), False)
    x = x * (1 + 0.5 * np.sin(np.linspace(0, 60, len(x))))
    save("S_Smiler", reverb(x, 2.2, 0.5), 0.75)


def s_hound():
    dur = 2.2
    n = int(dur * SR)
    tt = np.arange(n) / SR
    f0 = 70 + 15 * np.sin(2 * np.pi * 3 * tt) + 8 * RNG.standard_normal(n).cumsum() / SR
    src = saw(f0) * (0.6 + 0.4 * (np.sin(2 * np.pi * 30 * tt) > 0)) + 0.5 * RNG.standard_normal(n)
    x = resonate(src, [350, 900, 2200], q=5) * env(n, 0.2, 0.3, 0.8, 0.7, 1.0)
    x = np.tanh(x / np.std(x) * 1.5)
    save("S_Hound", reverb(x, 0.9, 0.25), 0.85)


def s_skinstealer():
    seq = [VOW[k] for k in ("eh", "o", "a", "i", "u", "eh")]
    x = voice(2.4, lambda t: 140 + 40 * np.sin(2 * np.pi * 1.7 * t) + 25 * np.sin(2 * np.pi * 9 * t), seq, breath=0.25, rough=0.8)
    x = x * (np.sin(np.linspace(0, 40, len(x))) > -0.6)
    save("S_SkinStealer", reverb(x, 1.4, 0.35), 0.8)


def s_faceling():
    seq = [VOW[k] for k in ("u", "o", "u", "a", "u")]
    x = voice(2.0, lambda t: 110 + 10 * np.sin(2 * np.pi * 2 * t), seq, breath=0.4)
    save("S_Faceling", reverb(fft_filter(x, hi=1500), 1.2, 0.4), 0.6)


def s_moth():
    dur = 1.0
    t = t_axis(dur)
    x = fft_filter(white(dur), lo=80, hi=900, circular=True)
    x *= (0.5 + 0.5 * np.sin(2 * np.pi * 22 * t)) ** 2
    save("S_Moth", x, 0.7, loop=True)


def s_wretch():
    seq = [VOW[k] for k in ("a", "o", "u")]
    x = voice(2.4, lambda t: 95 - 25 * t, seq, breath=0.5, rough=1.2)
    x = np.tanh(x / np.std(x))
    save("S_Wretch", reverb(x, 1.3, 0.35), 0.75)


def s_partygoer():
    dur = 2.6
    x = np.zeros(int(dur * SR))
    for i in range(5):
        v = voice(0.16, lambda t: 520 + 300 * t, [VOW["eh"]], breath=0.2)
        place(x, v, int((0.05 + i * 0.2) * SR), False)
    n = int(0.9 * SR)
    tt = np.arange(n) / SR
    horn = saw(np.full(n, 440) * (1 + 0.04 * np.sin(2 * np.pi * 5 * tt))) * env(n, 0.05, 0.2, 0.6, 0.3, 0.3)
    place(x, resonate(horn, [900, 1800, 3000], q=5) * 0.5, int(1.3 * SR), False)
    save("S_Partygoer", reverb(x, 1.5, 0.35), 0.8)


def s_clump():
    dur = 1.6
    x = np.zeros(int(dur * SR))
    for i in range(6):
        m = int(0.18 * SR)
        tt = np.arange(m) / SR
        sq = resonate(RNG.standard_normal(m), [RNG.uniform(200, 500), RNG.uniform(900, 1500)], q=7) * env(m, 0.02, 0.15)
        place(x, sq, int(RNG.uniform(0, dur - 0.2) * SR), False)
    save("S_Clump", reverb(x, 0.8, 0.3), 0.75)


# ---------------------------------------------------------------------------
# v2 : evenements, interface, objets
# ---------------------------------------------------------------------------
def s_blackout():
    dur = 2.4
    n = int(dur * SR)
    tt = np.arange(n) / SR
    f = 120 * np.exp(-tt * 1.6) + 20
    whir = (saw(f) * 0.6 + np.sin(2 * np.pi * np.cumsum(f * 2) / SR) * 0.3) * np.exp(-tt * 1.2)
    clunk = np.zeros(n)
    m = int(0.35 * SR)
    t2 = np.arange(m) / SR
    clunk[:m] = np.sin(2 * np.pi * (50 + 120 * np.exp(-t2 * 25)) * t2) * np.exp(-t2 * 9) * 1.4
    clunk[:m] += fft_filter(RNG.standard_normal(m), lo=300, hi=3000) * np.exp(-t2 * 30) * 0.8
    save("S_Blackout", reverb(whir + clunk, 2.0, 0.4), 0.95)


def s_power_up():
    dur = 2.2
    n = int(dur * SR)
    tt = np.arange(n) / SR
    x = np.zeros(n)
    m = int(0.3 * SR)
    t2 = np.arange(m) / SR
    x[:m] = np.sin(2 * np.pi * (60 + 100 * np.exp(-t2 * 30)) * t2) * np.exp(-t2 * 10)
    hum = sum(a * np.sin(2 * np.pi * 120 * k * tt) for k, a in [(1, 0.5), (2, 0.3), (3, 0.2)])
    x += hum * np.clip((tt - 0.2) / 1.2, 0, 1) * 0.5
    for k in range(5):  # tubes qui s'allument un par un
        place(x, fft_filter(RNG.standard_normal(int(0.05 * SR)), lo=1500, hi=6000) * 0.4, int((0.4 + k * 0.25) * SR), False)
    save("S_PowerUp", reverb(x, 1.5, 0.35), 0.85)


def s_rec_beep():
    n = int(0.35 * SR)
    tt = np.arange(n) / SR
    x = np.sin(2 * np.pi * 2000 * tt) * ((tt < 0.08) | ((tt > 0.14) & (tt < 0.22)))
    save("S_RecBeep", x * 0.5, 0.6)


def s_night_vision():
    dur = 1.2
    n = int(dur * SR)
    tt = np.arange(n) / SR
    f = 2000 + 7000 * (1 - np.exp(-tt * 3))
    x = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-tt * 2.5) * 0.4
    x += fft_filter(RNG.standard_normal(n), lo=4000, hi=9000) * np.exp(-tt * 4) * 0.1
    save("S_NightVision", x, 0.5)


def s_ui_click():
    n = int(0.08 * SR)
    tt = np.arange(n) / SR
    x = np.sin(2 * np.pi * 1400 * tt) * np.exp(-tt * 80) + fft_filter(RNG.standard_normal(n), lo=2000, hi=8000) * np.exp(-tt * 150) * 0.3
    save("S_UIClick", x, 0.5)


def s_item_move():
    n = int(0.25 * SR)
    tt = np.arange(n) / SR
    x = fft_filter(RNG.standard_normal(n), lo=300, hi=3000) * env(n, 0.01, 0.15) + np.sin(2 * np.pi * 180 * tt) * np.exp(-tt * 30) * 0.4
    save("S_ItemMove", x, 0.55)


def s_bandage():
    dur = 1.3
    n = int(dur * SR)
    x = np.zeros(n)
    for i in range(3):
        m = int(0.28 * SR)
        rip = fft_filter(RNG.standard_normal(m), lo=1500, hi=7000) * env(m, 0.02, 0.25) * (RNG.random(m) > 0.3)
        place(x, rip, int((0.05 + i * 0.4) * SR), False)
    save("S_Bandage", x, 0.6)


def s_eat():
    dur = 1.4
    n = int(dur * SR)
    x = np.zeros(n)
    for i in range(4):
        m = int(0.12 * SR)
        tt = np.arange(m) / SR
        c = fft_filter(RNG.standard_normal(m), lo=500, hi=5000) * np.exp(-tt * 40) * (RNG.random(m) > 0.5)
        place(x, c, int((0.1 + i * 0.3) * SR), False)
    save("S_Eat", x, 0.6)


def s_inventory():
    n = int(0.6 * SR)
    tt = np.arange(n) / SR
    x = fft_filter(RNG.standard_normal(n), lo=400, hi=4000) * env(n, 0.05, 0.4) * (0.5 + 0.5 * np.sin(2 * np.pi * 18 * tt))
    save("S_Inventory", x, 0.5)


def s_objective():
    n = int(1.4 * SR)
    tt = np.arange(n) / SR
    x = np.zeros(n)
    for k, f in enumerate((523.25, 659.25, 783.99)):
        seg = (tt > k * 0.12)
        x += np.sin(2 * np.pi * f * tt) * seg * np.exp(-np.maximum(0, tt - k * 0.12) * 3.0) * 0.4
    save("S_Objective", reverb(x, 1.2, 0.3), 0.7)


def s_bacteria():
    dur = 2.6
    n = int(dur * SR)
    tt = np.arange(n) / SR
    f0 = 180 + 120 * np.sin(2 * np.pi * 0.7 * tt) + 60 * RNG.standard_normal(n).cumsum() / SR * 3
    scream = saw(np.abs(f0)) + saw(np.abs(f0) * 1.51) * 0.5
    grind = fft_filter(RNG.standard_normal(n), lo=200, hi=2500) * (np.sin(2 * np.pi * 37 * tt) > 0)
    x = np.tanh((resonate(scream, [900, 1700, 3100], q=6) + grind * 0.6) * 2.5) * env(n, 0.05, 0.3, 0.8, 1.0, 1.2)
    x = np.round(x * 24) / 24
    save("S_Bacteria", reverb(x, 1.8, 0.4), 0.9)


# ---------------------------------------------------------------------------
# v3 : eau (nage, plongeon, apnee). Generateur aleatoire propre a chaque son
# pour ne pas modifier les sons precedents.
# ---------------------------------------------------------------------------
def bubbles(n, count, rng, pitch=(300, 1400), gain=0.4):
    """Petites bulles : sinusoides qui montent en frequence et s'eteignent vite"""
    x = np.zeros(n)
    for _ in range(count):
        start = int(rng.random() * n * 0.85)
        ln = int(rng.uniform(0.02, 0.09) * SR)
        tt = np.arange(min(ln, n - start)) / SR
        f = rng.uniform(*pitch)
        x[start:start + len(tt)] += np.sin(2 * np.pi * f * (1 + tt * rng.uniform(4, 12)) * tt) * np.exp(-tt * rng.uniform(40, 90)) * gain
    return x


def s_splash():
    rng = np.random.default_rng(3701)
    dur = 1.3
    n = int(dur * SR)
    tt = np.arange(n) / SR
    crash = fft_filter(rng.standard_normal(n), lo=250, hi=7000) * env(n, 0.004, 0.35)
    body = fft_filter(rng.standard_normal(n), lo=60, hi=500) * np.exp(-tt * 9)
    spray = fft_filter(rng.standard_normal(n), lo=2500, hi=9000) * np.exp(-tt * 5) * (rng.random(n) < 0.08)
    x = crash * 0.9 + body * 0.8 + spray * 0.5 + bubbles(n, 26, rng, (350, 1500), 0.5)
    save("S_Splash", reverb(x, 1.6, 0.35), 0.85)


def s_swim():
    rng = np.random.default_rng(3702)
    dur = 0.9
    n = int(dur * SR)
    tt = np.arange(n) / SR
    # brasse : l'eau poussee par les bras (souffle filtre qui gonfle puis retombe) + clapotis
    push = fft_filter(rng.standard_normal(n), lo=180, hi=1600) * np.sin(np.pi * np.clip(tt / 0.6, 0, 1)) ** 2
    lap = fft_filter(rng.standard_normal(n), lo=900, hi=4500) * env(n, 0.01, 0.18) * 0.4
    x = push + np.roll(lap, int(0.45 * SR)) + bubbles(n, 8, rng, (500, 1300), 0.25)
    save("S_Swim", reverb(x, 1.4, 0.3), 0.7)


def s_underwater():
    rng = np.random.default_rng(3703)
    dur = 10.0
    n = int(dur * SR)
    t = t_axis(dur)
    # grondement sourd, pression dans les oreilles, bulles lointaines (boucle parfaite)
    rumble = fft_filter(rng.standard_normal(n), lo=25, hi=320, circular=True)
    rumble /= np.std(rumble)
    hum = np.sin(2 * np.pi * loop_freq(58, dur) * t) * 0.15 + np.sin(2 * np.pi * loop_freq(116.5, dur) * t) * 0.05
    swell = 0.7 + 0.3 * np.sin(2 * np.pi * loop_freq(0.15, dur) * t)
    x = rumble * swell * 0.6 + hum
    b = fft_filter(bubbles(n, 40, rng, (250, 900), 0.6), hi=1400)
    x = x + np.roll(b, 0)
    save("S_Underwater", reverb_loop(x, 2.0, 0.4), 0.55, loop=True)


def s_gasp():
    rng = np.random.default_rng(3704)
    dur = 0.9
    n = int(dur * SR)
    tt = np.arange(n) / SR
    # inspiration brusque (souffle aspire) puis petite toux d'eau
    inhale = fft_filter(rng.standard_normal(n), lo=600, hi=5000) * np.clip(tt / 0.35, 0, 1) ** 1.5 * (tt < 0.45)
    inhale = resonate(inhale, [850, 1300, 2600], q=5) * 0.5 + inhale * 0.5
    cough_t = tt - 0.55
    cough = fft_filter(rng.standard_normal(n), lo=200, hi=2200) * np.exp(-np.maximum(cough_t, 0) * 18) * (cough_t > 0)
    x = inhale + cough * 0.6
    save("S_Gasp", reverb(x, 0.8, 0.2), 0.75)


WATER_V3 = (s_splash, s_swim, s_underwater, s_gasp)


if __name__ == "__main__":
    print("Synthese des sons dans", os.path.abspath(OUT))
    s_hum()
    s_amb_l0()
    s_amb_industrial()
    s_amb_machinery()
    s_amb_hotel()
    s_amb_dark()
    s_amb_cave()
    s_amb_night()
    s_amb_wind()
    s_amb_city()
    s_amb_pool()
    for k in ("Carpet", "Hard", "Water", "Grass"):
        for i in range(1, 5):
            step(k, i)
    s_flicker()
    s_flashlight()
    s_pickup()
    s_drink()
    s_battery()
    s_noclip()
    s_door()
    s_death()
    s_hurt()
    s_heartbeat()
    s_breath()
    s_whisper()
    s_distant_steps()
    s_chase()
    s_exit_hum()
    s_alert()
    s_smiler()
    s_hound()
    s_skinstealer()
    s_faceling()
    s_moth()
    s_wretch()
    s_partygoer()
    s_clump()
    s_blackout()
    s_power_up()
    s_rec_beep()
    s_night_vision()
    s_ui_click()
    s_item_move()
    s_bandage()
    s_eat()
    s_inventory()
    s_objective()
    s_bacteria()
    for fn in WATER_V3:
        fn()
    with open(os.path.join(OUT, "loops.txt"), "w") as f:
        f.write("\n".join(sorted(LOOPS)) + "\n")
    print("Termine. Boucles :", ", ".join(sorted(LOOPS)))
