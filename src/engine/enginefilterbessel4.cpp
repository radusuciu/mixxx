/***************************************************************************
                          enginefilteriir.cpp  -  description
                             -------------------
    copyright            : (C) 2002 by Tue and Ken Haste Andersen
    email                :
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "engine/enginefilterbessel4.h"
#include "util/counter.h"
#include "util/math.h"
#define MIXXX
#include "fidlib.h"

EngineFilterIIR::EngineFilterIIR(int bufSize)
        : m_sampleRate(44100),
          m_bufSize(bufSize),
          m_doRamping(false){
    memset(m_coef, 0, MAX_COEFS * sizeof(double));
    initBuffers();
}

void EngineFilterIIR::initBuffers() {
    // Copy the current buffers into the old buffers
    memcpy(m_oldBuf1, m_buf1, m_bufSize * sizeof(double));
    memcpy(m_oldBuf2, m_buf2, m_bufSize * sizeof(double));
    // Set the current buffers to 0
    memset(m_buf1, 0, m_bufSize * sizeof(double));
    memset(m_buf2, 0, m_bufSize * sizeof(double));
}

EngineFilterIIR::~EngineFilterIIR() {
}

inline double _processLowpass(double* coef, double* buf, register double val) {
    register double tmp, fir, iir;
    tmp = buf[0]; buf[0] = buf[1]; buf[1] = buf[2]; buf[2] = buf[3];
    iir = val * coef[0];
    iir -= coef[1] * tmp; fir = tmp;
    iir -= coef[2] * buf[0]; fir += buf[0] + buf[0];
    fir += iir;
    tmp = buf[1]; buf[1] = iir; val = fir;
    iir = val;
    iir -= coef[3] * tmp; fir = tmp;
    iir -= coef[4] * buf[2]; fir += buf[2] + buf[2];
    fir += iir;
    buf[3] = iir; val = fir;
    return val;
}

inline double _processBandpass(double* coef, double* buf, register double val) {
    register double tmp, fir, iir;
    tmp = buf[0]; buf[0] = buf[1]; buf[1] = buf[2]; buf[2] = buf[3];
    buf[3] = buf[4]; buf[4] = buf[5]; buf[5] = buf[6]; buf[6] = buf[7];
    iir = val * coef[0];
    iir -= coef[1] * tmp; fir = tmp;
    iir -= coef[2] * buf[0]; fir += -buf[0] - buf[0];
    fir += iir;
    tmp = buf[1]; buf[1] = iir; val= fir;
    iir = val;
    iir -= coef[3] * tmp; fir = tmp;
    iir -= coef[4] * buf[2]; fir += -buf[2] - buf[2];
    fir += iir;
    tmp = buf[3]; buf[3] = iir; val= fir;
    iir = val;
    iir -= coef[5] * tmp; fir = tmp;
    iir -= coef[6] * buf[4]; fir += buf[4] + buf[4];
    fir += iir;
    tmp = buf[5]; buf[5] = iir; val= fir;
    iir = val;
    iir -= coef[7] * tmp; fir = tmp;
    iir -= coef[8] * buf[6]; fir += buf[6] + buf[6];
    fir += iir;
    buf[7] = iir; val = fir;
    return val;
}

inline double _processHighpass(double* coef, double* buf, register double val) {
    register double tmp, fir, iir;
    tmp = buf[0]; buf[0] = buf[1]; buf[1] = buf[2]; buf[2] = buf[3];
    iir= val * coef[0];
    iir -= coef[1] * tmp; fir = tmp;
    iir -= coef[2] * buf[0]; fir += -buf[0] - buf[0];
    fir += iir;
    tmp = buf[1]; buf[1] = iir; val = fir;
    iir = val;
    iir -= coef[3] * tmp; fir = tmp;
    iir -= coef[4] * buf[2]; fir += -buf[2] - buf[2];
    fir += iir;
    buf[3] = iir; val = fir;
    return val;
}

EngineFilterIIRLow::EngineFilterIIRLow(int sampleRate, double freqCorner1)
        : EngineFilterIIR(4) {
    setFrequencyCorners(sampleRate, freqCorner1);
}

void EngineFilterIIRLow::setFrequencyCorners(int sampleRate,
                                             double freqCorner1) {
    m_sampleRate = sampleRate;
    // Copy the old coefficients into m_oldCoef
    memcpy(m_oldCoef, m_coef, MAX_COEFS * sizeof(double));
    m_coef[0] = fid_design_coef(m_coef + 1, 4, "LpBe4", m_sampleRate,
                               freqCorner1, 0, 0);
    initBuffers();
    m_doRamping = true;
}

void EngineFilterIIRLow::process(const CSAMPLE* pIn, CSAMPLE* pOutput,
                                 const int iBufferSize) {
    double tmp1, tmp2;
    double cross_mix = 0.0;
    double cross_inc = 2.0 / static_cast<double>(iBufferSize);
    for (int i = 0; i < iBufferSize; i += 2) {
        pOutput[i] = _processLowpass(m_coef, m_buf1, pIn[i]);
        pOutput[i+1] = _processLowpass(m_coef, m_buf2, pIn[i + 1]);
        // Do a linear cross fade between the old samples and the new samples
        if (m_doRamping) {
            tmp1 = _processLowpass(m_oldCoef, m_oldBuf1, pIn[i]);
            tmp2 = _processLowpass(m_oldCoef, m_oldBuf2, pIn[i + 1]);
            pOutput[i] = pOutput[i] * cross_mix +
                         tmp1 * (1.0 - cross_mix);
            pOutput[i + 1] = pOutput[i + 1] * cross_mix +
                         tmp2 * (1.0 - cross_mix);
            cross_mix += cross_inc;
        }
    }
    m_doRamping = false;
}

EngineFilterIIRBand::EngineFilterIIRBand(int sampleRate, double freqCorner1,
                                         double freqCorner2)
        : EngineFilterIIR(16) {
    setFrequencyCorners(sampleRate, freqCorner1, freqCorner2);
}

void EngineFilterIIRBand::setFrequencyCorners(int sampleRate,
                                             double freqCorner1,
                                             double freqCorner2) {
    m_sampleRate = sampleRate;
    // Copy the old coefficients into m_oldCoef
    memcpy(m_oldCoef, m_coef, MAX_COEFS * sizeof(double));
    m_coef[0] = fid_design_coef(m_coef + 1, 8, "BpBe4", m_sampleRate,
                               freqCorner1, freqCorner2, 0);
    initBuffers();
    m_doRamping = true;
}

void EngineFilterIIRBand::process(const CSAMPLE* pIn, CSAMPLE* pOutput,
                                 const int iBufferSize) {
    double tmp1, tmp2;
    double cross_mix = 0.0;
    double cross_inc = 2.0 / static_cast<double>(iBufferSize);
    for (int i = 0; i < iBufferSize; i += 2) {
        pOutput[i] = _processBandpass(m_coef, m_buf1, pIn[i]);
        pOutput[i+1] = _processBandpass(m_coef, m_buf2, pIn[i+1]);
        // Do a linear cross fade between the old samples and the new samples
        if (m_doRamping) {
            tmp1 = _processBandpass(m_oldCoef, m_oldBuf1, pIn[i]);
            tmp2 = _processBandpass(m_oldCoef, m_oldBuf2, pIn[i+1]);
            pOutput[i] = pOutput[i] * cross_mix +
                         tmp1 * (1.0 - cross_mix);
            pOutput[i + 1] = pOutput[i + 1] * cross_mix +
                         tmp2 * (1.0 - cross_mix);
            cross_mix += cross_inc;
        }
    }
    m_doRamping = false;
}

EngineFilterIIRHigh::EngineFilterIIRHigh(int sampleRate, double freqCorner1)
        : EngineFilterIIR(4) {
    setFrequencyCorners(sampleRate, freqCorner1);
}

void EngineFilterIIRHigh::setFrequencyCorners(int sampleRate,
                                             double freqCorner1) {
    m_sampleRate = sampleRate;
    // Copy the old coefficients into m_oldCoef
    memcpy(m_oldCoef, m_coef, MAX_COEFS * sizeof(double));
    m_coef[0] = fid_design_coef(m_coef + 1, 4, "HpBe4", m_sampleRate,
                               freqCorner1, 0, 0);
    initBuffers();
    m_doRamping = true;
}

void EngineFilterIIRHigh::process(const CSAMPLE* pIn, CSAMPLE* pOutput,
                                 const int iBufferSize) {
    double tmp1, tmp2;
    double cross_mix = 0.0;
    double cross_inc = 2.0 / static_cast<double>(iBufferSize);
    for (int i = 0; i < iBufferSize; i += 2) {
        pOutput[i] = _processHighpass(m_coef, m_buf1, pIn[i]);
        pOutput[i+1] = _processHighpass(m_coef, m_buf2, pIn[i+1]);
        // Do a linear cross fade between the old samples and the new samples
        if (m_doRamping) {
            tmp1 = _processHighpass(m_oldCoef, m_oldBuf1, pIn[i]);
            tmp2 = _processHighpass(m_oldCoef, m_oldBuf2, pIn[i+1]);
            pOutput[i] = pOutput[i] * cross_mix +
                         tmp1 * (1.0 - cross_mix);
            pOutput[i + 1] = pOutput[i + 1] * cross_mix +
                         tmp2 * (1.0 - cross_mix);
            cross_mix += cross_inc;
        }
    }
    m_doRamping = false;
}