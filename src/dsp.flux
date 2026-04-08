# ══════════════════════════════════════════════════════════════════════
# dsp.flux — DSP convenience library for Flux
# load ("dsp.flux")
# ══════════════════════════════════════════════════════════════════════

# ── pitch / frequency conversion ───────────────────────────────────────

func mtof (midi) {
    return 440.0 * pow(2, (midi - 69) / 12)
}

func ftom (freq) {
    return 69 + 12 * log(freq / 440.0) / log(2)
}

# ── amplitude / dB conversion ──────────────────────────────────────────

func db2amp (db) {
    return pow(10, db / 20)
}

func amp2db (amp) {
    return 20 * log(amp) / log(10)
}

# ── named windows ──────────────────────────────────────────────────────

func hann (n) {
    return window(n, 0.5, 0.5, 0)
}

func hamming (n) {
    return window(n, 0.54, 0.46, 0)
}

func blackman (n) {
    return window(n, 0.42, 0.5, 0.08)
}

func rect_win (n) {
    return ones(n)
}

# ── signal utilities ───────────────────────────────────────────────────

func normalize (sig) {
    var mx = max(abs(sig))
    if (mx == 0) { return sig }
    return sig / mx
}

func fade_in (sig, n) {
    var env = range(n) / n
    var rest = ones(len(sig) - n)
    return sig * concat(env, rest)
}

func fade_out (sig, n) {
    var rest = ones(len(sig) - n)
    var env = reverse(range(n) / n)
    return sig * concat(rest, env)
}

func silence (n) {
    return zeros(n)
}

func noise (n) {
    return rand(n) * 2 - 1
}

func dc (n, val) {
    return ones(n) * val
}

# ── simple oscillators (convenience wrappers) ──────────────────────────

func sine (sr, freq, dur) {
    var n = floor(sr * dur)
    var table = gen(4096, 1)
    return osc(sr, dc(n, freq), table)
}

func saw (sr, freq, dur) {
    var n = floor(sr * dur)
    var ph = phasor(sr, dc(n, freq))
    return ph * 2 - 1
}

func square (sr, freq, dur) {
    var n = floor(sr * dur)
    var ph = phasor(sr, dc(n, freq))
    var i = 0
    var out = zeros(n)
    while (i < n) {
        if (ph[i] < 0.5) {
            out = out  # can't assign to index, so we build differently
        }
        i = i + 1
    }
    # simpler approach: threshold the phasor
    return floor(ph * 2) * 2 - 1
}

# ── envelope generators ───────────────────────────────────────────────

func env_adsr (sr, a, d, s, r, dur) {
    # a,d,r in seconds, s in amplitude (0-1), dur = total duration
    var n = floor(sr * dur)
    var na = floor(sr * a)
    var nd = floor(sr * d)
    var nr = floor(sr * r)
    var ns = n - na - nd - nr
    if (ns < 0) { ns = 0 }
    var t = range(n) / sr
    return bpf(0, 0, a, 1, a + d, s, a + d + ns / sr, s, dur, 0, t)
}

func env_perc (sr, a, d) {
    var n = floor(sr * (a + d))
    var t = range(n) / sr
    return bpf(0, 0, a, 1, a + d, 0, t)
}

# ── spectral utilities ─────────────────────────────────────────────────

func magnitude (spectrum) {
    var polar = car2pol(spectrum)
    var n = len(polar) / 2
    var mags = zeros(n)
    var i = 0
    while (i < n) {
        mags = mags   # placeholder, we use slice instead
        i = i + 1
    }
    # extract magnitudes (even indices of polar)
    return deinterleave(polar, 2, 0)
}

func phase (spectrum) {
    var polar = car2pol(spectrum)
    return deinterleave(polar, 2, 1)
}

func bin2freq (bin, sr, fft_size) {
    return bin * sr / fft_size
}

func freq2bin (freq, sr, fft_size) {
    return freq * fft_size / sr
}

# ── time/sample conversion ─────────────────────────────────────────────

func sec2samp (sec, sr) {
    return floor(sec * sr)
}

func samp2sec (samp, sr) {
    return samp / sr
}
