# filters.flux — lowpass and bandpass filtering
# Demonstrates iirdesign + iir for standard filter types

load ("dsp.flux")

# convenience wrappers (lowpass / highpass via iirdesign + iir)
func lowpass (sig, sr, cutoff, q) {
    var coeffs = iirdesign("lowpass", sr, cutoff, q, 0)
    return iir(sig, coeffs[0], coeffs[1])
}

func highpass (sig, sr, cutoff, q) {
    var coeffs = iirdesign("highpass", sr, cutoff, q, 0)
    return iir(sig, coeffs[0], coeffs[1])
}

var wavinfo = wavread("../data/cage.wav")
var sr      = wavinfo[1]
var w       = deinterleave(wavinfo[0], wavinfo[2], 0)

print "lowpass"

var cutoff_lp = 200
var q_lp      = 0.707
var w_lp      = lowpass(w, sr, cutoff_lp, q_lp)
wavwrite(w_lp, sr, "lp.wav")

print "bandpass (highpass + lowpass cascade)"

var cutoff_hp = 2000
var cutoff_bp = 2500
var q_bp      = 0.707

var w_hp = highpass(w, sr, cutoff_hp, q_bp)
var w_bp = lowpass(w_hp, sr, cutoff_bp, q_bp)
wavwrite(w_bp, sr, "bp.wav")
