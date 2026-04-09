# reconstruction.flux — additive resynthesis from FFT magnitudes
# Analyses a sound, extracts the strongest spectral peaks, resynthesises
# with oscbank. Uses top-N peak selection to keep computation tractable.

load ("dsp.flux")

var dur       = 1.2
var nwin      = 4096
var offset    = 128
var max_parts = 64     # max number of partials to resynthesize
var tab1      = gen(nwin, 1)

print "analysing..."

var wavinfo = wavread("../data/gong_c_sharp.wav")
var raw     = wavinfo[0]
var sr      = wavinfo[1]
var nch     = wavinfo[2]

var input = raw
if (nch > 1) {
    input = deinterleave(raw, nch, 0)
}

var samps = floor(dur * sr)

var spec      = fft(input)
var polar     = car2pol(spec)
var mag0      = deinterleave(polar, 2, 0)
var nfft      = len(mag0)
var nstop     = nwin
if (nfft < nwin) { nstop = nfft }
var mag       = slice(mag0, offset, nstop)
var fftfreqs0 = range(nfft) * sr / (nfft - 1)
var fftfreqs  = slice(fftfreqs0, offset, offset + len(mag))

# ── select the top-N loudest bins ─────────────────────────────────────
var nbins = len(mag)
var nparts = max_parts
if (nbins < nparts) { nparts = nbins }

# find threshold: sort magnitudes, take the nparts-th from the top
var mag_sorted = sort(mag)
var threshold  = mag_sorted[nbins - nparts]

print "using" nparts "partials out of" nbins "bins"

# build amplitude and frequency envelopes only for bins above threshold
var amps  = list()
var freqs = list()
var count = 0
var i = 0
while (i < nbins and count < nparts) {
    if (mag[i] >= threshold) {
        var vsc  = mag[i] * 2 / nfft
        var enva = ones(samps) * vsc
        var envf = ones(samps) * fftfreqs[i]
        amps  = concat(amps, list(enva))
        freqs = concat(freqs, list(envf))
        count = count + 1
    }
    i = i + 1
}

print "done (" count "partials selected)"
print "synthesising..."

var out       = oscbank(sr, amps, freqs, tab1)
var fade      = range(samps) * (-0.8) / (samps - 1) + 0.8
var out_faded = out * fade

print "done"
wavwrite(out_faded, sr, "reconstructed.wav")
