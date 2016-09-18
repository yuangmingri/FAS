/*
 * MmseBasedNpe.h
 *
 *  Created on: 2015/11/27
 *      Author: aihara
 */

#ifndef MMSEBASEDNPE_H_
#define MMSEBASEDNPE_H_

#include "utils.h"
#include "string.h"
#include <cmath>

class MmseBasedNpe {
 public:
  MmseBasedNpe(int size, float* noiseProfile);
  virtual ~MmseBasedNpe();
  void process(float* __restrict amp);
  void updateNoiseProfile(float* noise);
 private:
  int fftsize;
  double* __restrict PH1mean;
  double alphaPH1mean;
  double alphaPSD;
  double q;
  double priorFact;
  double xiOptDb;
  double xiOpt;
  double logGLRFact;
  double GLRexp;

  float* __restrict noisePow;
  float* __restrict noisyPer;
  double* __restrict snrPost1;
  double* __restrict estimate;
  double* __restrict GLR;
  double* __restrict PH1;

};

#endif /* MMSEBASEDNPE_H_ */
