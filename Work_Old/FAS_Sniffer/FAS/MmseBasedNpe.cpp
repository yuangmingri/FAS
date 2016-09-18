/*
 * Mmsebasednpe.cpp
 */

#include <MmseBasedNpe.h>

#include "VadDetector.h"


MmseBasedNpe::MmseBasedNpe(int size, float* noiseProfile) {
  // TODO Auto-generated constructor stub
  fftsize = size;
  alphaPH1mean = 0.9;
  alphaPSD = 0.8;

  q = 0.5; // a priori probability of speech presence:
  priorFact = q / (1 - q);
  xiOptDb = 30.0; // optimal fixed a priori SNR for SPP estimation
  xiOpt = std::pow(10.0, (xiOptDb / 10.0));
  logGLRFact = std::log(1.0 / (1.0 + xiOpt));
  GLRexp = xiOpt / (1.0 + xiOpt);

  PH1mean = makeVector(fftsize, (double)0.5);
  noisePow = new float[fftsize];
  memcpy(noisePow, noiseProfile, sizeof(float) * fftsize);
  noisyPer = new float[fftsize];
  snrPost1 = new double[fftsize];
  estimate = new double[fftsize];
  GLR = new double[fftsize];
  PH1 = new double[fftsize];
}

MmseBasedNpe::~MmseBasedNpe() {
  // TODO Auto-generated destructor stub
  delete[] PH1mean;
  delete[] noisePow;
  delete[] noisyPer;
  delete[] snrPost1;
  delete[] estimate;
  delete[] GLR;
  delete[] PH1;
}


void MmseBasedNpe::process(float* __restrict amp) {
  int i = 0;
  double tmp;
  for(i = 0; i< fftsize; i++) {
    noisyPer[i] = amp[i] * amp[i];
    snrPost1[i] = (double)noisyPer[i] / (double)noisePow[i];
    
    tmp = logGLRFact + GLRexp * snrPost1[i];
    if (tmp > 200.0) {
      tmp = 200.0;
    }
    GLR[i] = priorFact * std::exp(tmp);
    PH1[i] = GLR[i] / (1.0 + GLR[i]);
    PH1mean[i] = alphaPH1mean * PH1mean[i] + (1.0 - alphaPH1mean) * PH1[i];
    if (PH1mean[i] > 0.99) {
      if (PH1[i] > 0.99) {
        PH1[i] = 0.99;
      }
    }
    estimate[i] = PH1[i] * (double)noisePow[i] + (1.0 - PH1[i]) * (double)noisyPer[i];
    noisePow[i] = float(alphaPSD * (double)noisePow[i] + (1.0 - alphaPSD) * estimate[i]);
  }
}

void MmseBasedNpe::updateNoiseProfile(float* noise){
  memcpy(noise, noisePow, sizeof(float) * fftsize);
}

