
#include "voice_activity_detector.h"

CVoiceActivityDetector::CVoiceActivityDetector()
{
	flagVAD = 0;                       // VAD flag (1 == SPEECH, 0 == NON SPEECH)
	memset(noiseMean, 0, sizeof(noiseMean));
	memset(noiseVar, 0, sizeof(noiseVar));

	NoiseEn = 0;

	nbFrame = 0;                       // frame number
	hangOver = 0;                      // hangover
	nbSpeechFrames = 0;                // nb speech frames (used to set hangover)
	meanEn = 0;                        // mean energy

	memset(index, 0, sizeof(index));
	memset(NEn, 0, sizeof(NEn));

	MaxScore = 0, SecScore = 0;
	nFeatureLenth = 0;

	memset(l_Hamming, 0, sizeof(l_Hamming));
	memset(nSample,0,sizeof(nSample));

	mSampleCnt = 0;
}


CVoiceActivityDetector::~CVoiceActivityDetector()
{

}


void CVoiceActivityDetector::VADectorInit()
{
	short	d;

	for (d = 0; d < FiltNum; d++)
	{
		noiseMean[d] = 0.;
		noiseVar[d] = 0.;
	}

	NoiseEn = 0.;

	// *VAD
	nbFrame = 0;
	hangOver = 0;
	nbSpeechFrames = 0;
	meanEn = (float)0.0;
	flagVAD = 0;
}

void CVoiceActivityDetector::VAD(float *newShiftFrame)
{
	int i;

	float	frameEn;
	float	lambdaLTE;

	if (nbFrame < 2000)
		nbFrame++;
	else
		nbFrame = 1;

	if (nbFrame < NS_NB_FRAME_THRESHOLD_LTE)  //10点
		lambdaLTE = 1 - 1 / (float)nbFrame;   //自适应因子系数
	else
		lambdaLTE = NS_LAMBDA_LTE_LOWER_E;    //0.97

	frameEn = BenchEn;//64.0;
	for (i = 0; i<FRAME_SHIFT; i++)
		frameEn += (long)newShiftFrame[i] * newShiftFrame[i];//newShiftFrame存放第一阶段第二帧80点语音起点指针
	frameEn = (float)0.5 + (log(frameEn / BenchEn) / log((float)2.0)) * (float)16.0;

	if (((frameEn - meanEn) < NS_SNR_THRESHOLD_UPD_LTE) || (nbFrame < NS_MIN_FRAME))		//20	10
	{
		if ((frameEn < meanEn) || (nbFrame < NS_MIN_FRAME))	//更新跟踪均值
			meanEn += (1 - lambdaLTE) * (frameEn - meanEn);
		else
			meanEn += (1 - NS_LAMBDA_LTE_HIGHER_E) * (frameEn - meanEn);
		if (meanEn < NS_ENERGY_FLOOR)
			meanEn = NS_ENERGY_FLOOR;
	}

	if (nbFrame > 4)
	{
		if ((frameEn - meanEn) > NS_SNR_THRESHOLD_VAD)		//15
		{
			flagVAD = 1;
			nbSpeechFrames++;
		}
		else
		{
			if (nbSpeechFrames > NS_MIN_SPEECH_FRAME_HANGOVER)	//4
				hangOver = NS_HANGOVER;//程序中是15，但文章中是5
			nbSpeechFrames = 0;
			if (hangOver != 0)
			{
				hangOver--;
				flagVAD = 1;
			}
			else
				flagVAD = 0;
		}
	}
	return;
}

void CVoiceActivityDetector::VADNoiseEstimate(float *SpeechCep, float *nSample, float SpeechEn)
{
	float	lambdaNSE;
	short	d;

	VAD(nSample);
	index[nbFrame] = flagVAD;

	if (flagVAD == 0)
	{
		if (nbFrame < NS_NB_FRAME_THRESHOLD_NSE)
			lambdaNSE = 1 - 1 / (float)nbFrame;
		else
			lambdaNSE = NS_LAMBDA_NSE;

		for (d = 0; d<FiltNum; d++)
		{
			noiseVar[d] = lambdaNSE * (noiseVar[d] + noiseMean[d] * noiseMean[d]) + (1 - lambdaNSE) * SpeechCep[d] * SpeechCep[d];
			noiseMean[d] = lambdaNSE * noiseMean[d] + (1 - lambdaNSE) * SpeechCep[d];
			noiseVar[d] = noiseVar[d] - noiseMean[d] * noiseMean[d];
			if (noiseVar[d] < 0.001)
				noiseVar[d] = 0.001;
		}

		NoiseEn = lambdaNSE*NoiseEn + (1 - lambdaNSE) * SpeechEn;
	}
	NEn[nbFrame] = NoiseEn;
}

void CVoiceActivityDetector::VADGetPara(float *VADnoiseMean, float *VADnoiseVar, int *VADflagVAD, float *VADNoiseEn)
{
	short	d;

	for (d = 0; d<FiltNum; d++)
	{
		VADnoiseMean[d] = noiseMean[d];
		VADnoiseVar[d] = noiseVar[d];
	}

	*VADflagVAD = flagVAD;
	*VADNoiseEn = NoiseEn;
}

void CVoiceActivityDetector::FeatureCollectorReset(void)
{

}

void CVoiceActivityDetector::FFT(double* xr, double* xi, int inv)
{
	int    m, nv2, i, j, nm1, k, l, le1, ip, n = FFTLen;
	double tr, ti, ur, ui, wr, wi, ur1, ui1;

	m = (int)(log((double)n) / log(2.) + .1);
	nv2 = n / 2;
	nm1 = n - 1;
	j = 1;
	for (i = 1; i <= nm1; ++i)
	{
		if (i < j)
		{
			tr = xr[j];
			ti = xi[j];
			xr[j] = xr[i];
			xi[j] = xi[i];
			xr[i] = tr;
			xi[i] = ti;
		}
		k = nv2;
	R20:
		if (k >= j) goto R30;
		j = j - k;
		k = (k / 2);
		goto R20;
	R30:
		j = j + k;
	}

	for (l = 1; l <= m; ++l)
	{
		le1 = (int)(pow((double)2, (l - 1)));
		ur = 1.0f;
		ui = 0.f;
		wr = (double)cos(PI / (double)(le1));
		wi = -(double)sin(PI / (double)(le1));
		if (inv != 0)
			wi = -wi;
		for (j = 1; j <= le1; ++j)
		{
			for (i = j; i <= n; i = i + 2 * le1)
			{
				ip = i + le1;
				tr = xr[ip] * ur - xi[ip] * ui;
				ti = xr[ip] * ui + xi[ip] * ur;
				xr[ip] = xr[i] - tr;
				xi[ip] = xi[i] - ti;
				xr[i] = xr[i] + tr;
				xi[i] = xi[i] + ti;
			}
			ur1 = ur * wr - ui * wi;
			ui1 = ur * wi + ui * wr;
			ur = ur1;
			ui = ui1;
		}
	}

	if (inv == 0)
		return;

	for (i = 1; i <= n; ++i)
	{
		xr[i] = xr[i] / (double)(n);
		xi[i] = xi[i] / (double)(n);
	}
}

void CVoiceActivityDetector::insert(MAXINDEX *index, MAXINDEX tmpindex, short num)
{
	short i, ii;

	for (i = 0; i < num; i++)
	{
		if (index[i].prob < tmpindex.prob)
			break;
	}

	if (i != num)
	{
		for (ii = num; ii > i; ii--)
			index[ii] = index[ii - 1];

		index[i] = tmpindex;
	}
}

void CVoiceActivityDetector::sort(MAXINDEX *index, short num)
{
	short	i, j;
	MAXINDEX	tmp;

	for (i = 0; i < num - 1; i++)
	{
		for (j = i + 1; j < num; j++)
		{
			if (index[i].prob<index[j].prob)
			{
				tmp = index[i];
				index[i] = index[j];
				index[j] = tmp;
			}
		}
	}
}

int CVoiceActivityDetector::NoiseEstimate(float *nSample, float *speechSpectrum){
	static int prev_vad = -1;
	float	Energy;
	short	id;
	float VADnoiseMean[NO_DIM];
	float VADnoiseVar[NO_DIM];
	int VADflagVAD;
	float VADNoiseEn;

	Energy = 0.;
	for (id = 0; id < FRAMELEN; id++){
		Energy += nSample[id] * nSample[id];
	}

	VADNoiseEstimate(speechSpectrum, nSample, Energy);
	VADGetPara(VADnoiseMean, VADnoiseVar, &VADflagVAD, &VADNoiseEn);

	//if (prev_vad != VADflagVAD)
	{
		//printf("%d\n", VADflagVAD);
		prev_vad = VADflagVAD;
	}
	return VADflagVAD;
}


int CVoiceActivityDetector::VADCalculate()
{
	double	real[FFTLen + 1], imgi[FFTLen + 1];
	int		id;
	double	speechSpectrum[FFTLen / 2 + 1];
	float	old_speechSpectrum[FFTLen / 2 + 1];

	for (id = 0; id < FFTLen; id++)
	{
		if (id < FRAMELEN)
			real[id + 1] = (double)nSample[id];
		else
			real[id + 1] = (double) 0.0;
		imgi[id + 1] = (double) 0.0;
	}
	FFT(real, imgi, 0);

	for (id = 0; id <= FFTLen / 2; id++)
		speechSpectrum[id] = real[id + 1] * real[id + 1] + imgi[id + 1] * imgi[id + 1];

	for (id = 0; id <= FFTLen / 2; id++)
		old_speechSpectrum[id] = speechSpectrum[id];
	return NoiseEstimate(nSample, old_speechSpectrum);
}

void CVoiceActivityDetector::GenerateHamming(void)
{
	short	i;

	for (i = 0; i<FRAMELEN + 1; i++)
	{
		l_Hamming[i] = 0.54 - 0.46*cos((double)i*twopi / (double)(FRAMELEN - 1));
	}
}

void CVoiceActivityDetector::ApplyHammingWindow()
{
	short	j;
	for (j = 0; j<FRAMELEN; j++)
		nSample[j] = nSample[j] * l_Hamming[j];
}

void CVoiceActivityDetector::Init()
{
	int i;
	GenerateHamming();
	VADectorInit();
}

int CVoiceActivityDetector::Detect(int count,short samples[]) // 256 samples
{
	int i,left;
	int rtotal = 0, rvad = 0;
	int j = 0;

	left = min(FRAMELEN - mSampleCnt, count);
	for (i = 0; i < left; i++)
		nSample[i + mSampleCnt] = samples[i];

	if (mSampleCnt + left < FRAMELEN)
	{
		mSampleCnt += left;
		return 0;
	}

	ApplyHammingWindow();
	rvad += VADCalculate();
	rtotal++;
	j = left;

	while ((count - j) >= FRAMELEN )
	{
		for (i = 0; i < FRAMELEN; i++,j++)
			nSample[i] = samples[j];
		ApplyHammingWindow();
		rvad += VADCalculate();
		rtotal++;
	}

	mSampleCnt = count - j;
	for (i = 0; i < mSampleCnt; i++, j++)
		nSample[i] = samples[j];

	if (rvad >= (rtotal+1) / 2)
		return 1;
	return 0;
}
