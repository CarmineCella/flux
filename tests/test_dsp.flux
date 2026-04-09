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
check("gen_harmonics_guard", approx(table3[0], table3[1024], 1e-10))

# ── osc (wavetable oscillator) ─────────────────────────────────────────

var sr = 44100
var dur_samp = 4410
var freq_vec = dc(dur_samp, 440)
var sig = osc(sr, freq_vec, table)
check("osc_length", len(sig) == dur_samp)
check("osc_range", max(sig) <= 1.01 and min(sig) >= -1.01)
check("osc_not_silent", max(abs(sig)) > 0.5)

# ── phasor ─────────────────────────────────────────────────────────────

var ph = phasor(sr, dc(sr, 1))
check("phasor_length", len(ph) == sr)
check("phasor_start", approx(ph[0], 0, 1e-10))
check("phasor_range", min(ph) >= 0 and max(ph) < 1)
check("phasor_monotonic_start", ph[1] > ph[0])

# ── bpf (breakpoint function) ─────────────────────────────────────────

var t = range(100) / 100
var env = bpf(0, 0, 0.5, 1, 1, 0, t)
check("bpf_length", len(env) == 100)
check("bpf_start", approx(env[0], 0, 1e-10))
check("bpf_mid", approx(env[50], 1, 0.05))
check("bpf_end", approx(env[99], 0, 0.05))
# bpf clamping outside range
var t2 = [-0.5, 0, 0.5, 1, 1.5]
var env2 = bpf(0, 10, 1, 20, t2)
check("bpf_clamp_low", approx(env2[0], 10, 1e-10))
check("bpf_clamp_high", approx(env2[4], 20, 1e-10))

# ── fft / ifft ─────────────────────────────────────────────────────────

var simple = sin(range(64) * 2 * pi / 64)
var spectrum = fft(simple)
check("fft_length", len(spectrum) == 128)

var reconstructed = ifft(spectrum)
check("ifft_length", len(reconstructed) == 64)
var recon_err = max(abs(reconstructed - simple))
check("fft_ifft_roundtrip", recon_err < 1e-10)

# DC signal FFT: bin 0 should have all energy
var dc_sig = ones(16)
var dc_spec = fft(dc_sig)
check("fft_dc_bin0", approx(dc_spec[0], 16, 1e-10))
check("fft_dc_bin0_im", approx(dc_spec[1], 0, 1e-10))

# ── car2pol / pol2car ──────────────────────────────────────────────────

var polar = car2pol(spectrum)
check("car2pol_length", len(polar) == len(spectrum))
var back = pol2car(polar)
var pol_err = max(abs(back - spectrum))
check("car2pol_pol2car_roundtrip", pol_err < 1e-10)

# known values: (3, 4) -> (5, atan2(4,3))
var known_cart = [3, 4]
var known_pol = car2pol(known_cart)
check("car2pol_mag", approx(known_pol[0], 5, 1e-10))
check("car2pol_phase", approx(known_pol[1], atan(4 / 3), 1e-6))

# ── window ─────────────────────────────────────────────────────────────

var w = window(256, 0.5, 0.5, 0)
check("window_length", len(w) == 256)
check("window_edges", approx(w[0], 0, 1e-10))
check("window_center", approx(w[127], 1, 0.01))

var wh = hann(256)
check("hann", max(abs(wh - w)) < 1e-10)
var wm = hamming(256)
check("hamming_length", len(wm) == 256)
check("hamming_edge", approx(wm[0], 0.08, 0.01))
var wb = blackman(256)
check("blackman_length", len(wb) == 256)
check("blackman_edge", approx(wb[0], 0, 0.01))

# ── rect_win ───────────────────────────────────────────────────────────

var wr = rect_win(128)
check("rect_win_length", len(wr) == 128)
check("rect_win_all_ones", approx(sum(wr), 128, 1e-10))

# ── conv (convolution) ────────────────────────────────────────────────

var imp_data = [1, 0, 0, 0, 0, 0, 0, 0]
var test_vec = [1, 2, 3, 4]
var c = conv(test_vec, imp_data)
check("conv_length", len(c) == len(test_vec) + len(imp_data) - 1)
check("conv_identity", approx(c[0], 1, 1e-10))
check("conv_identity2", approx(c[1], 2, 1e-10))
check("conv_identity3", approx(c[3], 4, 1e-10))

# convolution with [1, 1] = running sum of adjacent
var c2 = conv([1, 2, 3], [1, 1])
check("conv_sum_len", len(c2) == 4)
check("conv_sum_0", approx(c2[0], 1, 1e-10))
check("conv_sum_1", approx(c2[1], 3, 1e-10))
check("conv_sum_2", approx(c2[2], 5, 1e-10))
check("conv_sum_3", approx(c2[3], 3, 1e-10))

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
check("delay_start2", approx(delayed[1], 0, 1e-10))
check("delay_shift", approx(delayed[2], 1, 0.1))

# zero delay = identity
var d0 = delay(d_sig, 0)
check("delay_zero", max(abs(d0 - d_sig)) < 1e-10)

# ── comb filter ────────────────────────────────────────────────────────

var comb_in = concat([1], zeros(99))
var comb_out = comb(comb_in, 10, 0.5)
check("comb_length", len(comb_out) == 100)
check("comb_direct", approx(comb_out[0], 1, 1e-10))
check("comb_echo", approx(comb_out[10], 0.5, 1e-10))
check("comb_echo2", approx(comb_out[20], 0.25, 1e-10))
check("comb_echo3", approx(comb_out[30], 0.125, 1e-10))

# ── allpass filter ─────────────────────────────────────────────────────

var ap_in = concat([1], zeros(49))
var ap_out = allpass(ap_in, 5, 0.7)
check("allpass_length", len(ap_out) == 50)
check("allpass_first", approx(ap_out[0], -0.7, 1e-10))
check("allpass_delayed", approx(ap_out[5], 1 + 0.7 * ap_out[0], 1e-10))

# ── reson ──────────────────────────────────────────────────────────────

var res_in = concat([1], zeros(999))
var res_out = reson(res_in, 44100, 440, 0.05)
check("reson_length", len(res_out) == floor(44100 * 0.05))
check("reson_nonzero", max(abs(res_out)) > 0)
# output should decay
var res_early = max(abs(slice(res_out, 0, 100)))
var res_late = max(abs(slice(res_out, len(res_out) - 100, len(res_out))))
check("reson_decay", res_late < res_early)

# ── iir ────────────────────────────────────────────────────────────────

var iir_in = concat([1], zeros(31))
var iir_out = iir(iir_in, [1, 0, 0], [1, -0.5, 0])
check("iir_length", len(iir_out) == 32)
check("iir_direct", approx(iir_out[0], 1, 1e-10))
check("iir_feedback", approx(iir_out[1], 0.5, 1e-10))
check("iir_feedback2", approx(iir_out[2], 0.25, 1e-10))

# FIR mode (a = [1]): moving average
var fir_out = iir([1, 2, 3, 4, 5], [0.5, 0.5], [1])
check("iir_fir_0", approx(fir_out[0], 0.5, 1e-10))
check("iir_fir_1", approx(fir_out[1], 1.5, 1e-10))

# ── iirdesign ──────────────────────────────────────────────────────────

var coeffs = iirdesign("lowpass", 44100, 1000, 0.707, 0)
check("iirdesign_is_list", type(coeffs) == "list")
check("iirdesign_b_len", len(coeffs[0]) == 3)
check("iirdesign_a_len", len(coeffs[1]) == 3)
check("iirdesign_a0_normalized", approx(coeffs[1][0], 1, 1e-10))

var coeffs_hp = iirdesign("highpass", 44100, 1000, 0.707, 0)
check("iirdesign_hp", len(coeffs_hp[0]) == 3)
check("iirdesign_hp_a0", approx(coeffs_hp[1][0], 1, 1e-10))

var coeffs_pk = iirdesign("peak", 44100, 1000, 2, 6)
check("iirdesign_peak", len(coeffs_pk[0]) == 3)

var coeffs_notch = iirdesign("notch", 44100, 1000, 2, 0)
check("iirdesign_notch_b", len(coeffs_notch[0]) == 3)
check("iirdesign_notch_a", len(coeffs_notch[1]) == 3)

var coeffs_ls = iirdesign("lowshelf", 44100, 1000, 0.707, 6)
check("iirdesign_lowshelf", len(coeffs_ls[0]) == 3)

var coeffs_hs = iirdesign("highshelf", 44100, 1000, 0.707, 6)
check("iirdesign_highshelf", len(coeffs_hs[0]) == 3)

# apply designed filter
var lp_out = iir(concat([1], zeros(63)), coeffs[0], coeffs[1])
check("iirdesign_apply", len(lp_out) == 64)

# ── mix ────────────────────────────────────────────────────────────────

var mix_out = mix(0, [1, 1, 1], 2, [2, 2, 2])
check("mix_length", len(mix_out) == 5)
check("mix_overlap", approx(mix_out[2], 3, 1e-10))
check("mix_tail", approx(mix_out[4], 2, 1e-10))
check("mix_head", approx(mix_out[0], 1, 1e-10))

# three signals
var mix3 = mix(0, [1], 1, [1], 2, [1])
check("mix_three", approx(mix3[0], 1, 1e-10))
check("mix_three_len", len(mix3) == 3)

# ── wavwrite / wavread ────────────────────────────────────────────────

var wav_sig = sin(range(1000) * 2 * pi * 440 / 44100)
wavwrite(wav_sig, 44100, "/tmp/flux_test.wav")
var wav_data = wavread("/tmp/flux_test.wav")
check("wavread_is_list", type(wav_data) == "list")
check("wavread_sr", wav_data[1] == 44100)
check("wavread_nch", wav_data[2] == 1)
check("wavread_samples", len(wav_data[0]) == 1000)

# stereo write/read
var stereo_sig = interleave(wav_sig, wav_sig * 0.5)
wavwrite(stereo_sig, 44100, "/tmp/flux_test_stereo.wav", 2)
var wav_st = wavread("/tmp/flux_test_stereo.wav")
check("wavread_stereo_nch", wav_st[2] == 2)

# ── deinterleave / interleave ──────────────────────────────────────────

var stereo_il = interleave([1, 2, 3], [4, 5, 6])
check("interleave_length", len(stereo_il) == 6)
check("interleave_order", approx(stereo_il[0], 1, 1e-10))
check("interleave_order2", approx(stereo_il[1], 4, 1e-10))
check("interleave_order3", approx(stereo_il[2], 2, 1e-10))

var left = deinterleave(stereo_il, 2, 0)
var right = deinterleave(stereo_il, 2, 1)
check("deinterleave_left", max(abs(left - [1, 2, 3])) < 1e-10)
check("deinterleave_right", max(abs(right - [4, 5, 6])) < 1e-10)

# 3-channel interleave/deinterleave
var tri = interleave([1, 2], [3, 4], [5, 6])
check("interleave_3ch_len", len(tri) == 6)
var ch0 = deinterleave(tri, 3, 0)
var ch1 = deinterleave(tri, 3, 1)
var ch2 = deinterleave(tri, 3, 2)
check("deinterleave_3ch_0", max(abs(ch0 - [1, 2])) < 1e-10)
check("deinterleave_3ch_1", max(abs(ch1 - [3, 4])) < 1e-10)
check("deinterleave_3ch_2", max(abs(ch2 - [5, 6])) < 1e-10)

# ── stft / istft ──────────────────────────────────────────────────────

var stft_sig = sin(range(1024) * 2 * pi * 10 / 1024)
var frames = stft(stft_sig, 256, 128)
check("stft_type", type(frames) == "list")
check("stft_nframes", len(frames) == 7)

var istft_sig = istft(frames, 256, 128)
check("istft_length", len(istft_sig) >= 1024)

# roundtrip: interior samples should be close
var stft_err = max(abs(slice(istft_sig, 128, 896) - slice(stft_sig, 128, 896)))
check("stft_istft_roundtrip", stft_err < 0.05)

# ── oscbank ────────────────────────────────────────────────────────────

var bank_n = 4410
var amp1 = dc(bank_n, 0.5)
var amp2 = dc(bank_n, 0.3)
var freq1 = dc(bank_n, 440)
var freq2 = dc(bank_n, 880)
var bank_out = oscbank(44100, list(amp1, amp2), list(freq1, freq2), table)
check("oscbank_length", len(bank_out) == bank_n)
check("oscbank_range", max(abs(bank_out)) <= 1.01)

# single partial oscbank should match osc * amp
var bank_1p = oscbank(44100, list(dc(4410, 1)), list(dc(4410, 440)), table)
var osc_ref = osc(44100, dc(4410, 440), table)
check("oscbank_single_match", max(abs(bank_1p - osc_ref)) < 1e-10)

# ══════════════════════════════════════════════════════════════════════
# dsp.flux convenience functions
# ══════════════════════════════════════════════════════════════════════

# ── mtof / ftom ────────────────────────────────────────────────────────

check("mtof_a4", approx(mtof(69), 440, 0.01))
check("mtof_a5", approx(mtof(81), 880, 0.01))
check("mtof_c4", approx(mtof(60), 261.63, 0.1))
check("ftom_440", approx(ftom(440), 69, 0.01))
check("ftom_880", approx(ftom(880), 81, 0.01))
# roundtrip
check("mtof_ftom_roundtrip", approx(ftom(mtof(42)), 42, 1e-6))

# ── db2amp / amp2db ────────────────────────────────────────────────────

check("db2amp_0", approx(db2amp(0), 1, 1e-10))
check("db2amp_-6", approx(db2amp(-6), 0.5012, 0.01))
check("db2amp_-20", approx(db2amp(-20), 0.1, 0.001))
check("amp2db_1", approx(amp2db(1), 0, 1e-10))
check("amp2db_roundtrip", approx(amp2db(db2amp(-12)), -12, 1e-6))

# ── normalize ──────────────────────────────────────────────────────────

check("normalize", approx(max(abs(normalize([0.5, -0.3, 0.1]))), 1, 1e-10))
check("normalize_zero", max(abs(normalize(zeros(10)))) < 1e-10)

# ── noise / silence / dc ──────────────────────────────────────────────

check("noise_range", max(noise(1000)) <= 1 and min(noise(1000)) >= -1)
check("noise_length", len(noise(500)) == 500)
check("silence", sum(silence(100)) == 0)
check("silence_length", len(silence(50)) == 50)
check("dc", approx(dc(10, 3)[0], 3, 1e-10))
check("dc_all", approx(sum(dc(10, 3)), 30, 1e-10))

# ── fade_in / fade_out ─────────────────────────────────────────────────

var fade_sig = ones(100)
var fi = fade_in(fade_sig, 20)
check("fade_in_length", len(fi) == 100)
check("fade_in_start", approx(fi[0], 0, 1e-10))
check("fade_in_end", approx(fi[99], 1, 1e-10))
check("fade_in_mid", fi[10] > 0 and fi[10] < 1)

var fo = fade_out(fade_sig, 20)
check("fade_out_length", len(fo) == 100)
check("fade_out_start", approx(fo[0], 1, 1e-10))
check("fade_out_tail", approx(fo[99], 0, 0.1))
check("fade_out_mid", fo[90] > 0 and fo[90] < 1)

# ── sine / saw / square ───────────────────────────────────────────────

var sine_sig = sine(44100, 440, 0.1)
check("sine_length", len(sine_sig) == 4410)
check("sine_range", max(sine_sig) <= 1.01 and min(sine_sig) >= -1.01)

var saw_sig = saw(44100, 440, 0.1)
check("saw_length", len(saw_sig) == 4410)
check("saw_range", max(saw_sig) <= 1.01 and min(saw_sig) >= -1.01)
check("saw_nonzero", max(abs(saw_sig)) > 0.5)

var sq_sig = square(44100, 440, 0.1)
check("square_length", len(sq_sig) == 4410)
check("square_values", max(sq_sig) <= 1.01 and min(sq_sig) >= -1.01)

# ── env_adsr / env_perc ───────────────────────────────────────────────

var env_ad = env_perc(44100, 0.01, 0.1)
check("env_perc_length", len(env_ad) == floor(44100 * 0.11))
check("env_perc_start", approx(env_ad[0], 0, 0.01))
check("env_perc_end", approx(env_ad[len(env_ad) - 1], 0, 0.05))

var env_a = env_adsr(44100, 0.01, 0.01, 0.5, 0.01, 0.1)
check("env_adsr_length", len(env_a) == floor(44100 * 0.1))
check("env_adsr_start", approx(env_a[0], 0, 0.01))
check("env_adsr_peak", max(env_a) > 0.9)

# ── bin2freq / freq2bin ────────────────────────────────────────────────

check("bin2freq_dc", approx(bin2freq(0, 44100, 1024), 0, 1e-10))
check("bin2freq_nyq", approx(bin2freq(512, 44100, 1024), 22050, 0.1))
check("freq2bin_440", approx(freq2bin(440, 44100, 1024), 440 * 1024 / 44100, 0.01))
check("bin_freq_roundtrip", approx(bin2freq(freq2bin(1000, 44100, 2048), 44100, 2048), 1000, 1e-6))

# ── sec2samp / samp2sec ───────────────────────────────────────────────

check("sec2samp", sec2samp(1, 44100) == 44100)
check("sec2samp_half", sec2samp(0.5, 44100) == 22050)
check("samp2sec", approx(samp2sec(44100, 44100), 1, 1e-10))
check("samp2sec_roundtrip", approx(samp2sec(sec2samp(0.25, 48000), 48000), 0.25, 1e-10))

# ── magnitude / phase ─────────────────────────────────────────────────

check("magnitude_len", len(magnitude(spectrum)) == 64)
check("phase_len", len(phase(spectrum)) == 64)
# magnitude of DC signal FFT: bin 0 should be 16
var dc_mags = magnitude(dc_spec)
check("magnitude_dc", approx(dc_mags[0], 16, 1e-6))

# ── rect_win (alias) ──────────────────────────────────────────────────

var rw2 = rect_win(64)
check("rect_win_flat", min(rw2) == 1 and max(rw2) == 1)

# ── summary ────────────────────────────────────────────────────────────

print ""
print "═══════════════════════════════════════════"
print " test_dsp: " pass " passed, " fail " failed"
print "═══════════════════════════════════════════"

if (fail > 0) {
    error("dsp tests failed")
}
