# conv_verb.flux — convolution reverb
# Convolves a dry signal with an impulse response

load ("dsp.flux")

var sig1_data = wavread("../data/anechoic1.wav")
var sig2_data = wavread("../data/Concertgebouw-s.wav")

var sig1 = sig1_data[0]
var sr1  = sig1_data[1]
var ch1  = sig1_data[2]

var sig2 = sig2_data[0]
var sr2  = sig2_data[1]
var ch2  = sig2_data[2]

if (sr1 != sr2) {
    error("signal1 and signal2 must have the same sample rate")
}
if (ch1 != 1 and ch1 != 2) {
    error("signal1 must be mono or stereo")
}
if (ch2 != 1 and ch2 != 2) {
    error("signal2 must be mono or stereo")
}

# extract channels (mono → use as-is, stereo → deinterleave)
var s1L = sig1
var s1R = sig1
if (ch1 == 2) {
    s1L = deinterleave(sig1, ch1, 0)
    s1R = deinterleave(sig1, ch1, 1)
}

var s2L = sig2
var s2R = sig2
if (ch2 == 2) {
    s2L = deinterleave(sig2, ch2, 0)
    s2R = deinterleave(sig2, ch2, 1)
}

if (ch1 == 1 and ch2 == 1) {
    var wet = normalize(conv(s1L, s2L))
    wavwrite(wet, sr1, "reverb.wav")
} else if (ch1 == 1 and ch2 == 2) {
    var wetL = normalize(conv(s1L, s2L))
    var wetR = normalize(conv(s1L, s2R))
    wavwrite(interleave(wetL, wetR), sr1, "reverb.wav", 2)
} else if (ch1 == 2 and ch2 == 1) {
    var wetL = normalize(conv(s1L, s2L))
    var wetR = normalize(conv(s1R, s2L))
    wavwrite(interleave(wetL, wetR), sr1, "reverb.wav", 2)
} else {
    var wetL = normalize(conv(s1L, s2L))
    var wetR = normalize(conv(s1R, s2R))
    wavwrite(interleave(wetL, wetR), sr1, "reverb.wav", 2)
}
