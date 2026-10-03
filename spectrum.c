#include "spectrum.h"

#include <math.h>

#define TAU 6.28318530717958647692

void spectrumInit(Spectrum *spectrum)
{
    spectrum->windowSum = 0.0;
    for (int i = 0; i < SPECTRUM_SIZE; ++i) {
        spectrum->window[i] = 0.5 * (1.0 - cos(TAU * i / (SPECTRUM_SIZE - 1)));
        spectrum->windowSum += spectrum->window[i];
    }
    for (int i = 0; i < SPECTRUM_BINS; ++i)
        spectrum->db[i] = -90.0f;
}

void spectrumCompute(Spectrum *spectrum, const float samples[SPECTRUM_SIZE])
{
    for (int i = 0, reversed = 0; i < SPECTRUM_SIZE; ++i) {
        spectrum->real[reversed] = samples[i] * spectrum->window[i];
        spectrum->imag[reversed] = 0.0;
        int bit = SPECTRUM_SIZE / 2;
        while (reversed & bit) {
            reversed ^= bit;
            bit >>= 1;
        }
        reversed ^= bit;
    }
    for (int length = 2; length <= SPECTRUM_SIZE; length *= 2) {
        double stepReal = cos(-TAU / length);
        double stepImag = sin(-TAU / length);
        for (int base = 0; base < SPECTRUM_SIZE; base += length) {
            double twiddleReal = 1.0, twiddleImag = 0.0;
            for (int i = 0; i < length / 2; ++i) {
                int even = base + i, odd = even + length / 2;
                double real = twiddleReal * spectrum->real[odd] - twiddleImag * spectrum->imag[odd];
                double imag = twiddleReal * spectrum->imag[odd] + twiddleImag * spectrum->real[odd];
                spectrum->real[odd] = spectrum->real[even] - real;
                spectrum->imag[odd] = spectrum->imag[even] - imag;
                spectrum->real[even] += real;
                spectrum->imag[even] += imag;
                double nextReal = twiddleReal * stepReal - twiddleImag * stepImag;
                twiddleImag = twiddleReal * stepImag + twiddleImag * stepReal;
                twiddleReal = nextReal;
            }
        }
    }
    for (int i = 0; i < SPECTRUM_BINS; ++i) {
        double scale = i == 0 || i == SPECTRUM_SIZE / 2 ? 1.0 : 2.0;
        double amplitude = scale * hypot(spectrum->real[i], spectrum->imag[i]) / spectrum->windowSum;
        spectrum->db[i] = (float)fmax(-90.0, fmin(0.0, 20.0 * log10(fmax(1e-12, amplitude))));
    }
}
