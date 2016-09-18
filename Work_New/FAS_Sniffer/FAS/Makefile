CC :=g++
AR :=ar
RANLIB :=ranlib
STRIP :=strip

CFLAGS :=  -I.
LDFLAGS := -L.

CKFFT_SRC_FILES := \
    ckfft/debug.cpp \
    ckfft/context.cpp \
    ckfft/ckfft.cpp \
    ckfft/fft.cpp \
    ckfft/fft_default.cpp \
    ckfft/fft_real.cpp \
    ckfft/fft_real_default.cpp \
    ckfft/fft_neon.cpp \
    ckfft/fft_real_neon.cpp

CKFFT_OBJS := $(CKFFT_SRC_FILES:.cpp=.o)

CFLAGS += -D__LINUX__

all: libVadDetector.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o fas VAD.cpp -lVadDetector -lm

libVadDetector.a: lpcr.o parade.o vaddetector.o ms.o mmse.o $(CKFFT_OBJS)
	$(AR) cr $@ lpcr.o parade.o ms.o mmse.o vaddetector.o $(CKFFT_OBJS)
	$(RANLIB) $@

vaddetector.o: VadDetector.cpp
	$(CC) $(CFLAGS) -o vaddetector.o -c VadDetector.cpp

ms.o: MinimumStatistics.cpp
	$(CC) $(CFLAGS) -o ms.o -c MinimumStatistics.cpp

mmse.o: MmseBasedNpe.cpp
	$(CC) $(CFLAGS) -o mmse.o -c MmseBasedNpe.cpp

lpcr.o:LPCResidual.cpp
	$(CC) $(CFLAGS) -o lpcr.o -c LPCResidual.cpp

parade.o: PARADE.cpp
	$(CC) $(CFLAGS) -o parade.o -c PARADE.cpp


ckfft/debug.o: ckfft/debug.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

ckfft/context.o: ckfft/context.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

ckfft/ckfft.o: ckfft/ckfft.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

ckfft/fft.o: ckfft/fft.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

ckfft/fft_default.o: ckfft/fft_default.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

ckfft/fft_real.o: ckfft/fft_real.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

ckfft/fft_real_default.o: ckfft/fft_real_default.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

ckfft/fft_neon.o: ckfft/fft_neon.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

ckfft/fft_real_neon.o: ckfft/fft_real_neon.cpp
	$(CC) $(CFLAGS) -o $@ -c $<


clean:
	rm -rf *.so
	rm -rf *.o
	rm -rf *.a
	rm -rf ckfft/*.o
	rm -rf fas fas.exe
