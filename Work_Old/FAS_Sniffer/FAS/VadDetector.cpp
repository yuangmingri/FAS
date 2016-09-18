/*
 * VadDetector.cpp
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "VadDetector.h"

VadDetector::VadDetector(int winsize, int samprate, int order, double e0, double e1, double lambda0, double lambda1,
           double k0, double k1, double k2){
  windowsize = winsize;
  analysissize = winsize * 4;
  fftsize = analysissize / 2 + 1;
  freqsize = fftsize / 2.5;
  samplingrate = samprate;
  m_order = order;
  m_e0 = e0;
  m_e1 = e1;
  m_lambda0 = lambda0;
  m_lambda1 = lambda1;
  m_k0 = k0;
  m_k1 = k1;
  m_k2 = k2;
  fft_errors = 0;
  vad_history_size = 1;

  vad_histories = new bool[vad_history_size];
  for (int i=0; i < vad_history_size; i++){
    vad_histories[i] = false;
  }

  fft_in = new float[analysissize];
  for(int i=0; i< analysissize; i++){
    fft_in[i] = 0.0;
  }
  ltse = new float[fftsize];
  noise_profile = new float[fftsize];
  power_spectrum = new float[fftsize];
  for(int i=0; i< fftsize; i++){
    noise_profile[i] = 0.0;
    power_spectrum[i] = 0.0;
  }

  estimated = false;
  createWindow();
  context = CkFftInit(analysissize, kCkFftDirection_Both, NULL, NULL);
  forwardOutput = new CkFftComplex[analysissize];
    
  parade = new PARADE(winsize, analysissize, window);
  mmse = NULL;

  lpcr = new LPCResidual(winsize, 10);
}

VadDetector::~VadDetector() {
  for (std::deque<short*>::iterator its = signal_history.begin(); its != signal_history.end(); its++){
    delete[] (*its);
  }
  for (std::deque<float*>::iterator ita = amp_history.begin(); ita != amp_history.end(); ita++){
    delete[] (*ita);
  }
  if (window != NULL){
    delete[] window;
  }
  delete[] fft_in;
  delete[] ltse;
  delete[] noise_profile;
  delete[] power_spectrum;
  delete[] forwardOutput;
  CkFftShutdown(context);

  if (mmse != NULL){
    delete mmse;
  }
  if (parade != NULL){
    delete parade;
  }

  if(lpcr != NULL){
    delete lpcr;
  }
}

bool VadDetector::process(char *input){
  short* signal = (short *)input;
  for(int i=0; i<windowsize; i++){
    fft_in[i]=(float(signal[i]) / 32767.0) * window[i];
  }
  fft_score = 0;

  int i;
  CkFftRealForward(context, analysissize, fft_in, forwardOutput);
    
  float* amp = new float[fftsize];
  float t_avg_pow = 0.0;
  int  * i_amp = new int[fftsize];
  float  t_avg_amp = 0.0;

  //printf("ffw: ");
  for(i=0; i<fftsize; i++) {
    if (forwardOutput != NULL){
        amp[i] = std::sqrt(std::pow(forwardOutput[i].real, 2.0) + std::pow(forwardOutput[i].imag, 2.0)) + 0.0000001;
      if (std::isnan(amp[i]) || std::isinf(amp[i])){
        amp[i] = 0.0000001;
        fft_errors++;
      }else if (amp[i] > 100){
        fft_errors++;
      };
      power_spectrum[i] = amp[i] * amp[i];
      t_avg_pow += power_spectrum[i];

	  i_amp[i] = (int)amp[i];
	  t_avg_amp += amp[i];
    }
	//printf("[%d]=%.4f,", i, amp[i]);
  }
  //printf("\n");

  int b_rising, b_falling;
  b_rising = 0;
  b_falling = 0;
  t_avg_amp = t_avg_amp / fftsize;

  // Cutting with Avg Threadsold
  for (i = 0; i < fftsize; i++) {
	  if (i_amp[i] <= (int)(t_avg_amp + 1.5))
		  i_amp[i] = 0;
  }

  // Smooth Filter
  for (i = 1; i < fftsize-1; i++) {
	  i_amp[i] = (i_amp[i-1] + i_amp[i])/2;
  }  
  //printf("ffw: ");
  for (i = 0; i < fftsize-1; i++) {
	  if (i_amp[i + 1]>i_amp[i] && b_rising == 0 && b_falling == 0)
		  b_rising = 1;
	  if (b_rising == 1 && i_amp[i + 1] < i_amp[i] && b_falling == 0)
		  b_falling = 1;
	  if (b_rising == 1 && b_falling == 1) {
		  fft_score++;
		  b_rising = 0;
		  b_falling = 0;
	  }
	  //printf("%d,", i_amp[i]);
  }
  //printf("\n");
  //printf("ffw pow = %.8f, avg_amp = %.4f, score = %d\n", t_avg_pow, t_avg_amp, fft_score);

  avg_pow = t_avg_pow / float(fftsize);

  short* sig = new short[windowsize];
  memcpy(sig, signal, sizeof(short) * windowsize);
  signal_history.push_back(sig);
  amp_history.push_back(amp);

  if (signal_history.size() > m_order){
    if(!estimated){
      createNoiseProfile();
      estimated = true;
      mmse = new MmseBasedNpe(fftsize, noise_profile);
    }
    if (mmse != NULL){
      mmse->process(amp);
      mmse->updateNoiseProfile(noise_profile);
    }

    delete[] signal_history[0];
    signal_history.pop_front();
    delete[] amp_history[0];
    amp_history.pop_front();

    bool decision = isSignal();
	if (decision && fft_score > 2)
		decision = 1;
	else
		decision = 0;
    //LOGE("%d", decision);
    updateVadHistories(decision);
    return vadDecision();
  }else{
    updateVadHistories(false);
    return false;
  }
}

bool VadDetector::isSignal(){
  calcLTSE();
  float ltsd = calcLTSD();
  //float e = calcPower();
  float e2 = calcNoisePower();
  //float sn = fabs(e - e2);
  float lamb = (m_lambda0 - m_lambda1) / (m_e0 - m_e1) * e2 + m_lambda0 -
    (m_lambda0 - m_lambda1) / (1.0 - (m_e1 / m_e0));
  float par = 1.0;
  float k = 5.0;
  //float kthresh = 4.0;
  par = parade->process(power_spectrum, avg_pow);
  k = lpcr->process(fft_in);
#ifdef DEBUG
  printf("noise: %f, ltsd: %f, lambda:%f, e0:%f, lpc_k:%f, par:%f\n", e2, ltsd, lamb, m_e0, k, par);
#endif
  if (e2 < m_e0){
    if(ltsd > m_lambda0){
      if (k < m_k0 || par > 0.0){
        return false;
      }else {
        //printf("Signal 1: %f, ltsd: %f, lambda:%f, e0:%f, lpc_k:%f, par:%f\n", e2, ltsd, lamb, m_e0, k, par);
        return true;
      }
    }else{
      return false;
    }
  }else if (e2 > m_e1){
    if(ltsd > m_lambda1){
      if (k < m_k2 || par > 0.0){
        return false;
      }else{
		//printf("Signal 2: %f, ltsd: %f, lambda:%f, e0:%f, lpc_k:%f, par:%f\n", e2, ltsd, lamb, m_e0, k, par);
		return true;
      }
    }else{
      return false;
    }
  }else {
    if (ltsd > lamb) {
      if (k < m_k1 || par > 0.0){
        return false;
      }else {
		//printf("Signal 3: %f, ltsd: %f, lambda:%f, e0:%f, lpc_k:%f, par:%f\n", e2, ltsd, lamb, m_e0, k, par);
		return true;
      }
    } else {
      return false;
    }
  }
}

float VadDetector::calcPower(){
  float* __restrict amp = amp_history.at(amp_history.size() - 1);
  float sum = 0.0;
  for(int i = 0; i < freqsize; i++){
    sum += amp[i] * amp[i];
  }
  return 10 * log10f((sum / float(freqsize)) / ((1.0e-5 * 2.0) * (1.0e-5 * 2.0)));
}

float VadDetector::calcNoisePower(){
  float s = 0.0;
  for(int i = 0; i < freqsize; i++) {
    s += noise_profile[i];
  }
  return 10 * log10f((s / float(freqsize)) / ((1.0e-5 * 2.0) * (1.0e-5 * 2.0)));
}

char* VadDetector::getSignal(){
  if (signal_history.size() != m_order){
    return NULL;
  }else{
    short* src = signal_history.at(signal_history.size() - 1);
    short* dest = new short[windowsize];
    memcpy(dest, src, sizeof(short) * windowsize);
    return (char *)dest;
  }
}

void VadDetector::calcLTSE(){
  int i = 0;
  float amp;
  for(i=0;i < freqsize; i++){
    ltse[i] = 0.0;
  }
  for (std::deque<float*>::iterator ita = amp_history.begin(); ita != amp_history.end(); ita++){
    for(i=0;i < freqsize; i++){
      amp = (*ita)[i];
      if (ltse[i] < amp){
        ltse[i] = amp;
      }
    }
  }
  for(i=0;i < freqsize; i++){
    ltse[i] = ltse[i] * ltse[i];
  }
}

float VadDetector::calcLTSD(){
  float sum = 0.0;
  for(int i = 0; i < freqsize; i++){
    sum += ltse[i] / noise_profile[i];
  }
  return 10 * log10(sum / freqsize);
}

void VadDetector::createNoiseProfile(){
  int i = 0;
  float s = (float)amp_history.size();
  for (std::deque<float*>::iterator ita = amp_history.begin(); ita != amp_history.end(); ita++){
    float* __restrict x = (*ita);
    for(i=0;i < fftsize; i++){
      noise_profile[i] += x[i];
    }
  }
  for(i=0;i < fftsize; i++){
      noise_profile[i] = std::pow(noise_profile[i] / s, 2);
  }
}

void VadDetector::createWindow(){
  window = new float[windowsize];
  if (windowsize == 1){
    window[0] = 1.0;
  }else{
    float n = windowsize -1;
    float coef = M_PI * 2 / float(n);
    for (int i = 0; i < n; i++){
      window[i] = 0.54 - 0.46 * cos(coef * float(i));
    }
  }
}

void VadDetector::updateVadHistories(bool decision){
  for (int i=vad_history_size - 2; i >= 0; i--){
    vad_histories[i + 1] = vad_histories[i];
  }
  vad_histories[0] = decision;
}

bool VadDetector::vadDecision(){
  for(int i=0; i < vad_history_size; i++){
    if (!vad_histories[i]){
      return false;
    }
  }
  return true;
}

void VadDetector::updateParams(double e0, double e1, double lambda0, double lambda1, double k0, double k1, double k2){
  m_e0 = e0;
  m_e1 = e1;
  m_lambda0 = lambda0;
  m_lambda1 = lambda1; 
}

int VadDetector::fftErrors() {
  return fft_errors;
}

void VadDetector::initFFT() {
  CkFftShutdown(context);
  context = CkFftInit(analysissize, kCkFftDirection_Both, NULL, NULL);
}
