/*
 * LPCResidual.h
 */

#ifndef LPCRESIDUAL_H_
#define LPCRESIDUAL_H_

class LPCResidual {
 public:
  LPCResidual(int windowsize, int lpcorder);
  virtual ~LPCResidual();
  float process(float* __restrict signal);

 private:

  void calcResiduals(float* __restrict windowed);
  float calcKurtosis();

  int winsize;
  int ressize;
  float last_k;
  unsigned int order;
  float* __restrict R;
  float* __restrict K;
  float* __restrict A;
  float* __restrict Residuals;
};

#endif /* LPCRESIDUAL_H_ */
