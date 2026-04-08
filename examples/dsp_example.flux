# ══════════════════════════════════════════════════════════════════════
# dsp_example.flux — DSP showcase
# Run: ./flux dsp_example.flux
# Outputs: several .wav files in the working directory
# ══════════════════════════════════════════════════════════════════════

load ("dsp.flux")

var sr = 44100
print "sample rate:" sr

# ┌────────────────────────────────────────────────┐
# │  1. WAVETABLE SYNTHESIS                        │
# └────────────────────────────────────────────────┘

print ""
print "── 1. Wavetable Synthesis ──"

# gen creates a wavetable with harmonics (gen10 style)
var sine_table = gen(4096, 1)           # pure sine
var rich_table = gen(4096, 1, 0.5, 0.33, 0.25, 0.2)  # 5 harmonics

# osc reads a wavetable with a frequency envelope
var n = sr * 2   # 2 seconds
var freq = dc(n, 440)
var tone = osc(sr, freq, sine_table)
print "sine tone:" len(tone) "samples"

# frequency modulation: vibrato
var vibrato = dc(n, 440) + osc(sr, dc(n, 6), sine_table) * 10
var vibrato_tone = osc(sr, vibrato, sine_table)

# rich timbre
var rich_tone = osc(sr, dc(n, 220), rich_table)

# ┌────────────────────────────────────────────────┐
# │  2. ENVELOPES AND BPF                         │
# └────────────────────────────────────────────────┘

print ""
print "── 2. Envelopes ──"

# ADSR envelope using dsp.flux convenience
var adsr = env_adsr(sr, 0.05, 0.1, 0.6, 0.3, 1.0)
print "ADSR length:" len(adsr)

# percussive envelope
var perc = env_perc(sr, 0.005, 0.3)
print "percussive env:" len(perc)

# manual bpf (breakpoint function)
var t = range(sr) / sr   # 1 second of time values
var custom_env = bpf(0, 0, 0.1, 1, 0.3, 0.5, 0.8, 0.5, 1, 0, t)

# apply envelope to tone
var shaped = osc(sr, dc(sr, 440), sine_table) * custom_env
wavwrite(shaped, sr, "/tmp/flux_shaped.wav")
print "wrote /tmp/flux_shaped.wav"

# ┌────────────────────────────────────────────────┐
# │  3. PHASOR AND SAW/SQUARE                     │
# └────────────────────────────────────────────────┘

print ""
print "── 3. Phasor-based Oscillators ──"

var ph = phasor(sr, dc(sr, 440))
var saw_wave = ph * 2 - 1
var sq_wave = floor(ph * 2) * 2 - 1
print "phasor range:" min(ph) "to" max(ph)

# ┌────────────────────────────────────────────────┐
# │  4. FFT AND SPECTRAL PROCESSING               │
# └────────────────────────────────────────────────┘

print ""
print "── 4. FFT / Spectral ──"

# analyse a short signal
var test_sig = sin(range(256) * 2 * pi * 4 / 256)
var spec = fft(test_sig)
print "spectrum length:" len(spec) "(complex pairs:" len(spec) / 2 ")"

# convert to polar to see magnitudes
var polar = car2pol(spec)
var mags = magnitude(spec)
print "peak magnitude at bin:" 0

# reconstruct
var back = ifft(spec)
var error_val = max(abs(back - test_sig))
print "FFT roundtrip error:" error_val

# ┌────────────────────────────────────────────────┐
# │  5. STFT (SHORT-TIME FOURIER TRANSFORM)        │
# └────────────────────────────────────────────────┘

print ""
print "── 5. STFT ──"

var long_sig = sin(range(sr) * 2 * pi * 440 / sr) * env_perc(sr, 0.01, 0.99)
var frames = stft(long_sig, 1024, 512)
print "STFT frames:" len(frames) "of size" len(frames[0])

# resynthesize
var resyn = istft(frames, 1024, 512)
print "resynthesized:" len(resyn) "samples"

# ┌────────────────────────────────────────────────┐
# │  6. WINDOWS                                    │
# └────────────────────────────────────────────────┘

print ""
print "── 6. Windows ──"

var w_hann = hann(512)
var w_hamm = hamming(512)
var w_black = blackman(512)
print "hann center:" w_hann[255]
print "hamming center:" w_hamm[255]
print "blackman center:" w_black[255]

# ┌────────────────────────────────────────────────┐
# │  7. FILTERS                                    │
# └────────────────────────────────────────────────┘

print ""
print "── 7. Filters ──"

# design a lowpass at 1kHz
var lp = iirdesign("lowpass", sr, 1000, 0.707, 0)
print "lowpass b:" lp[0]
print "lowpass a:" lp[1]

# apply to noise
var n_samp = sr
var white = noise(n_samp)
var filtered = iir(white, lp[0], lp[1])
print "filtered noise rms:" sqrt(mean(filtered * filtered))

# resonant filter (bell struck)
var impulse = concat([1], zeros(sr - 1))
var bell = reson(impulse, sr, 880, 0.5)
wavwrite(bell, sr, "/tmp/flux_bell.wav")
print "wrote /tmp/flux_bell.wav"

# comb filter (metallic resonance)
var comb_sig = comb(impulse, floor(sr / 440), 0.95)
var comb_shaped = slice(comb_sig, 0, sr) * env_perc(sr, 0.001, 0.999)
wavwrite(comb_shaped, sr, "/tmp/flux_comb.wav")
print "wrote /tmp/flux_comb.wav"

# ┌────────────────────────────────────────────────┐
# │  8. CONVOLUTION AND RESAMPLING                 │
# └────────────────────────────────────────────────┘

print ""
print "── 8. Convolution / Resample ──"

# convolve with a short reverb impulse
var ir = env_perc(sr, 0, 0.05) * noise(floor(sr * 0.05))
var convolved = conv(slice(shaped, 0, sr), ir)
print "convolution length:" len(convolved)

# resample
var original = sine(sr, 440, 0.1)
var upsampled = resample(original, 2)
var downsampled = resample(original, 0.5)
print "original:" len(original) "up:" len(upsampled) "down:" len(downsampled)

# ┌────────────────────────────────────────────────┐
# │  9. MIXING AND COMPOSITION                    │
# └────────────────────────────────────────────────┘

print ""
print "── 9. Mixing ──"

# create a short melody
var note1 = sine(sr, mtof(60), 0.3) * env_perc(sr, 0.01, 0.29)  # C4
var note2 = sine(sr, mtof(64), 0.3) * env_perc(sr, 0.01, 0.29)  # E4
var note3 = sine(sr, mtof(67), 0.3) * env_perc(sr, 0.01, 0.29)  # G4
var note4 = sine(sr, mtof(72), 0.5) * env_perc(sr, 0.01, 0.49)  # C5

var melody = mix(
    0,                    note1,
    floor(sr * 0.3),      note2,
    floor(sr * 0.6),      note3,
    floor(sr * 0.9),      note4
)
wavwrite(melody, sr, "/tmp/flux_melody.wav")
print "wrote /tmp/flux_melody.wav (" len(melody) "samples)"

# ┌────────────────────────────────────────────────┐
# │  10. ADDITIVE SYNTHESIS (OSCBANK)              │
# └────────────────────────────────────────────────┘

print ""
print "── 10. Oscbank (Additive Synthesis) ──"

var bank_n = sr * 2
var fund = 220

# create amplitude and frequency envelopes for 4 partials
var amps = list(
    dc(bank_n, 1.0) * bpf(0, 1, 2, 0, range(bank_n) / sr),
    dc(bank_n, 0.5) * bpf(0, 1, 1.5, 0, range(bank_n) / sr),
    dc(bank_n, 0.3) * bpf(0, 1, 1, 0, range(bank_n) / sr),
    dc(bank_n, 0.15) * bpf(0, 1, 0.5, 0, range(bank_n) / sr)
)
var freqs_bank = list(
    dc(bank_n, fund),
    dc(bank_n, fund * 2),
    dc(bank_n, fund * 3),
    dc(bank_n, fund * 4)
)

var additive = oscbank(sr, amps, freqs_bank, sine_table)
var additive_norm = normalize(additive)
wavwrite(additive_norm, sr, "/tmp/flux_additive.wav")
print "wrote /tmp/flux_additive.wav (" len(additive) "samples)"

# ┌────────────────────────────────────────────────┐
# │  11. INTERLEAVE / DEINTERLEAVE (STEREO)        │
# └────────────────────────────────────────────────┘

print ""
print "── 11. Stereo ──"

var left = melody
var right = delay(melody, floor(sr * 0.03))  # 30ms delay for width
var stereo = interleave(left, slice(right, 0, len(left)))
wavwrite(stereo, sr, "/tmp/flux_stereo.wav", 2)
print "wrote /tmp/flux_stereo.wav (stereo)"

# verify roundtrip
var left_back = deinterleave(stereo, 2, 0)
var right_back = deinterleave(stereo, 2, 1)
print "stereo roundtrip error:" max(abs(left_back - left))

# ┌────────────────────────────────────────────────┐
# │  12. PITCH UTILITIES                           │
# └────────────────────────────────────────────────┘

print ""
print "── 12. Pitch Utilities ──"

print "A4 = MIDI 69 =" mtof(69) "Hz"
print "C4 = MIDI 60 =" mtof(60) "Hz"
print "440 Hz = MIDI" ftom(440)
print "0 dB =" db2amp(0) "amplitude"
print "-6 dB =" db2amp(-6) "amplitude"

print ""
print "════════════════════════════════════════════"
print " DSP example complete. Check /tmp/flux_*.wav"
print "════════════════════════════════════════════"
