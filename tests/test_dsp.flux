# ══════════════════════════════════════════════════════════════════════
# test_dsp.flux — tests for DSP builtins (dsp.h) and dsp.flux
# ══════════════════════════════════════════════════════════════════════

load ("dsp.flux")

var pass = 0
var fail = 0

func check (name, cond) {
    if (cond) {
        pass = pass + 1
    } else {
        print "FAIL:" name
        fail = fail + 1
    }
}

func approx (a, b, tol) {
    return abs(a - b) < tol
}

# ── gen (wavetable generation) ─────────────────────────────────────────

var table = gen(1024, 1)
check("gen_length", len(table) == 1025)
check("gen_guard", approx(table[0], table[1024], 1e-10))
check("gen_zero_start", approx(table[0], 0, 1e-10))
check("gen_peak", max(table) > 0.9)

var table3 = gen(1024, 1, 0.5, 0.25)
check("gen_harmonics", len(table3) == 1025)

# ── osc (wavetable oscillator) ─────────────────────────────────────────

var sr = 44100
var dur_samp = 4410
var freq_vec = dc(dur_samp, 440)
var sig = osc(sr, freq_vec, table)
check("osc_length", len(sig) == dur_samp)
check("osc_range", max(sig) <= 1.01 and min(sig) >= -1.01)

# ── phasor ─────────────────────────────────────────────────────────────

var ph = phasor(sr, dc(sr, 1))
check("phasor_length", len(ph) == sr)
check("phasor_start", approx(ph[0], 0, 1e-10))
check("phasor_range", min(ph) >= 0 and max(ph) < 1)

# ── bpf (breakpoint function) ─────────────────────────────────────────

var t = range(100) / 100
var env = bpf(0, 0, 0.5, 1, 1, 0, t)
check("bpf_length", len(env) == 100)
check("bpf_start", approx(env[0], 0, 1e-10))
check("bpf_mid", approx(env[50], 1, 0.05))
check("bpf_end", approx(env[99], 0, 0.05))

# ── fft / ifft ─────────────────────────────────────────────────────────

var test_sig = zeros(64)
test_sig = test_sig + 0
var i = 0
while (i < 64) {
    i = i + 1
}
# use a simple signal
var simple = sin(range(64) * 2 * pi / 64)
var spectrum = fft(simple)
check("fft_length", len(spectrum) == 128)

var reconstructed = ifft(spectrum)
check("ifft_length", len(reconstructed) == 64)
var recon_err = max(abs(reconstructed - simple))
check("fft_ifft_roundtrip", recon_err < 1e-10)

# ── car2pol / pol2car ──────────────────────────────────────────────────

var polar = car2pol(spectrum)
check("car2pol_length", len(polar) == len(spectrum))
var back = pol2car(polar)
var pol_err = max(abs(back - spectrum))
check("car2pol_pol2car_roundtrip", pol_err < 1e-10)

# ── window ─────────────────────────────────────────────────────────────

var w = window(256, 0.5, 0.5, 0)
check("window_length", len(w) == 256)
check("window_edges", approx(w[0], 0, 1e-10))
check("window_center", approx(w[127], 1, 0.01))

var wh = hann(256)
check("hann", max(abs(wh - w)) < 1e-10)
var wm = hamming(256)
check("hamming_length", len(wm) == 256)
var wb = blackman(256)
check("blackman_length", len(wb) == 256)

# ── conv (convolution) ────────────────────────────────────────────────

var impulse = zeros(8)
# manually set impulse[0] = 1
var imp_data = [1, 0, 0, 0, 0, 0, 0, 0]
var test_vec = [1, 2, 3, 4]
var c = conv(test_vec, imp_data)
check("conv_length", len(c) == len(test_vec) + len(imp_data) - 1)
check("conv_identity", approx(c[0], 1, 1e-10))
check("conv_identity2", approx(c[1], 2, 1e-10))

# ── resample ───────────────────────────────────────────────────────────

var orig = sin(range(100) * 2 * pi / 100)
var up = resample(orig, 2)
check("resample_up", approx(len(up), 200, 1))
var down = resample(orig, 0.5)
check("resample_down", approx(len(down), 50, 1))

# ── delay ──────────────────────────────────────────────────────────────

var d_sig = [1, 2, 3, 4, 5]
var delayed = delay(d_sig, 2)
check("delay_length", len(delayed) == 5)
check("delay_start", approx(delayed[0], 0, 1e-10))
check("delay_shift", approx(delayed[2], 1, 0.1))

# ── comb filter ────────────────────────────────────────────────────────

var comb_in = concat([1], zeros(99))
var comb_out = comb(comb_in, 10, 0.5)
check("comb_length", len(comb_out) == 100)
check("comb_direct", approx(comb_out[0], 1, 1e-10))
check("comb_echo", approx(comb_out[10], 0.5, 1e-10))
check("comb_echo2", approx(comb_out[20], 0.25, 1e-10))

# ── allpass filter ─────────────────────────────────────────────────────

var ap_in = concat([1], zeros(49))
var ap_out = allpass(ap_in, 5, 0.7)
check("allpass_length", len(ap_out) == 50)

# ── reson ──────────────────────────────────────────────────────────────

var res_in = concat([1], zeros(999))
var res_out = reson(res_in, 44100, 440, 0.05)
check("reson_length", len(res_out) == floor(44100 * 0.05))

# ── iir ────────────────────────────────────────────────────────────────

var iir_in = concat([1], zeros(31))
var iir_out = iir(iir_in, [1, 0, 0], [1, -0.5, 0])
check("iir_length", len(iir_out) == 32)
check("iir_direct", approx(iir_out[0], 1, 1e-10))
check("iir_feedback", approx(iir_out[1], 0.5, 1e-10))

# ── iirdesign ──────────────────────────────────────────────────────────

var coeffs = iirdesign("lowpass", 44100, 1000, 0.707, 0)
check("iirdesign_is_list", type(coeffs) == "list")
check("iirdesign_b_len", len(coeffs[0]) == 3)
check("iirdesign_a_len", len(coeffs[1]) == 3)

var coeffs_hp = iirdesign("highpass", 44100, 1000, 0.707, 0)
check("iirdesign_hp", len(coeffs_hp[0]) == 3)

var coeffs_pk = iirdesign("peak", 44100, 1000, 2, 6)
check("iirdesign_peak", len(coeffs_pk[0]) == 3)

# ── mix ────────────────────────────────────────────────────────────────

var mix_out = mix(0, [1, 1, 1], 2, [2, 2, 2])
check("mix_length", len(mix_out) == 5)
check("mix_overlap", approx(mix_out[2], 3, 1e-10))
check("mix_tail", approx(mix_out[4], 2, 1e-10))

# ── wavwrite / wavread ────────────────────────────────────────────────

var wav_sig = sin(range(1000) * 2 * pi * 440 / 44100)
wavwrite(wav_sig, 44100, "/tmp/flux_test.wav")
var wav_data = wavread("/tmp/flux_test.wav")
check("wavread_is_list", type(wav_data) == "list")
check("wavread_sr", wav_data[1] == 44100)
check("wavread_nch", wav_data[2] == 1)
check("wavread_samples", len(wav_data[0]) == 1000)

# ── deinterleave / interleave ──────────────────────────────────────────

var stereo = interleave([1, 2, 3], [4, 5, 6])
check("interleave_length", len(stereo) == 6)
check("interleave_order", approx(stereo[0], 1, 1e-10))
check("interleave_order2", approx(stereo[1], 4, 1e-10))

var left = deinterleave(stereo, 2, 0)
var right = deinterleave(stereo, 2, 1)
check("deinterleave_left", max(abs(left - [1,2,3])) < 1e-10)
check("deinterleave_right", max(abs(right - [4,5,6])) < 1e-10)

# ── stft / istft ──────────────────────────────────────────────────────

var stft_sig = sin(range(1024) * 2 * pi * 10 / 1024)
var frames = stft(stft_sig, 256, 128)
check("stft_type", type(frames) == "list")
check("stft_frames", len(frames) > 0)
var istft_sig = istft(frames, 256, 128)
check("istft_length", len(istft_sig) > 0)

# ── oscbank ────────────────────────────────────────────────────────────

var bank_n = 4410
var amp1 = dc(bank_n, 0.5)
var amp2 = dc(bank_n, 0.3)
var freq1 = dc(bank_n, 440)
var freq2 = dc(bank_n, 880)
var bank_out = oscbank(44100, list(amp1, amp2), list(freq1, freq2), table)
check("oscbank_length", len(bank_out) == bank_n)
check("oscbank_range", max(abs(bank_out)) <= 1.01)

# ── dsp.flux convenience functions ─────────────────────────────────────

check("mtof_a4", approx(mtof(69), 440, 0.01))
check("mtof_a5", approx(mtof(81), 880, 0.01))
check("ftom_440", approx(ftom(440), 69, 0.01))
check("db2amp_0", approx(db2amp(0), 1, 1e-10))
check("db2amp_-6", approx(db2amp(-6), 0.5012, 0.01))
check("amp2db_1", approx(amp2db(1), 0, 1e-10))
check("normalize", approx(max(abs(normalize([0.5, -0.3, 0.1]))), 1, 1e-10))
check("noise_range", max(noise(1000)) <= 1 and min(noise(1000)) >= -1)
check("silence", sum(silence(100)) == 0)
check("dc", approx(dc(10, 3)[0], 3, 1e-10))

var sine_sig = sine(44100, 440, 0.1)
check("sine_length", len(sine_sig) == 4410)

var env_ad = env_perc(44100, 0.01, 0.1)
check("env_perc_length", len(env_ad) == floor(44100 * 0.11))
check("env_perc_start", approx(env_ad[0], 0, 0.01))

check("sec2samp", sec2samp(1, 44100) == 44100)
check("samp2sec", approx(samp2sec(44100, 44100), 1, 1e-10))

check("magnitude_len", len(magnitude(spectrum)) == 64)
check("phase_len", len(phase(spectrum)) == 64)

# ── summary ────────────────────────────────────────────────────────────

print ""
print "═══════════════════════════════════════════"
print " test_dsp: " pass " passed, " fail " failed"
print "═══════════════════════════════════════════"

if (fail > 0) {
    error("dsp tests failed")
}
