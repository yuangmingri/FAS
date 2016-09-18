
#ifndef __VOICE_ACTIVITY_DETECTOR_H__
#define __VOICE_ACTIVITY_DETECTOR_H__

#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include <stdlib.h>

#ifndef min
#define min(a,b) ((a) > (b) ? (b) : (a))
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#define	NO_DIM					(FFTLen/2+1)

#define	FRAMELEN				256
#define	FRAMESHIFT				128

#define	MAXFRAMELEN				512

#define	ModelNum				10//61
#define	cTrainSample			10//58

#define cPre                    0.97f
#define	nLeft					(1<<14)
#define	twopi					6.2831853071795864

#define negtive_infinit         -2147483647


#define	FiltNum					129
#define FRAME_SHIFT				128//80

const	float	BenchEn = (float)((float)(64. / 80 * FRAME_SHIFT));

#define	NS_NB_FRAME_THRESHOLD_NSE	10
#define NS_NB_FRAME_THRESHOLD_LTE	10

const	float	NS_LAMBDA_NSE = (float)(0.99f);
const	float	NS_LAMBDA_LTE_LOWER_E = (float)(0.97f);
//#define NS_LAMBDA_LTE_LOWER_E	(float)0.97
//#define NS_SNR_THRESHOLD_UPD_LTE	20

const	float	NS_SNR_THRESHOLD_UPD_LTE = (float)(20.f);
#define NS_MIN_FRAME			10
//#define NS_LAMBDA_LTE_HIGHER_E	(float)0.99
const	float	NS_LAMBDA_LTE_HIGHER_E = (float)(0.99f);
//#define NS_ENERGY_FLOOR			(float)80.0
const	float	NS_ENERGY_FLOOR = (float)(80.0f);
//#define NS_SNR_THRESHOLD_VAD	(int)15
const	float	NS_SNR_THRESHOLD_VAD = (float)(100L);
#define NS_MIN_SPEECH_FRAME_HANGOVER	(int)4
#define NS_HANGOVER				(int)5

#define		FFTLen			256
#define		PI				3.14159265254
#define		FS				8

typedef	struct{
	double	prob;
	short	index;
}MAXINDEX;


class CVoiceActivityDetector
{
private:
	int		flagVAD;                       // VAD flag (1 == SPEECH, 0 == NON SPEECH)
	float	noiseMean[FiltNum], noiseVar[FiltNum];
	float	NoiseEn;

	int		nbFrame;                   // frame number
	int		hangOver;                      // hangover
	int		nbSpeechFrames;                // nb speech frames (used to set hangover)
	float	meanEn;                        // mean energy
	short	index[2000];
	float	NEn[2000];


	double	MaxScore, SecScore;
	long	nFeatureLenth;

	double	l_Hamming[FRAMELEN + 1];
	float	nSample[FRAMELEN];
	int		mSampleCnt;

public:
	CVoiceActivityDetector();
	~CVoiceActivityDetector();
protected:
	void VADectorInit();
	void VAD(float *newShiftFrame);
	void VADNoiseEstimate(float *SpeechCep, float *nSample, float SpeechEn);
	void VADGetPara(float *VADnoiseMean, float *VADnoiseVar, int *VADflagVAD, float *VADNoiseEn);

	void FeatureCollectorReset(void);
	void FFT(double* xr, double* xi, int inv);
	void insert(MAXINDEX *index, MAXINDEX tmpindex, short num);
	void sort(MAXINDEX *index, short num);
	int NoiseEstimate(float *nSample, float *speechSpectrum);
	int VADCalculate();
	void GenerateHamming(void);
	void ApplyHammingWindow();
public:

	void Init();
	int Detect(int count,short samples[]); // 256 samples

};

#endif // __VOICE_ACTIVITY_DETECTOR_H__
