/* -*- C++ -*-
 * File: dcraw_emu.cpp
 * Copyright 2008-2010 LibRaw LLC (info@libraw.org)
 * Created: Sun Mar 23,   2008
 *
 * LibRaw simple C++ API sample: almost complete dcraw emulator
 *

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of three licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

3. LibRaw Software License 27032010
   (See file LICENSE.LibRaw.pdf provided in LibRaw distribution archive for details).



 */
#ifdef WIN32
// suppress sprintf-related warning. sprintf() is permitted in sample code
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#ifndef WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#endif

#include "libraw/libraw.h"
#ifdef WIN32
#define snprintf _snprintf
#include <windows.h>
#endif


void usage(const char *prog)
{
    printf("dcraw_emu: almost complete dcraw emulator\n");
    printf("Usage:  %s [OPTION]... [FILE]...\n", prog);
    printf(
"-c float-num       Set adjust maximum threshold (default 0.75)\n"
"-v        Verbose: print progress messages (repeated -v will add verbosity)\n"
"-w        Use camera white balance, if possible\n"
"-a        Average the whole image for white balance\n"
"-A <x y w h> Average a grey box for white balance\n"
"-r <r g b g> Set custom white balance\n"
"+M/-M     Use/don't use an embedded color matrix\n"
"-C <r b>  Correct chromatic aberration\n"
"-P <file> Fix the dead pixels listed in this file\n"
"-K <file> Subtract dark frame (16-bit raw PGM)\n"
"-k <num>  Set the darkness level\n"
"-S <num>  Set the saturation level\n"
"-n <num>  Set threshold for wavelet denoising\n"
"-H [0-9]  Highlight mode (0=clip, 1=unclip, 2=blend, 3+=rebuild)\n"
"-t [0-7]  Flip image (0=none, 3=180, 5=90CCW, 6=90CW)\n"
"-o [0-5]  Output colorspace (raw,sRGB,Adobe,Wide,ProPhoto,XYZ)\n"
#ifndef NO_LCMS
"-o file   Output ICC profile\n"
"-p file   Camera input profile (use \'embed\' for embedded profile)\n"
#endif
"-j        Don't stretch or rotate raw pixels\n"
"-W        Don't automatically brighten the image\n"
"-b <num>  Adjust brightness (default = 1.0)\n"
"-q [0-3]  Set the interpolation quality\n"
"-h        Half-size color image (twice as fast as \"-q 0\")\n"
"-f        Interpolate RGGB as four colors\n"
"-m <num>  Apply a 3x3 median filter to R-G and B-G\n"
"-s [0..N-1] Select one raw image from input file\n"
"-4        Linear 16-bit, same as \"-6 -W -g 1 1\""
"-6        Write 16-bit linear instead of 8-bit with gamma\n"
"-g pow ts Set gamma curve to gamma pow and toe slope ts (default = 2.222 4.5)\n"
"-T        Write TIFF instead of PPM\n"
"-G        Use green_matching() filter\n"
"-B <x y w h> use cropbox\n"
"-F        Use FILE I/O instead of streambuf API\n"
"-d        Detailed timing report\n"
#ifndef WIN32
"-E        Use mmap()-ed buffer instead of plain FILE I/O\n"
#endif
        );
    exit(1);
}

static int verbosity=0;

int cnt=0;
int my_progress_callback(void *d,enum LibRaw_progress p,int iteration, int expected)
{
    char *passed  = (char*)(d?d:"default string"); // data passed to callback at set_callback stage

    if(verbosity>2) // verbosity set by repeat -v switches
        {
            printf("CB: %s  pass %d of %d (data passed=%s)\n",libraw_strprogress(p),iteration,expected,passed);
        }
    else if (iteration == 0) // 1st iteration of each step
        printf("Starting %s (expecting %d iterations)\n", libraw_strprogress(p),expected);
    else if (iteration == expected-1)
        printf("%s finished\n",libraw_strprogress(p));

///    if(++cnt>10) return 1; // emulate user termination on 10-th callback call

    return 0; // always return 0 to continue processing
}

// timer
#ifndef WIN32
static struct timeval start,end;
void timerstart(void)
{
    gettimeofday(&start,NULL);
}
void timerprint(const char *msg,const char *filename)
{
    gettimeofday(&end,NULL);
    float msec = (end.tv_sec - start.tv_sec)*1000.0f + (end.tv_usec - start.tv_usec)/1000.0f;
    printf("Timing: %s/%s: %6.3f msec\n",filename,msg,msec);
}
#else
LARGE_INTEGER start;
void timerstart(void)
{
	QueryPerformanceCounter(&start);
}
void timerprint(const char *msg, const char *filename)
{
	LARGE_INTEGER unit,end;
	QueryPerformanceCounter(&end);
	QueryPerformanceFrequency(&unit);
	float msec = (float)(end.QuadPart - start.QuadPart);
	msec /= (float)unit.QuadPart/1000.0f;
	printf("Timing: %s/%s: %6.3f msec\n",filename,msg,msec);
}

#endif

int main(int argc, char *argv[])
{
    if(argc==1) usage(argv[0]);

    LibRaw RawProcessor;
    int i,arg,c,ret;
    char opm,opt,*cp,*sp;
    int use_bigfile=0, use_timing=0;
#ifndef WIN32
    int msize = 0,use_mmap=0;
    void *iobuffer=0;
#endif

#ifdef OUT
#undef OUT
#endif

#define OUT RawProcessor.imgdata.params
    
  argv[argc] = (char*)"";
  for (arg=1; (((opm = argv[arg][0]) - 2) | 2) == '+'; ) 
      {
          opt = argv[arg++][1];
          if ((cp = strchr (sp=(char*)"cnbrkStqmHABCgU", opt))!=0)
              for (i=0; i < "111411111142"[cp-sp]-'0'; i++)
                  if (!isdigit(argv[arg+i][0])) 
                      {
                          fprintf (stderr,"Non-numeric argument to \"-%c\"\n", opt);
                          return 1;
                      }
          switch (opt) 
              {
              case 'v':  verbosity++;  break;
              case 'G':  OUT.green_matching = 1; break;
              case 'c':  OUT.adjust_maximum_thr   = (float)atof(argv[arg++]);  break;
              case 'U':  OUT.auto_bright_thr   = (float)atof(argv[arg++]);  break;
              case 'n':  OUT.threshold   = (float)atof(argv[arg++]);  break;
              case 'b':  OUT.bright      = (float)atof(argv[arg++]);  break;
              case 'P':  OUT.bad_pixels  = argv[arg++];        break;
              case 'K':  OUT.dark_frame  = argv[arg++];        break;
              case 'r':
                  for(c=0;c<4;c++) 
                      OUT.user_mul[c] = (float)atof(argv[arg++]);  
                  break;
              case 'C':  
                  OUT.aber[0] = 1 / atof(argv[arg++]);
                  OUT.aber[2] = 1 / atof(argv[arg++]);  
                  break;
              case 'g':  
                  OUT.gamm[0] = 1 / atof(argv[arg++]);
                  OUT.gamm[1] =     atof(argv[arg++]);  
                  break;
              case 'k':  OUT.user_black  = atoi(argv[arg++]);  break;
              case 'S':  OUT.user_sat    = atoi(argv[arg++]);  break;
              case 't':  OUT.user_flip   = atoi(argv[arg++]);  break;
              case 'q':  OUT.user_qual   = atoi(argv[arg++]);  break;
              case 'm':  OUT.med_passes  = atoi(argv[arg++]);  break;
              case 'H':  OUT.highlight   = atoi(argv[arg++]);  break;
              case 's':  OUT.shot_select = abs(atoi(argv[arg++])); break;
              case 'o':  
                  if(isdigit(argv[arg][0]) && !isdigit(argv[arg][1]))
                          OUT.output_color = atoi(argv[arg++]);
#ifndef NO_LCMS
                   else
                         OUT.output_profile = argv[arg++];
                  break;
              case 'p':  OUT.camera_profile = argv[arg++];
#endif
                  break;
              case 'h':  OUT.half_size         = 1;		
                  // no break:  "-h" implies "-f" 
              case 'f':  
                  OUT.four_color_rgb    = 1;  
                  break;
              case 'A':  for(c=0; c<4;c++) OUT.greybox[c]  = atoi(argv[arg++]); break;
              case 'B':  for(c=0; c<4;c++) OUT.cropbox[c]  = atoi(argv[arg++]); break;
              case 'a':  OUT.use_auto_wb       = 1;  break;
              case 'w':  OUT.use_camera_wb     = 1;  break;
              case 'M':  OUT.use_camera_matrix = (opm == '+');  break;
              case 'j':  OUT.use_fuji_rotate   = 0;  break;
              case 'W':  OUT.no_auto_bright    = 1;  break;
              case 'T':  OUT.output_tiff       = 1;  break;
              case '4':  OUT.gamm[0] = OUT.gamm[1] =  OUT.no_auto_bright    = 1; /* no break here! */
              case '6':  OUT.output_bps = 16;  break;
              case 'F':  use_bigfile=1; break;
              case 'd':  use_timing=1; break;
#ifndef WIN32
              case 'E':  use_mmap              = 1;  break;
#endif
              default:
                  fprintf (stderr,"Unknown option \"-%c\".\n", opt);
                  return 1;
              }
      }
#ifndef WIN32
  putenv ((char*)"TZ=UTC"); // dcraw compatibility, affects TIFF datestamp field
#else
  _putenv ((char*)"TZ=UTC"); // dcraw compatibility, affects TIFF datestamp field
#endif
  OUT.filtering_mode = LIBRAW_FILTERING_AUTOMATIC;
#define P1 RawProcessor.imgdata.idata
#define S RawProcessor.imgdata.sizes
#define C RawProcessor.imgdata.color
#define T RawProcessor.imgdata.thumbnail
#define P2 RawProcessor.imgdata.other

  if(verbosity>1)
          RawProcessor.set_progress_handler(my_progress_callback,(void*)"Sample data passed");
#ifdef LIBRAW_USE_OPENMP
  if(verbosity)
          printf ("Using %d threads\n", omp_get_max_threads());
#endif

  for ( ; arg < argc; arg++)
        {
            char outfn[1024];

            if(verbosity) printf("Processing file %s\n",argv[arg]);
            
            timerstart();
            
#ifndef WIN32
            if(use_mmap)
                {
                    int file = open(argv[arg],O_RDONLY);
                    struct stat st;
                    if(file<0)
                        {
                            fprintf(stderr,"Cannot open %s: %s\n",argv[arg],strerror(errno));
                            continue;
                        }
                    if(fstat(file,&st))
                        {
                            fprintf(stderr,"Cannot stat %s: %s\n",argv[arg],strerror(errno));
                            close(file);
                            continue;
                        }
                    int pgsz = getpagesize();
                    msize = ((st.st_size+pgsz-1)/pgsz)*pgsz;
                    iobuffer = mmap(NULL,msize,PROT_READ,MAP_PRIVATE,file,0);
                    if(!iobuffer)
                        {
                            fprintf(stderr,"Cannot mmap %s: %s\n",argv[arg],strerror(errno));
                            close(file);
                            continue;
                        }
                    close(file);
                    if( (ret = RawProcessor.open_buffer(iobuffer,st.st_size) != LIBRAW_SUCCESS))
                        {
                            fprintf(stderr,"Cannot open_buffer %s: %s\n",argv[arg],libraw_strerror(ret));
                            continue; // no recycle b/c open file will recycle itself
                        }

                }
            else
#endif
                {
                    if(use_bigfile)
                        // force open_file switch to bigfile processing
                        ret = RawProcessor.open_file(argv[arg],1);
                    else
                        ret = RawProcessor.open_file(argv[arg]);
                        
                    if( ret  != LIBRAW_SUCCESS)
                        {
                            fprintf(stderr,"Cannot open %s: %s\n",argv[arg],libraw_strerror(ret));
                            continue; // no recycle b/c open_file will recycle itself
                        }
                }

            if(use_timing)
                timerprint("LibRaw::open_file()",argv[arg]);


            timerstart();
            if( (ret = RawProcessor.unpack() ) != LIBRAW_SUCCESS)
                {
                    fprintf(stderr,"Cannot unpack %s: %s\n",argv[arg],libraw_strerror(ret));
                    continue;
                }

            if(use_timing)
                timerprint("LibRaw::unpack()",argv[arg]);

            timerstart();
            if (LIBRAW_SUCCESS != (ret = RawProcessor.dcraw_process()))
                {
                    fprintf(stderr,"Cannot do postpocessing on %s: %s\n",argv[arg],libraw_strerror(ret));
                    if(LIBRAW_FATAL_ERROR(ret))
                        continue; 
                }
            if(use_timing)
                timerprint("LibRaw::dcraw_process()",argv[arg]);

            snprintf(outfn,sizeof(outfn),
                     "%s.%s",
                     argv[arg], OUT.output_tiff ? "tiff" : (P1.colors>1?"ppm":"pgm"));

            if(verbosity)
                {
                    printf("Writing file %s\n",outfn);
                }

            if( LIBRAW_SUCCESS != (ret = RawProcessor.dcraw_ppm_tiff_writer(outfn)))
                fprintf(stderr,"Cannot write %s: %s\n",outfn,libraw_strerror(ret));

#ifndef WIN32
            if(use_mmap && iobuffer)
                {
                    munmap(iobuffer,msize);
                    iobuffer=0;
                }
#endif
            
            RawProcessor.recycle(); // just for show this call
        }
    return 0;
}
