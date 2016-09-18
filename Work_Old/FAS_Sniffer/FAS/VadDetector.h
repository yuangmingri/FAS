/*
 * VadDetector.h
 */

#ifndef LTSD_H_
#define LTSD_H_

#include <vector>
#include <deque>
#include <string.h>
#include <cmath>
#include <math.h>
#include "utils.h"
#include "ckfft/ckfft.h"
#include "MmseBasedNpe.h"
#include "PARADE.h"
#include "LPCResidual.h"

class VadDetector {
 public:
  VadDetector(int winsize, int samprate, int order = 7,double e0 = 40.0, double e1 = 80.0, double lambda0 = 20.0, double lambda1 = 10.0, double k0 = 4.0, double k1 = 4.0, double k2 = 4.0);
  virtual ~VadDetector();
  bool process(char* input);
  char* getSignal();
  void updateParams(double e0, double e1, double lambda0, double lambda1, double k0, double k1, double k2);
  int fftErrors();
 private:
  void createWindow();
  void calcLTSE();
  float calcLTSD();
  bool isSignal();
  void createNoiseProfile();
  float calcPower();
  float calcNoisePower();
  void updateVadHistories(bool decision);
  bool vadDecision();
  void initFFT();
  int windowsize;
  int analysissize;
  int fftsize;
  int freqsize;
  int samplingrate;
  int m_order;
  int vad_history_size;
  float m_e0;
  float m_e1;
  float m_lambda0;
  float m_lambda1;
  float m_k0;
  float m_k1;
  float m_k2;
  float* __restrict window;
  float* __restrict ltse;
  float* __restrict noise_profile;
  bool* vad_histories;
  float* __restrict power_spectrum;
  float avg_pow;
  bool estimated;

  MmseBasedNpe *mmse;
  PARADE *parade;
  LPCResidual *lpcr;
  float* __restrict fft_in;
  std::deque<float*> amp_history;
  std::deque<short*> signal_history;

  CkFftContext* context;
  CkFftComplex* forwardOutput;

  int fft_errors;
  int fft_score;
};

#endif /* LTSD_H_ */
