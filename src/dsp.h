// dsp.h — Flux DSP library
//
// Call register_dsp(interp) from main after constructing the Interpreter.
//
// Primitives: gen, osc, phasor, bpf, fft, ifft, car2pol, pol2car,
//             stft, istft, window, conv, resample, delay, comb, allpass,
//             reson, iir, iirdesign, mix, wavread, wavwrite,
//             deinterleave, interleave, oscbank

#ifndef DSP_H
#define DSP_H

#include "flux.h"
#include <cstring>
#include <cstdint>

namespace flux {

// ── Cooley-Tukey FFT ──────────────────────────────────────────────────
// buf: interleaved (re,im), length = 2*N, N must be power of 2
// sign = -1 → forward, +1 → inverse (unnormalized)
static void fft_core(double* buf, int N, int sign) {
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(buf[2*i], buf[2*j]);
            std::swap(buf[2*i+1], buf[2*j+1]);
        }
    }
    for (int len = 2; len <= N; len <<= 1) {
        double ang = sign * 2.0 * M_PI / len;
        double wRe = std::cos(ang), wIm = std::sin(ang);
        for (int i = 0; i < N; i += len) {
            double tRe = 1.0, tIm = 0.0;
            for (int j = 0; j < len / 2; ++j) {
                int u = 2*(i+j), v = 2*(i+j+len/2);
                double uRe=buf[u], uIm=buf[u+1], vRe=buf[v], vIm=buf[v+1];
                double xRe = tRe*vRe - tIm*vIm, xIm = tRe*vIm + tIm*vRe;
                buf[u]=uRe+xRe;
                buf[u+1]=uIm+xIm;
                buf[v]=uRe-xRe;
                buf[v+1]=uIm-xIm;
                double nRe = tRe*wRe - tIm*wIm;
                tIm = tRe*wIm + tIm*wRe;
                tRe = nRe;
            }
        }
    }
}

static int next_pow2(int n) {
    if (n <= 1) return 1;
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

// ── internal: convolution via FFT ─────────────────────────────────────
static Vec conv_fft(const Vec& x, const Vec& y) {
    int sx=(int)x.size(), sy=(int)y.size();
    if (!sx || !sy) return Vec(0.0, 0);
    int cl = sx+sy-1, N = next_pow2(cl);
    std::vector<double> X(2*N,0), Y(2*N,0), R(2*N,0);
    for (int i=0; i<sx; ++i) X[2*i]=x[i];
    for (int i=0; i<sy; ++i) Y[2*i]=y[i];
    fft_core(X.data(),N,-1);
    fft_core(Y.data(),N,-1);
    for (int i=0; i<N; ++i) {
        double xr=X[2*i],xi=X[2*i+1],yr=Y[2*i],yi=Y[2*i+1];
        R[2*i]=xr*yr-xi*yi;
        R[2*i+1]=xr*yi+xi*yr;
    }
    fft_core(R.data(),N,+1);
    Vec out(cl);
    for (int i=0; i<cl; ++i) out[i]=R[2*i]/N;
    return out;
}

// ── internal: frequency-domain resampling ─────────────────────────────
static Vec resample_fd(const Vec& x, double factor) {
    int in_len=(int)x.size();
    if (!in_len||factor<=0) return Vec(0.0,0);
    int out_len=std::max(1,(int)std::floor(in_len*factor+0.5));
    int N1=next_pow2(in_len), N2=next_pow2(out_len);
    std::vector<double> X(2*N1,0);
    for (int i=0; i<in_len; ++i) X[2*i]=x[i];
    fft_core(X.data(),N1,-1);
    std::vector<double> Y(2*N2,0);
    int Nc=std::min(N1/2,N2/2);
    Y[0]=X[0];
    Y[1]=X[1];
    for (int k=1; k<Nc; ++k) {
        Y[2*k]=X[2*k];
        Y[2*k+1]=X[2*k+1];
        Y[2*(N2-k)]=X[2*(N1-k)];
        Y[2*(N2-k)+1]=X[2*(N1-k)+1];
    }
    if (N1%2==0&&N2%2==0) {
        Y[N2]=X[N1];
        Y[N2+1]=X[N1+1];
    }
    fft_core(Y.data(),N2,+1);
    Vec out(out_len);
    for (int i=0; i<out_len; ++i) out[i]=Y[2*i]/N2;
    return out;
}

// ── WAV I/O helpers ───────────────────────────────────────────────────
static void wle16(std::ostream& o, uint16_t v) {
    o.put((char)(v&0xff));
    o.put((char)((v>>8)&0xff));
}
static void wle32(std::ostream& o, uint32_t v) {
    o.put((char)(v&0xff));
    o.put((char)((v>>8)&0xff));
    o.put((char)((v>>16)&0xff));
    o.put((char)((v>>24)&0xff));
}
static uint16_t rle16(std::istream& in) {
    uint8_t a=(uint8_t)in.get(),b=(uint8_t)in.get();
    return (uint16_t)(a|(b<<8));
}
static uint32_t rle32(std::istream& in) {
    uint8_t a=(uint8_t)in.get(),b=(uint8_t)in.get(),c=(uint8_t)in.get(),d=(uint8_t)in.get();
    return (uint32_t)(a|(b<<8)|(c<<16)|(d<<24));
}

// ── oscillator core (used by osc and oscbank) ─────────────────────────
static Vec osc_core(double sr, const Vec& freqs, const Vec& table) {
    int tN=(int)table.size()-1;
    double fn=sr/tN, phi=0;
    Vec out(freqs.size());
    for (size_t i=0; i<freqs.size(); ++i) {
        int ip=(int)phi;
        double fp=phi-ip;
        out[i]=(1.0-fp)*table[ip]+fp*table[ip+1];
        phi+=freqs[i]/fn;
        while (phi>=tN) phi-=tN;
        while (phi<0) phi+=tN;
    }
    return out;
}

// ── DSP registration ──────────────────────────────────────────────────
inline void register_dsp(Interpreter& interp) {
    auto D = [&](const char* nm, NativeFn fn) {
        interp.global->def(nm, Value(std::move(fn)));
    };

    // gen(n, c1, c2, ...) — additive wavetable (gen10)
    D("gen",[](auto& a,int ln,auto& f)->Value{
        if(a.size()<2) err(f,ln,"gen expects: n c1 [c2 ...]");
        if(!a[0].is_vec()) err(f,ln,"gen: n must be numeric");
        int n=(int)a[0].scalar();
        if(n<=0) err(f,ln,"gen: n must be > 0");
        int nh=(int)a.size()-1;
        std::vector<double> coeffs(nh); double norm=0;
        for(int j=0; j<nh; ++j) {
            if(!a[j+1].is_vec()) err(f,ln,"gen: coefficients must be numeric");
            coeffs[j]=a[j+1].scalar();
            norm+=std::abs(coeffs[j]);
        }
        if(norm==0) norm=1;
        Vec table(n+1);
        for(int i=0; i<n; ++i) {
            double acc=0;
            for(int j=0; j<nh; ++j) acc+=coeffs[j]*std::sin(2.0*M_PI*(j+1)*i/n);
            table[i]=acc/norm;
        }
        table[n]=table[0];
        return Value(table);});

    // osc(sr, freq_vec, table) — wavetable oscillator
    D("osc",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=3) err(f,ln,"osc expects: sr freq table");
        if(!a[0].is_vec()) err(f,ln,"osc: sr must be numeric");
        if(!a[1].is_vec()) err(f,ln,"osc: freq must be numeric");
        if(!a[2].is_vec()) err(f,ln,"osc: table must be numeric");
        double sr=a[0].scalar();
        if(sr<=0) err(f,ln,"osc: sr must be > 0");
        auto& table=a[2].as_vec();
        if((int)table.size()<2) err(f,ln,"osc: table must have >= 2 samples");
        return Value(osc_core(sr,a[1].as_vec(),table));});

    // phasor(sr, freq_vec) — phasor oscillator (0 to 1)
    D("phasor",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=2) err(f,ln,"phasor expects: sr freq");
        if(!a[0].is_vec()||!a[1].is_vec()) err(f,ln,"phasor: args must be numeric");
        double sr=a[0].scalar();
        if(sr<=0) err(f,ln,"phasor: sr must be > 0");
        auto& freqs=a[1].as_vec();
        Vec out(freqs.size()); double phi=0;
        for(size_t i=0; i<freqs.size(); ++i) {
            out[i]=phi;
            phi+=freqs[i]/sr;
            while(phi>=1.0)phi-=1.0;
            while(phi<0.0)phi+=1.0;
        }
        return Value(out);});

    // bpf(x1,y1,x2,y2,...,phase_vec) — breakpoint function
    D("bpf",[](auto& a,int ln,auto& f)->Value{
        if(a.size()<3||(a.size()%2)==0) err(f,ln,"bpf expects: x y pairs and phase signal as last arg");
        if(!a.back().is_vec()) err(f,ln,"bpf: last arg must be numeric (phase)");
        auto& phase=a.back().as_vec();
        std::vector<double> pts; pts.reserve(a.size()-1);
        for(size_t i=0; i+1<a.size(); ++i) {
            if(!a[i].is_vec()) err(f,ln,"bpf: control points must be numeric");
            pts.push_back(a[i].scalar());
        }
        if(pts.size()<4) err(f,ln,"bpf needs at least 2 points");
        Vec out(phase.size());
        for(size_t k=0; k<phase.size(); ++k) {
            double ph=phase[k], y=pts[1];
            if(ph<=pts[0]) y=pts[1];
            else if(ph>=pts[pts.size()-2]) y=pts[pts.size()-1];
            else {
                for(size_t i=0; i+3<pts.size(); i+=2) {
                    double x0=pts[i],y0=pts[i+1],x1=pts[i+2],y1=pts[i+3];
                    if(ph>=x0&&ph<=x1) {
                        double u=(x1>x0)?((ph-x0)/(x1-x0)):0;
                        y=y0+(y1-y0)*u;
                        break;
                    }
                }
            }
            out[k]=y;
        }
        return Value(out);});

    // fft(sig) → interleaved complex [re0 im0 re1 im1 ...]
    D("fft",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=1) err(f,ln,"fft expects: sig");
        if(!a[0].is_vec()) err(f,ln,"fft: sig must be numeric");
        auto& sig=a[0].as_vec(); int d=(int)sig.size(), N=next_pow2(d);
        std::vector<double> buf(2*N,0);
        for(int i=0; i<d; ++i) buf[2*i]=sig[i];
        fft_core(buf.data(),N,-1);
        Vec out(2*N); for(int i=0; i<2*N; ++i) out[i]=buf[i];
        return Value(out);});

    // ifft(spectrum) → real signal
    D("ifft",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=1) err(f,ln,"ifft expects: spectrum");
        if(!a[0].is_vec()) err(f,ln,"ifft: spectrum must be numeric");
        auto& spec=a[0].as_vec(); int len=(int)spec.size();
        if(len%2) err(f,ln,"ifft: length must be even");
        int N=len/2;
        std::vector<double> buf(len);
        for(int i=0; i<len; ++i) buf[i]=spec[i];
        fft_core(buf.data(),N,+1);
        Vec out(N); for(int i=0; i<N; ++i) out[i]=buf[2*i]/N;
        return Value(out);});

    // car2pol(spectrum) — cartesian → polar (mag,phase pairs)
    D("car2pol",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=1) err(f,ln,"car2pol expects: spectrum");
        if(!a[0].is_vec()) err(f,ln,"car2pol: arg must be numeric");
        auto& v=a[0].as_vec();
        if(v.size()%2) err(f,ln,"car2pol: length must be even");
        int N=(int)v.size()/2; Vec out(2*N);
        for(int i=0; i<N; ++i) {
            double re=v[2*i],im=v[2*i+1];
            out[2*i]=std::sqrt(re*re+im*im);
            out[2*i+1]=std::atan2(im,re);
        }
        return Value(out);});

    // pol2car(spectrum) — polar → cartesian
    D("pol2car",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=1) err(f,ln,"pol2car expects: spectrum");
        if(!a[0].is_vec()) err(f,ln,"pol2car: arg must be numeric");
        auto& v=a[0].as_vec();
        if(v.size()%2) err(f,ln,"pol2car: length must be even");
        int N=(int)v.size()/2; Vec out(2*N);
        for(int i=0; i<N; ++i) {
            double mag=v[2*i],ph=v[2*i+1];
            out[2*i]=mag*std::cos(ph);
            out[2*i+1]=mag*std::sin(ph);
        }
        return Value(out);});

    // stft(sig, n, hop) → list of spectra
    D("stft",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=3) err(f,ln,"stft expects: sig n hop");
        if(!a[0].is_vec()||!a[1].is_vec()||!a[2].is_vec()) err(f,ln,"stft: args must be numeric");
        auto& sig=a[0].as_vec(); int n=(int)a[1].scalar(), hop=(int)a[2].scalar();
        if(n<=0) err(f,ln,"stft: n must be > 0");
        if(hop<=0) err(f,ln,"stft: hop must be > 0");
        // Hann window
        Vec win(n);
        if(n==1) win[0]=1; else for(int i=0; i<n; ++i) win[i]=0.5-0.5*std::cos(2.0*M_PI*i/(n-1));
        int N=next_pow2(n);
        List frames;
        for(int off=0; off+n<=(int)sig.size(); off+=hop) {
            std::vector<double> buf(2*N,0);
            for(int i=0; i<n; ++i) buf[2*i]=sig[off+i]*win[i];
            fft_core(buf.data(),N,-1);
            Vec spec(2*N);
            for(int i=0; i<2*N; ++i) spec[i]=buf[i];
            frames.push_back(Value(spec));
        }
        return Value(frames);});

    // istft(specs, n, hop) → signal (overlap-add)
    D("istft",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=3) err(f,ln,"istft expects: specs n hop");
        if(!a[0].is_list()) err(f,ln,"istft: specs must be a list");
        if(!a[1].is_vec()||!a[2].is_vec()) err(f,ln,"istft: n and hop must be numeric");
        int n=(int)a[1].scalar(), hop=(int)a[2].scalar();
        if(n<=0||hop<=0) err(f,ln,"istft: n and hop must be > 0");
        auto& specs=a[0].as_list(); size_t nf=specs.size();
        if(!nf) return Value(Vec(0.0,0));
        Vec win(n);
        if(n==1) win[0]=1; else for(int i=0; i<n; ++i) win[i]=0.5-0.5*std::cos(2.0*M_PI*i/(n-1));
        size_t outlen=(size_t)n+(nf-1)*(size_t)hop;
        Vec out(0.0,outlen), wsum(0.0,outlen);
        for(size_t fi=0; fi<nf; ++fi) {
            if(!specs[fi].is_vec()) err(f,ln,"istft: each frame must be a vec");
            auto& sp=specs[fi].as_vec();
            int len=(int)sp.size();
            if(len%2) err(f,ln,"istft: frame length must be even");
            int N=len/2;
            std::vector<double> buf(len);
            for(int i=0; i<len; ++i) buf[i]=sp[i];
            fft_core(buf.data(),N,+1);
            size_t off=fi*(size_t)hop;
            for(int i=0; i<n&&i<N; ++i) {
                double w=win[i];
                out[off+i]+=buf[2*i]/N*w;
                wsum[off+i]+=w*w;
            }
        }
        for(size_t i=0; i<outlen; ++i) if(wsum[i]>1e-12) out[i]/=wsum[i];
        return Value(out);});

    // window(n, a0, a1, a2) — generalized cosine window
    D("window",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=4) err(f,ln,"window expects: n a0 a1 a2");
        for(int i=0; i<4; ++i) if(!a[i].is_vec()) err(f,ln,"window: args must be numeric");
        int N=(int)a[0].scalar(); double a0=a[1].scalar(),a1=a[2].scalar(),a2=a[3].scalar();
        if(N<=0) err(f,ln,"window: n must be > 0");
        Vec w(N);
        for(int i=0; i<N; ++i) {
            double t=2.0*M_PI*i/(N-1);
            w[i]=a0-a1*std::cos(t)+a2*std::cos(2.0*t);
        }
        return Value(w);});

    // conv(x, y) — FFT convolution
    D("conv",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=2) err(f,ln,"conv expects: x y");
        if(!a[0].is_vec()||!a[1].is_vec()) err(f,ln,"conv: args must be numeric");
        return Value(conv_fft(a[0].as_vec(),a[1].as_vec()));});

    // resample(sig, factor)
    D("resample",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=2) err(f,ln,"resample expects: sig factor");
        if(!a[0].is_vec()||!a[1].is_vec()) err(f,ln,"resample: args must be numeric");
        return Value(resample_fd(a[0].as_vec(),a[1].scalar()));});

    // delay(sig, d) — fractional delay (linear interpolation)
    D("delay",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=2) err(f,ln,"delay expects: sig d");
        if(!a[0].is_vec()||!a[1].is_vec()) err(f,ln,"delay: args must be numeric");
        auto& x=a[0].as_vec(); double D=a[1].scalar(); int N=(int)x.size();
        Vec y(N);
        for(int n=0; n<N; ++n) {
            double pos=n-D;
            if(pos<0) {
                y[n]=0;
                continue;
            }
            int i0=(int)std::floor(pos);
            double frac=pos-i0;
            y[n]=(i0>=N-1)?x[N-1]:(1.0-frac)*x[i0]+frac*x[i0+1];
        }
        return Value(y);});

    // comb(sig, d, g) — feedback comb: y[n] = x[n] + g*y[n-d]
    D("comb",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=3) err(f,ln,"comb expects: sig d g");
        for(int i=0; i<3; ++i) if(!a[i].is_vec()) err(f,ln,"comb: args must be numeric");
        auto& x=a[0].as_vec(); int D=(int)a[1].scalar(); double g=a[2].scalar();
        if(D<0) err(f,ln,"comb: d must be >= 0");
        int N=(int)x.size(); Vec y(N);
        for(int n=0; n<N; ++n) {
            double fb=(n-D>=0)?g*y[n-D]:0;
            y[n]=x[n]+fb;
        }
        return Value(y);});

    // allpass(sig, d, g) — Schroeder allpass
    D("allpass",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=3) err(f,ln,"allpass expects: sig d g");
        for(int i=0; i<3; ++i) if(!a[i].is_vec()) err(f,ln,"allpass: args must be numeric");
        auto& x=a[0].as_vec(); int D=(int)a[1].scalar(); double g=a[2].scalar();
        if(D<0) err(f,ln,"allpass: d must be >= 0");
        int N=(int)x.size(); Vec y(N);
        for(int n=0; n<N; ++n) {
            double xD=(n-D>=0)?x[n-D]:0, yD=(n-D>=0)?y[n-D]:0;
            y[n]=-g*x[n]+xD+g*yD;
        }
        return Value(y);});

    // reson(sig, sr, freq, tau) — 2nd-order resonant filter
    D("reson",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=4) err(f,ln,"reson expects: sig sr freq tau");
        for(int i=0; i<4; ++i) if(!a[i].is_vec()) err(f,ln,"reson: args must be numeric");
        auto& sig=a[0].as_vec(); double sr=a[1].scalar(),freq=a[2].scalar(),tau=a[3].scalar();
        double om=2.0*M_PI*(freq/sr), radius=std::exp(-2.0*M_PI/(tau*sr));
        double a1=-2.0*radius*std::cos(om), a2=radius*radius, gain=radius*std::sin(om);
        int samps=(int)(sr*tau), insize=(int)sig.size();
        Vec out(samps); double y1=0,y2=0;
        for(int i=0; i<samps; ++i) {
            double x=i<insize?sig[i]:0;
            double v=gain*x-a1*y1-a2*y2;
            y2=y1;
            y1=v;
            out[i]=v;
        }
        return Value(out);});

    // iir(sig, b, a) — direct-form II IIR/FIR filter
    D("iir",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=3) err(f,ln,"iir expects: sig b a");
        for(int i=0; i<3; ++i) if(!a[i].is_vec()) err(f,ln,"iir: args must be numeric");
        auto& x=a[0].as_vec(); auto bv=a[1].as_vec(); auto av=a[2].as_vec();
        int N=(int)x.size(),Nb=(int)bv.size(),Na=(int)av.size();
        if(!Nb) err(f,ln,"iir: b is empty"); if(!Na) err(f,ln,"iir: a is empty");
        double a0=av[0]; if(a0==0) err(f,ln,"iir: a[0] is zero");
        Vec bn=bv/a0, an=av/a0, y(N);
        for(int n=0; n<N; ++n) {
            double acc=0;
            for(int k=0; k<Nb; ++k) if(n-k>=0) acc+=bn[k]*x[n-k];
            for(int k=1; k<Na; ++k) if(n-k>=0) acc-=an[k]*y[n-k];
            y[n]=acc;
        }
        return Value(y);});

    // iirdesign(type, fs, f0, Q, gain_db) → list(b, a)
    D("iirdesign",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=5) err(f,ln,"iirdesign expects: type fs f0 Q gain_db");
        if(!a[0].is_str()) err(f,ln,"iirdesign: type must be string");
        for(int i=1; i<5; ++i) if(!a[i].is_vec()) err(f,ln,"iirdesign: numeric args expected");
        auto& type=a[0].as_str();
        double fs=a[1].scalar(),f0=a[2].scalar(),Q=a[3].scalar(),dBg=a[4].scalar();
        if(fs<=0||f0<=0||f0>=fs/2) err(f,ln,"iirdesign: invalid fs or f0");
        if(Q<=0) err(f,ln,"iirdesign: Q must be > 0");
        double w0=2.0*M_PI*f0/fs, cosw=std::cos(w0), sinw=std::sin(w0);
        double alpha=sinw/(2.0*Q), A=std::pow(10.0,dBg/40.0);
        double b0,b1,b2,a0,a1r,a2r;
        if(type=="lowpass") {
            b0=(1-cosw)/2;
            b1=1-cosw;
            b2=(1-cosw)/2;
            a0=1+alpha;
            a1r=-2*cosw;
            a2r=1-alpha;
        } else if(type=="highpass") {
            b0=(1+cosw)/2;
            b1=-(1+cosw);
            b2=(1+cosw)/2;
            a0=1+alpha;
            a1r=-2*cosw;
            a2r=1-alpha;
        } else if(type=="notch") {
            b0=1;
            b1=-2*cosw;
            b2=1;
            a0=1+alpha;
            a1r=-2*cosw;
            a2r=1-alpha;
        } else if(type=="peak"||type=="peaking") {
            b0=1+alpha*A;
            b1=-2*cosw;
            b2=1-alpha*A;
            a0=1+alpha/A;
            a1r=-2*cosw;
            a2r=1-alpha/A;
        } else if(type=="lowshelf"||type=="loshelf") {
            double s=2*std::sqrt(A)*alpha;
            b0=A*((A+1)-(A-1)*cosw+s);
            b1=2*A*((A-1)-(A+1)*cosw);
            b2=A*((A+1)-(A-1)*cosw-s);
            a0=(A+1)+(A-1)*cosw+s;
            a1r=-2*((A-1)+(A+1)*cosw);
            a2r=(A+1)+(A-1)*cosw-s;
        } else if(type=="highshelf"||type=="hishelf") {
            double s=2*std::sqrt(A)*alpha;
            b0=A*((A+1)+(A-1)*cosw+s);
            b1=-2*A*((A-1)+(A+1)*cosw);
            b2=A*((A+1)+(A-1)*cosw-s);
            a0=(A+1)-(A-1)*cosw+s;
            a1r=2*((A-1)-(A+1)*cosw);
            a2r=(A+1)-(A-1)*cosw-s;
        } else err(f,ln,"iirdesign: unknown type: "+type);
        b0/=a0; b1/=a0; b2/=a0; a1r/=a0; a2r/=a0;
        Vec bv(3),av(3); bv[0]=b0; bv[1]=b1; bv[2]=b2; av[0]=1; av[1]=a1r; av[2]=a2r;
        return Value(List{Value(bv),Value(av)});});

    // mix(offset1,sig1,offset2,sig2,...) — overlay signals at offsets
    D("mix",[](auto& a,int ln,auto& f)->Value{
        if(a.size()%2) err(f,ln,"mix expects pairs: offset sig ...");
        std::vector<double> out;
        for(size_t i=0; i<a.size(); i+=2) {
            if(!a[i].is_vec()||!a[i+1].is_vec()) err(f,ln,"mix: args must be numeric");
            int off=(int)a[i].scalar();
            if(off<0) err(f,ln,"mix: offset must be >= 0");
            auto& sig=a[i+1].as_vec();
            int end=off+(int)sig.size();
            if(end>(int)out.size()) out.resize(end,0);
            for(int t=0; t<(int)sig.size(); ++t) out[t+off]+=sig[t];
        }
        Vec v(out.size()); for(size_t i=0; i<out.size(); ++i) v[i]=out[i];
        return Value(v);});

    // wavwrite(sig, sr, path, [nch=1])
    D("wavwrite",[](auto& a,int ln,auto& f)->Value{
        if(a.size()<3||a.size()>4) err(f,ln,"wavwrite expects: sig sr path [nch]");
        if(!a[0].is_vec()) err(f,ln,"wavwrite: sig must be numeric");
        if(!a[1].is_vec()) err(f,ln,"wavwrite: sr must be numeric");
        if(!a[2].is_str()) err(f,ln,"wavwrite: path must be string");
        auto& sig=a[0].as_vec(); int sr=(int)a[1].scalar();
        auto path=normalize_path(a[2].as_str(),f);
        int nch=(a.size()==4&&a[3].is_vec())?(int)a[3].scalar():1;
        if(nch<1) err(f,ln,"wavwrite: nch must be >= 1");
        int ns=(int)sig.size(); double mx=0;
        for(int i=0; i<ns; ++i) mx=std::max(mx,std::abs(sig[i]));
        double scale=(mx>0)?(32767.0/mx):32767.0;
        std::ofstream fo(path,std::ios::binary);
        if(!fo) err(f,ln,"wavwrite: cannot open "+path.string());
        uint32_t db=(uint32_t)(ns*2);
        fo.write("RIFF",4); wle32(fo,36+db); fo.write("WAVE",4); fo.write("fmt ",4); wle32(fo,16);
        wle16(fo,1); wle16(fo,(uint16_t)nch); wle32(fo,(uint32_t)sr);
        wle32(fo,(uint32_t)(sr*nch*2)); wle16(fo,(uint16_t)(nch*2)); wle16(fo,16);
        fo.write("data",4); wle32(fo,db);
        for(int i=0; i<ns; ++i) {
            int16_t s=(int16_t)std::max(-32768.0,std::min(32767.0,sig[i]*scale));
            fo.put((char)(s&0xff));
            fo.put((char)((s>>8)&0xff));
        }
        return Value(nullptr);});

    // wavread(path) → list(signal, sr, nch)
    D("wavread",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=1||!a[0].is_str()) err(f,ln,"wavread expects: path");
        auto path=normalize_path(a[0].as_str(),f);
        std::ifstream fi(path,std::ios::binary);
        if(!fi) err(f,ln,"wavread: cannot open "+path.string());
        char id4[4]; fi.read(id4,4);
        if(std::memcmp(id4,"RIFF",4)) err(f,ln,"wavread: not RIFF");
        rle32(fi); fi.read(id4,4);
        if(std::memcmp(id4,"WAVE",4)) err(f,ln,"wavread: not WAVE");
        int sr=44100,nch=1,bits=16;
        while(fi) {
            char chunk[4];
            fi.read(chunk,4);
            if(!fi)break;
            uint32_t sz=rle32(fi);
            if(!std::memcmp(chunk,"fmt ",4)) {
                rle16(fi);
                nch=(int)rle16(fi);
                sr=(int)rle32(fi);
                rle32(fi);
                rle16(fi);
                bits=(int)rle16(fi);
                if(sz>16)fi.seekg(sz-16,std::ios::cur);
            } else if(!std::memcmp(chunk,"data",4)) {
                int bps=bits/8, ns=(int)(sz/(uint32_t)bps);
                Vec sig(ns);
                if(bits==16) {
                    for(int i=0; i<ns; ++i) {
                        uint8_t lo=(uint8_t)fi.get(),hi=(uint8_t)fi.get();
                        sig[i]=(int16_t)(lo|(hi<<8))/32768.0;
                    }
                } else if(bits==8) {
                    for(int i=0; i<ns; ++i) sig[i]=((uint8_t)fi.get()-128)/128.0;
                } else if(bits==24) {
                    for(int i=0; i<ns; ++i) {
                        uint8_t a=(uint8_t)fi.get(),b=(uint8_t)fi.get(),c=(uint8_t)fi.get();
                        int32_t s=(int32_t)(a|(b<<8)|(c<<16));
                        if(s&0x800000)s|=(int32_t)0xFF000000;
                        sig[i]=s/8388608.0;
                    }
                } else err(f,ln,"wavread: unsupported bit depth "+std::to_string(bits));
                return Value(List{Value(sig),Value((double)sr),Value((double)nch)});
            } else fi.seekg(sz,std::ios::cur);
        }
        err(f,ln,"wavread: no data chunk");});

    // deinterleave(sig, nstreams, index)
    D("deinterleave",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=3) err(f,ln,"deinterleave expects: sig nstreams index");
        if(!a[0].is_vec()||!a[1].is_vec()||!a[2].is_vec()) err(f,ln,"deinterleave: args must be numeric");
        auto& sig=a[0].as_vec(); int stride=(int)a[1].scalar(), idx=(int)a[2].scalar();
        if(stride<=0) err(f,ln,"deinterleave: nstreams must be > 0");
        if(idx<0||idx>=stride) err(f,ln,"deinterleave: index out of range");
        size_t frames=sig.size()/stride;
        Vec out(frames); for(size_t i=0; i<frames; ++i) out[i]=sig[i*stride+idx];
        return Value(out);});

    // interleave(ch1, ch2, ...)
    D("interleave",[](auto& a,int ln,auto& f)->Value{
        if(a.empty()) err(f,ln,"interleave expects at least 1 vector");
        size_t nch=a.size(), maxlen=0;
        for(auto& x:a) {
            if(!x.is_vec()) err(f,ln,"interleave: args must be numeric");
            maxlen=std::max(maxlen,x.as_vec().size());
        }
        Vec out(0.0,maxlen*nch);
        for(size_t c=0; c<nch; ++c) {
            auto& ch=a[c].as_vec();
            for(size_t i=0; i<maxlen; ++i) out[i*nch+c]=(i<ch.size())?ch[i]:0;
        }
        return Value(out);});

    // oscbank(sr, amps_list, freqs_list, table) — additive synthesis
    D("oscbank",[](auto& a,int ln,auto& f)->Value{
        if(a.size()!=4) err(f,ln,"oscbank expects: sr amps freqs table");
        if(!a[0].is_vec()) err(f,ln,"oscbank: sr must be numeric");
        if(!a[1].is_list()||!a[2].is_list()) err(f,ln,"oscbank: amps and freqs must be lists");
        if(!a[3].is_vec()) err(f,ln,"oscbank: table must be numeric");
        double sr=a[0].scalar();
        if(sr<=0) err(f,ln,"oscbank: sr must be > 0");
        auto& table=a[3].as_vec();
        if((int)table.size()<2) err(f,ln,"oscbank: table too small");
        auto& amps=a[1].as_list(); auto& freqs=a[2].as_list();
        if(amps.size()!=freqs.size()) err(f,ln,"oscbank: amps/freqs size mismatch");
        if(amps.empty()) return Value(Vec(0.0,0));
        if(!amps[0].is_vec()||!freqs[0].is_vec()) err(f,ln,"oscbank: elements must be vecs");
        size_t n=amps[0].as_vec().size();
        Vec acc(0.0,n);
        for(size_t k=0; k<amps.size(); ++k) {
            if(!amps[k].is_vec()||!freqs[k].is_vec()) err(f,ln,"oscbank: elements must be vecs");
            auto& ak=amps[k].as_vec();
            auto& fk=freqs[k].as_vec();
            if(ak.size()!=n||fk.size()!=n) err(f,ln,"oscbank: all tracks must have same length");
            acc+=osc_core(sr,fk,table)*ak;
        }
        return Value(acc);});
}
} // namespace flux

#endif // DSP_H