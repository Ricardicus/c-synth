#include "spectrum.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

#define TAU 6.28318530717958647692

int main(void)
{
    Spectrum spectrum;
    float samples[SPECTRUM_SIZE] = {0};
    spectrumInit(&spectrum);
    spectrumCompute(&spectrum, samples);
    for (int i = 0; i < SPECTRUM_BINS; ++i)
        CHECK(spectrum.db[i] == -90);
    for (int i = 0; i < SPECTRUM_SIZE; ++i)
        samples[i] = (float)(0.5 * sin(TAU * 64 * i / SPECTRUM_SIZE));
    spectrumCompute(&spectrum, samples);
    CHECK(fabs(spectrum.db[64] - 20 * log10(0.5)) < 0.01);
    for (int i = 0; i < SPECTRUM_BINS; ++i)
        CHECK(spectrum.db[i] <= spectrum.db[64]);
    for (int i = 0; i < SPECTRUM_SIZE; ++i)
        samples[i] = 0.25f;
    spectrumCompute(&spectrum, samples);
    CHECK(fabs(spectrum.db[0] - 20 * log10(0.25)) < 0.01);
    for (int i = 0; i < SPECTRUM_SIZE; ++i)
        samples[i] = i % 2 == 0 ? 0.25f : -0.25f;
    spectrumCompute(&spectrum, samples);
    CHECK(fabs(spectrum.db[SPECTRUM_SIZE / 2] - 20 * log10(0.25)) < 0.01);
    puts("FFT tone frequency, amplitude scaling, silence, DC, and Nyquist checks passed.");
    return 0;
}
