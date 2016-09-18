#ifndef LPC_H_
#define LPC_H_

// from http://www.musicdsp.org/showone.php?id=137

//find the order-P autocorrelation array, R, for the sequence x of length L and warping of lambda
//wAutocorrelate(&pfSrc[stIndex],siglen,R,P,0);
void wAutocorrelate(float* __restrict x, unsigned int L, float* __restrict R, unsigned int P, float lambda){
  double *dl = new double [L];
  double *Rt = new double [L];
  double r1,r2,r1t;
  R[0]=0;
  Rt[0]=0;
  r1=0;
  r2=0;
  r1t=0;
  for(unsigned int k=0; k<L;k++){
    Rt[0]+=double(x[k])*double(x[k]);
      
    dl[k]=r1-double(lambda)*double(x[k]-r2);
    r1 = x[k];
    r2 = dl[k];
  }
  for(unsigned int i=1; i<=P; i++){
    Rt[i]=0;
    r1=0;
    r2=0;
    for(unsigned int k=0; k<L;k++){
      Rt[i]+=double(dl[k])*double(x[k]);
        
      r1t = dl[k];
      dl[k]=r1-double(lambda)*double(r1t-r2);
      r1 = r1t;
      r2 = dl[k];
    }
  }
  for(int i=0; i<=P; i++){
    R[i]=float(Rt[i]);
  }
  delete[] dl;
  delete[] Rt;
}

// Calculate the Levinson-Durbin recursion for the autocorrelation sequence R of length P+1 and return the autocorrelation coefficients a and reflection coefficients K
int LevinsonRecursion(unsigned int P, float* __restrict R, float* __restrict A, float* __restrict K){
  double Am1[62];
  
  if(R[0]==0.0) { 
    for(unsigned int i=1; i<=P; i++) 
      {
        K[i]=0.0; 
        A[i]=0.0;
      }
  } else {
    double km,Em1,Em;
    unsigned int k,s,m;
    for (k=0;k<=P;k++){
      A[0]=0;
      Am1[0]=0;
    }
    A[0]=1;
    Am1[0]=1;
    km=0;
    Em1=R[0];
    for (m=1;m<=P;m++){   //m=2:N+1
      double err=0.0f;                    //err = 0;
      for (k=1;k<=m-1;k++){            //for k=2:m-1
        err += Am1[k]*R[m-k];        // err = err + am1(k)*R(m-k+1);
      }
      km = (R[m]-err)/Em1;            //km=(R(m)-err)/Em1;
      K[m-1] = -float(km);
      A[m]=(float)km;                        //am(m)=km;
      for (k=1;k<=m-1;k++){            //for k=2:m-1
        A[k]=float(Am1[k]-km*Am1[m-k]);  // am(k)=am1(k)-km*am1(m-k+1);
      }
      Em=(1-km*km)*Em1;                //Em=(1-km*km)*Em1;
      for(s=0;s<=P;s++){                //for s=1:N+1
        Am1[s] = A[s];                // am1(s) = am(s)
      }
      Em1 = Em;                        //Em1 = Em;
    }
  }
  return 0;
}


#endif /* LPC_H_ */
