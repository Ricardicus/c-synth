#ifndef SPECTRUM_H
#define SPECTRUM_H

#define SPECTRUM_SIZE 2048
#define SPECTRUM_BINS (SPECTRUM_SIZE / 2 + 1)

/* Main-thread Hann-windowed FFT workspace. No allocation during analysis. */
typedef struct {
    double real[SPECTRUM_SIZE];
    double imag[SPECTRUM_SIZE];
    double window[SPECTRUM_SIZE];
    double windowSum;
    float db[SPECTRUM_BINS];
} Spectrum;

void spectrumInit(Spectrum *spectrum);
/* One-sided peak amplitudes in dBFS, floored at -90 dB, capped at 0 dB. */
void spectrumCompute(Spectrum *spectrum, const float samples[SPECTRUM_SIZE]);

#endif
