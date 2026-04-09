# stft_istft.flux — STFT time-stretching and pitch-shifting
# Demonstrates analysis/resynthesis, time-stretch, and pitch-shift

load ("dsp.flux")

var snd = wavread("../data/cage.wav")
var sr  = snd[1]
var ch  = deinterleave(snd[0], snd[2], 0)

var N   = 2048
var hop = N / 8

# normal analysis / resynthesis
var specs = stft(ch, N, hop)
var recon = istft(specs, N, hop)
wavwrite(recon, sr, "stft_istft_test.wav")

# time-stretch by changing overlap-add hop
var stretch2x = istft(specs, N, hop * 2)
wavwrite(stretch2x, sr, "stft_istft_2x.wav")

var stretch05x = istft(specs, N, hop / 2)
wavwrite(stretch05x, sr, "stft_istft_halfx.wav")

# plain resampling for comparison
var resamp2x = resample(ch, 2)
wavwrite(resamp2x, sr, "stft_istft_resampled_2x.wav")

var resamp05x = resample(ch, 0.5)
wavwrite(resamp05x, sr, "stft_istft_resampled_halfx.wav")

# ── pitch shifting ─────────────────────────────────────────────────────
# pitch up = stretch longer, then resample down
# pitch down = stretch shorter, then resample up

# up one octave
var ps_up2_stretch = istft(specs, N, hop * 2)
var ps_up2         = resample(ps_up2_stretch, 0.5)
wavwrite(ps_up2, sr, "stft_pitch_up_2x.wav")

# down one octave
var ps_down2_stretch = istft(specs, N, hop / 2)
var ps_down2         = resample(ps_down2_stretch, 2.0)
wavwrite(ps_down2, sr, "stft_pitch_down_0_5x.wav")

# up a perfect fifth (~1.5x)
var ps_up15_stretch = istft(specs, N, floor(hop * 1.5))
var ps_up15         = resample(ps_up15_stretch, 1 / 1.5)
wavwrite(ps_up15, sr, "stft_pitch_up_1_5x.wav")

# down to 0.8x
var ps_down08_stretch = istft(specs, N, floor(hop * 0.8))
var ps_down08         = resample(ps_down08_stretch, 1 / 0.8)
wavwrite(ps_down08, sr, "stft_pitch_down_0_8x.wav")
