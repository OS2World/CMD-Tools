/********************************************/
/* DCF Decoder for radioclock receiver      */
/*------------------------------------------*/
/* serial port, power on RTS, signal on DSR */
/* for other hardware change macros         */
/* INIT (power) and POOL (signal)           */
/*------------------------------------------*/
/* B.Horeni 1999            horeni@login.cz */
/********************************************/
 
#include<sys/hw.h>
#include<getopt.h>
#include<time.h>

#define VERSION "1.0.1"
#define OS ""

#define DCF_OS2

int yearbase=2000;

#define set_time(t)

#ifdef DCF_OS2
#include<os2.h>
#undef set_time
#undef OS
#define OS "OS/2"

int set_time(struct tm *t)
//------------------------
{ DATETIME dt;
  dt.hours      = t -> tm_hour;
  dt.minutes    = t -> tm_min;
  dt.seconds    = t -> tm_sec;
  dt.hundredths = 0;
  dt.day        = t -> tm_mday;
  dt.month      = t -> tm_mon  + 1;
  dt.year       = t -> tm_year + 1900;
  dt.weekday    = t -> tm_wday;
  dt.timezone   = -1;

  return DosSetDateTime(&dt);
}
#endif

void txt_out_dcf(struct tm *t, int err, int set)
//----------------------------------------------
{ printf("%02i:%02i %02i.%02i.%04i %i ",
          t->tm_hour,t->tm_min,
          t->tm_mday,t->tm_mon+1,t->tm_year+1900,
          t->tm_wday);
  if(err)printf("%x",err);
  if(set)printf("*"); 
  printf("\n");
}

#define SB 20
#define P1 28
#define P2 35
#define P3 58

void txt_out_bit(int index, int bit)
//----------------------------------
{ int spec=(index==SB||index==P1||index==P2||index==P3)?1:0;
  if(index<59)printf("%c",bit?(spec?'|':'#'):(spec?'_':'.'));
  else printf("\n");
}

void txt_out_slp(int tmin)
//------------------------
{ if(tmin)printf("sleep %i min...",tmin);
  else printf("done.");
}

void (*out_dcf)(struct tm *t, int err, int set)=txt_out_dcf;
void (*out_bit)(int index, int bit)            =txt_out_bit;
void (*out_slp)(int tmin)                      =txt_out_slp;
//==========================================================


int bcd(int *B,int n)
//-------------------
{ int BCD[8]={1,2,4,8,10,20,40,80},rc=0,i;
  for(i=0;i<n;i++)rc+=BCD[i]*B[i];
  return rc;
}

int par(int *B,int n) 
//------------------- 
{ int rc=0,i;
  for(i=0;i<n;i++)rc+=B[i];
  return rc%2;
}

int eval_dcf(struct tm *t, int B[59])
//-----------------------------------
{ int rc;
  t->tm_min     = bcd(B+21,7);
  t->tm_hour    = bcd(B+29,6);
  t->tm_mday    = bcd(B+36,6);
  t->tm_wday    = bcd(B+42,3);
  t->tm_mon     = bcd(B+45,5)-1;
  t->tm_year    = bcd(B+50,8)+yearbase-1900;
  rc=par(B+P2+1,P3-P2)<<2|par(B+P1+1,P2-P1)<<1|par(B+SB+1,P1-SB);     
  return rc;
}

#define TRESHOLD 0.15

int eval_bit(double dt)
//---------------------
{ int bit=dt>TRESHOLD?1:0;
  return bit;
}

#define COM1     0x3f8
#define COM2     0x2f8
#define COM3     0x3e8
#define COM4     0x2e8
#define STAT(B)  ((B)+6)
#define MCR(B)   ((B)+4)
#define INIDATA  0x02    /* RTS (power on)   */
#define MASK     0x20    /* DSR (dcf signal) */

#define POOL(B)  (_inp8(STAT(B))&MASK?1:0)
#define INIT(B)  { _portaccess((B),(B)+7); _outp8(MCR(B),INIDATA); }

#define SYST()   (clock()/(double)CLOCKS_PER_SEC)

struct dcf_init { int base;
                  int noks;
                  int slpm;
                };

#define MAX_NOKS 60

void dcf_loop(void *param)  
//------------------------
{ struct dcf_init *DCF=(struct dcf_init *)param;
  int i,k=0,d,D[60],on,nok=0,npool=0,min[MAX_NOKS+10];
  double st; 
  INIT(DCF->base);

  on=POOL(DCF->base);
  st=SYST();

  while(1)
     { d=POOL(DCF->base);
       if(d!=on) 
         { double t=SYST();
           double dt=t-st;
           if(dt>1.0) { if(k==59)
                         { struct tm T;
                           int err=eval_dcf(&T,D),set=0,sleep_min=DCF->slpm;
                           if(err)nok=0;   
                           else { min[nok]=T.tm_min; 
                                  if(nok++)
                                    { int oldm=min[nok-2],
                                          newm=min[nok-1],dmin;
                                      dmin=newm-oldm;
                                      if(dmin<0)dmin+=60;
                                      if(dmin!=1){min[0]=newm;nok=1;}  
                                    }   
                                }
                           if(nok>=DCF->noks)
                             { set=1; set_time(&T); 
                               nok=0;
                             }
                           out_dcf(&T,err,set); 
                           if(set&&sleep_min)
                            { out_slp(sleep_min);
                              sleep(50);
                              while(--sleep_min)sleep(60);
                              t=SYST();
                              on=0;
                              d=0; 
                              out_slp(0);
                            } 
                         }
                        else out_bit(59,0);
                        k=0; 
                      }
           if(on)     { int bit=eval_bit(dt); 
                        if(k<60){ D[k]=bit; out_bit(k,bit); k++; }
                      } 
           on=d;
           st=t;
         }
       _sleep2(1);  /* cca 1/32[sec] (scheduler) */
     } 
}

void version()
//------------
{ printf("DCF Decoder for radioclock receiver ver.:%s %s (%s)\n", 
          VERSION,OS,__DATE__);
}

void start_info(struct dcf_init *DCF)
//-----------------------------------
{ version();
  printf("Options: -p%03x -n%i -s%i , for help type: dcf -h\n", 
          DCF->base,DCF->noks,DCF->slpm);
}

#define DEFAULT_PORT COM2
#define DEFAULT_NOKS 2
#define DEFAULT_SLPM 15

void usage()
//----------
{ version();
  printf("usage:  dcf [-p#] [-n#] [-s#]\n"
         "        p...com port base address (hex) (default 0x%03x)\n"  
         "            standard com1-4 :  0x%03x,0x%03x,0x%03x,0x%03x\n" 
         "        n...requiered sucessfull blocks (default %5i)\n"  
         "        s...sleep time [min]            (default %5i)\n"
         "sample: dcf -p3f8 -n3 -s30\n",
         DEFAULT_PORT,COM1,COM2,COM3,COM4,DEFAULT_NOKS,DEFAULT_SLPM);
}

int main(int argc, char *argv[])
//==============================
{ struct dcf_init DCF;
  int             opt,x;

  DCF.base=DEFAULT_PORT;   /* port */
  DCF.noks=DEFAULT_NOKS;   /* requiered sucessfull blocks */ 
  DCF.slpm=DEFAULT_SLPM;   /* sleep time [min] */

  while((opt=getopt(argc,argv,"p:n:s:h"))>=0)
   switch(opt)
     { case 'p': if(sscanf(optarg,"%x",&x))DCF.base=x;     break;
       case 'n': if(sscanf(optarg,"%i",&x))DCF.noks=x;     break;
       case 's': if(sscanf(optarg,"%i",&x))DCF.slpm=x;     break;
       case 'h': usage();                                  return 0;
       default:;
     }
  if(DCF.noks>MAX_NOKS)DCF.noks=MAX_NOKS;
  start_info(&DCF);
  dcf_loop(&DCF);
}