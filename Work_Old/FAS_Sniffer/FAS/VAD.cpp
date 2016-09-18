#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "VadDetector.h"

using namespace std;

#define SAMPLE_SIZE 256

#if 1
int main(int argc, char** argv) {
  VadDetector vaddet(SAMPLE_SIZE, 8000, 7);
  int i, j;
  int valid_vad_counter;

  FILE *fp;
  short ss[SAMPLE_SIZE];

  if ((fp = fopen(argv[1], "rb")) == NULL) {
    printf("FileError\n");
    exit(EXIT_FAILURE);
  }

  i = 0;
  valid_vad_counter = 0;
  while (fread(ss, sizeof(short), SAMPLE_SIZE, fp)>0) {
        bool is_signal = vaddet.process((char*)ss);
#if 0
        cout << is_signal << " ";
        i++;
        if(i%16==0)
          cout << endl;
#endif
        if(is_signal)
            valid_vad_counter = valid_vad_counter + 1;
        if(is_signal){
            char *hoge = vaddet.getSignal();
            delete[] hoge;
        }
        memset(ss, 0, sizeof(ss));
  }
#if 0
  cout << endl;
  printf("Counter = %d\n", valid_vad_counter);
#endif
  if(valid_vad_counter>0)
    printf("ValidVoice\n");
  else
    printf("BadVoice\n");
  fclose(fp);
  return 0;
}
#else
int main(int argc, char** argv) {

  int i=0;
  FILE *fp;
  short ss[SAMPLE_SIZE];
  char path[1024];

  while (1)
  {
    VadDetector vaddet(SAMPLE_SIZE, 8000, 7);

    puts("Please input path:");
    gets(path);
    if (strlen(path) == 0)
      break;

    i=0;
    if (path[0] == '\"')
    {
      path[strlen(path)-1] = '\0';
      fp = fopen(&path[1], "rb");
    }
    else
    {
      fp = fopen(path, "rb");
    }

      if (fp == NULL) {
      printf("file open error\n");
      continue;
    }

    while (fread(ss, sizeof(short), SAMPLE_SIZE, fp)>0) {
          bool is_signal = vaddet.process((char*)ss);
          cout << is_signal << " ";
          i++;
          if(i%32==0)
            cout << endl;
          if(is_signal){
              char *hoge = vaddet.getSignal();
              delete[] hoge;
          }
          memset(ss, 0, sizeof(ss));
    }
    cout << endl;
    fclose(fp);
  }

  return 0;
}
#endif
