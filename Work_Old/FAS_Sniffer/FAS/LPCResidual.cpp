/*
 * LPCResidual.cpp
 */

#include "LPCResidual.h"
#include <cmath>
#include "lpc.h"

LPCResidual::LPCResidual(int windowsize, int lpcorder) {
  // TODO Auto-generated constructor stub
  winsize = windowsize;
  order = lpcorder;
  R = new float[order + 1];
  K = new float[order + 1];
  A = new float[order + 1];
  for (int i=0; i< order + 1; i++){
    R[i] = 0.0;
    K[i] = 0.0;
    A[i] = 0.0;
  }
  ressize = winsize - order;
  last_k = 0.0;
  Residuals = new float[ressize];
}

LPCResidual::~LPCResidual() {
  // TODO Auto-generated destructor stub
  delete[] R;
  delete[] K;
  delete[] A;
  delete[] Residuals;
}


float LPCResidual::process(float* __restrict windowed){
  //wAutocorrelate(float *x, unsigned int L, float *R, unsigned int P, float lambda){
  wAutocorrelate(windowed, winsize, R, order, 0.0);
  LevinsonRecursion(order, R, A, K);
  calcResiduals(windowed);
  float k = calcKurtosis();

  last_k = last_k * 0.7 + k * 0.3;
  return last_k;
}

void LPCResidual::calcResiduals(float* __restrict windowed){
  float predict = 0.0;
  for (int i = order; i < winsize; i++){
    predict = 0.0;
    for(int j = 1; j <= order; j++){
      // LPCオーダー分処理
      predict += A[j] * windowed[i - j];
    }
    Residuals[i - order] = windowed[i] - predict;
  }
}

float LPCResidual::calcKurtosis(){
  float avg = 0;
  float moment = 0;
  for(int i = 0; i < ressize; i++){
    avg += Residuals[i];
  }
  avg /= float(ressize);

  for(int i = 0; i < ressize; i++){
    moment += std::pow(Residuals[i] - avg, 2);
  }
  moment /= ressize;

  float s_sd = std::sqrt((float(ressize) / (float(ressize) - 1)) * moment);

  float k4 = 0.0;
  for(int i = 0; i < ressize; i++){
    k4 += std::pow((Residuals[i] - avg) / s_sd, 4);
  }
  float k = ((ressize * (ressize + 1.0)) / ((ressize - 1.0) * (ressize - 2.0) * (ressize - 3.0))) * k4;
  return k - 3.0 * (std::pow((float(ressize - 1.0)), 2)) / ((ressize - 2.0) * (ressize - 3.0));
}
