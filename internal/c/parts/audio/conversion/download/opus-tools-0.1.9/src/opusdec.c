/* Copyright (c) 2002-2007 Jean-Marc Valin
   Copyright (c) 2008 CSIRO
   Copyright (c) 2007-2013 Xiph.Org Foundation
   File: opusdec.c

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#if !defined WIN32 && !defined _WIN32
#include <unistd.h>
#endif

#include <getopt.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h> /*tolower()*/

#include <opus.h>
#include <opus_multistream.h>
#include <ogg/ogg.h>

#if defined WIN32 || defined _WIN32 || defined WIN64 || defined _WIN64
# include "unicode_support.h"
# include "wave_out.h"
/* We need the following two to set stdout to binary */
# include <io.h>
# include <fcntl.h>
# define I64FORMAT "I64d"
#else
# define I64FORMAT "lld"
# define fopen_utf8(_x,_y) fopen((_x),(_y))
# define argc_utf8 argc
# define argv_utf8 argv
#endif

#include <math.h>

#ifdef HAVE_LRINTF
# define float2int(x) lrintf(x)
#else
# define float2int(flt) ((int)(floor(.5+flt)))
#endif

#if defined HAVE_LIBSNDIO
# include <sndio.h>
#elif defined HAVE_SYS_SOUNDCARD_H || defined HAVE_MACHINE_SOUNDCARD_H || HAVE_SOUNDCARD_H
# ifdef HAVE_SYS_SOUNDCARD_H
#  include <sys/soundcard.h>
# elif HAVE_MACHINE_SOUNDCARD_H
#  include <machine/soundcard.h>
# else
#  include <soundcard.h>
# endif
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <sys/ioctl.h>
#elif defined HAVE_SYS_AUDIOIO_H
# include <sys/types.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/audioio.h>
# ifndef AUDIO_ENCODING_SLINEAR
#  define AUDIO_ENCODING_SLINEAR AUDIO_ENCODING_LINEAR /* Solaris */
# endif
#endif

#include <string.h>
#include "wav_io.h"
#include "opus_header.h"
#include "diag_range.h"
#include "speex_resampler.h"
#include "stack_alloc.h"
#include "cpusupport.h"

#define MINI(_a,_b)      ((_a)<(_b)?(_a):(_b))
#define MAXI(_a,_b)      ((_a)>(_b)?(_a):(_b))
#define CLAMPI(_a,_b,_c) (MAXI(_a,MINI(_b,_c)))

/* 120ms at 48000 */
#define MAX_FRAME_SIZE (960*6)

#define readint(buf, base) (((buf[base+3]<<24)&0xff000000)| \
                           ((buf[base+2]<<16)&0xff0000)| \
                           ((buf[base+1]<<8)&0xff00)| \
                           (buf[base]&0xff))

#ifdef HAVE_LIBSNDIO
struct sio_hdl *hdl;
#endif

typedef struct shapestate shapestate;
struct shapestate {
  float * b_buf;
  float * a_buf;
  int fs;
  int mute;
};

static unsigned int rngseed = 22222;
static inline unsigned int fast_rand(void) {
  rngseed = (rngseed * 96314165) + 907633515;
  return rngseed;
}

#ifndef HAVE_FMINF
# define fminf(_x,_y) ((_x)<(_y)?(_x):(_y))
#endif

#ifndef HAVE_FMAXF
# define fmaxf(_x,_y) ((_x)>(_y)?(_x):(_y))
#endif

static void quit(int _x) {
#ifdef WIN_UNICODE
  uninit_console_utf8();
#endif
  exit(_x);
}

/* This implements a 16 bit quantization with full triangular dither
   and IIR noise shaping. The noise shaping filters were designed by
   Sebastian Gesemann based on the LAME ATH curves with flattening
   to limit their peak gain to 20 dB.
   (Everyone elses' noise shaping filters are mildly crazy)
   The 48kHz version of this filter is just a warped version of the
   44.1kHz filter and probably could be improved by shifting the
   HF shelf up in frequency a little bit since 48k has a bit more
   room and being more conservative against bat-ears is probably
   more important than more noise suppression.
   This process can increase the peak level of the signal (in theory
   by the peak error of 1.5 +20 dB though this much is unobservable rare)
   so to avoid clipping the signal is attenuated by a couple thousandths
   of a dB. Initially the approach taken here was to only attenuate by
   the 99.9th percentile, making clipping rare but not impossible (like
   SoX) but the limited gain of the filter means that the worst case was
   only two thousandths of a dB more, so this just uses the worst case.
   The attenuation is probably also helpful to prevent clipping in the DAC
   reconstruction filters or downstream resampling in any case.*/
static inline void shape_dither_toshort(shapestate *_ss, short *_o, float *_i, int _n, int _CC)
{
  const float gains[3]={32768.f-15.f,32768.f-15.f,32768.f-3.f};
  const float fcoef[3][8] =
  {
    {2.2374f, -.7339f, -.1251f, -.6033f, 0.9030f, .0116f, -.5853f, -.2571f}, /* 48.0kHz noise shaping filter sd=2.34*/
    {2.2061f, -.4706f, -.2534f, -.6214f, 1.0587f, .0676f, -.6054f, -.2738f}, /* 44.1kHz noise shaping filter sd=2.51*/
    {1.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f,0.0000f, 0.0000f, 0.0000f}, /* lowpass noise shaping filter sd=0.65*/
  };
  int i;
  int rate=_ss->fs==44100?1:(_ss->fs==48000?0:2);
  float gain=gains[rate];
  float *b_buf;
  float *a_buf;
  int mute=_ss->mute;
  b_buf=_ss->b_buf;
  a_buf=_ss->a_buf;
  /*In order to avoid replacing digital silence with quiet dither noise
    we mute if the output has been silent for a while*/
  if(mute>64)
    memset(a_buf,0,sizeof(float)*_CC*4);
  for(i=0;i<_n;i++)
  {
    int c;
    int pos = i*_CC;
    int silent=1;
    for(c=0;c<_CC;c++)
    {
      int j, si;
      float r,s,err=0;
      silent&=_i[pos+c]==0;
      s=_i[pos+c]*gain;
      for(j=0;j<4;j++)
        err += fcoef[rate][j]*b_buf[c*4+j] - fcoef[rate][j+4]*a_buf[c*4+j];
      memmove(&a_buf[c*4+1],&a_buf[c*4],sizeof(float)*3);
      memmove(&b_buf[c*4+1],&b_buf[c*4],sizeof(float)*3);
      a_buf[c*4]=err;
      s = s - err;
      r=(float)fast_rand()*(1/(float)UINT_MAX) - (float)fast_rand()*(1/(float)UINT_MAX);
      if (mute>16)r=0;
      /*Clamp in float out of paranoia that the input will be >96 dBFS and wrap if the
        integer is clamped.*/
      _o[pos+c] = si = float2int(fmaxf(-32768,fminf(s + r,32767)));
      /*Including clipping in the noise shaping is generally disastrous:
        the futile effort to restore the clipped energy results in more clipping.
        However, small amounts-- at the level which could normally be created by
        dither and rounding-- are harmless and can even reduce clipping somewhat
        due to the clipping sometimes reducing the dither+rounding error.*/
      b_buf[c*4] = (mute>16)?0:fmaxf(-1.5f,fminf(si - s,1.5f));
    }
    mute++;
    if(!silent)mute=0;
  }
  _ss->mute=MINI(mute,960);
}

static void print_comments(char *comments, int length)
{
   char *c=comments;
   int len, i, nb_fields, err=0;

   if (length<(8+4+4))
   {
      fprintf (stderr, "Invalid/corrupted comments\n");
      return;
   }
   if (strncmp(c, "OpusTags", 8) != 0)
   {
      fprintf (stderr, "Invalid/corrupted comments\n");
      return;
   }
   c += 8;
   fprintf(stderr, "Encoded with ");
   len=readint(c, 0);
   c+=4;
   if (len < 0 || len>(length-16))
   {
      fprintf (stderr, "Invalid/corrupted comments\n");
      return;
   }
   err&=fwrite(c, 1, len, stderr)!=(unsigned)len;
   c+=len;
   fprintf (stderr, "\n");
   /*The -16 check above makes sure we can read this.*/
   nb_fields=readint(c, 0);
   c+=4;
   length-=16+len;
   if (nb_fields < 0 || nb_fields>(length>>2))
   {
      fprintf (stderr, "Invalid/corrupted comments\n");
      return;
   }
   for (i=0;i<nb_fields;i++)
   {
      if (length<4)
      {
         fprintf (stderr, "Invalid/corrupted comments\n");
         return;
      }
      len=readint(c, 0);
      c+=4;
      length-=4;
      if (len < 0 || len>length)
      {
         fprintf (stderr, "Invalid/corrupted comments\n");
         return;
      }
      err&=fwrite(c, 1, len, stderr)!=(unsigned)len;
      c+=len;
      length-=len;
      fprintf (stderr, "\n");
   }
}

FILE *out_file_open(char *outFile, int *wav_format, int rate, int mapping_family, int *channels, int fp)
{
   FILE *fout=NULL;
   /*Open output file*/
   if (strlen(outFile)==0)
   {
#if defined HAVE_LIBSNDIO
      struct sio_par par;

      hdl = sio_open(NULL, SIO_PLAY, 0);
      if (!hdl)
      {
         fprintf(stderr, "Cannot open sndio device\n");
         quit(1);
      }

      sio_initpar(&par);
      par.sig = 1;
      par.bits = 16;
      par.rate = rate;
      par.pchan = *channels;

      if (!sio_setpar(hdl, &par) || !sio_getpar(hdl, &par) ||
        par.sig != 1 || par.bits != 16 || par.rate != rate) {
          fprintf(stderr, "could not set sndio parameters\n");
          quit(1);
      }
      *channels = par.pchan;
      if (!sio_start(hdl)) {
          fprintf(stderr, "could not start sndio\n");
          quit(1);
      }
#elif defined HAVE_SYS_SOUNDCARD_H || defined HAVE_MACHINE_SOUNDCARD_H || HAVE_SOUNDCARD_H
      int audio_fd, format, stereo;
      audio_fd=open("/dev/dsp", O_WRONLY);
      if (audio_fd<0)
      {
         perror("Cannot open /dev/dsp");
         quit(1);
      }

      format=AFMT_S16_NE;
      if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format)==-1)
      {
         perror("SNDCTL_DSP_SETFMT");
         close(audio_fd);
         quit(1);
      }

      if (*channels > 2)
      {
        /* There doesn't seem to be a way to get or set the channel
         * matrix with the sys/soundcard api, so we can't support
         * multichannel. We should fall back to stereo downmix.
         */
        fprintf(stderr, "Cannot configure multichannel playback."
                        " Try decoding to a file instead.\n");
        close(audio_fd);
        quit(1);
      }
      stereo=0;
      if (*channels==2)
         stereo=1;
      if (ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo)==-1)
      {
         perror("SNDCTL_DSP_STEREO");
         close(audio_fd);
         quit(1);
      }
      if (stereo!=0)
      {
         if (*channels==1)
            fprintf (stderr, "Cannot set mono mode, will decode in stereo\n");
         *channels=2;
      }

      if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate)==-1)
      {
         perror("SNDCTL_DSP_SPEED");
         close(audio_fd);
         quit(1);
      }
      fout = fdopen(audio_fd, "w");
      if(!fout)
      {
        perror("Cannot open output");
        quit(1);
      }
#elif defined HAVE_SYS_AUDIOIO_H
      audio_info_t info;
      int audio_fd;

      audio_fd = open("/dev/audio", O_WRONLY);
      if (audio_fd<0)
      {
         perror("Cannot open /dev/audio");
         quit(1);
      }

      AUDIO_INITINFO(&info);
#ifdef AUMODE_PLAY    /* NetBSD/OpenBSD */
      info.mode = AUMODE_PLAY;
#endif
      info.play.encoding = AUDIO_ENCODING_SLINEAR;
      info.play.precision = 16;
      info.play.input_sample_rate = rate;
      info.play.channels = *channels;

      if (ioctl(audio_fd, AUDIO_SETINFO, &info) < 0)
      {
         perror ("AUDIO_SETINFO");
         quit(1);
      }
      fout = fdopen(audio_fd, "w");
      if(!fout)
      {
        perror("Cannot open output");
        quit(1);
      }
#elif defined WIN32 || defined _WIN32
      {
         unsigned int opus_channels = *channels;
         if (Set_WIN_Params (INVALID_FILEDESC, rate, SAMPLE_SIZE, opus_channels))
         {
            fprintf (stderr, "Can't access %s\n", "WAVE OUT");
            quit(1);
         }
      }
#else
      fprintf (stderr, "No soundcard support\n");
      quit(1);
#endif
   } else {
      if (strcmp(outFile,"-")==0)
      {
#if defined WIN32 || defined _WIN32
         _setmode(_fileno(stdout), _O_BINARY);
#endif
         fout=stdout;
      }
      else
      {
         fout = fopen_utf8(outFile, "wb");
         if (!fout)
         {
            perror(outFile);
            quit(1);
         }
      }
      if (*wav_format)
      {
         *wav_format = write_wav_header(fout, rate, mapping_family, *channels, fp);
         if (*wav_format < 0)
         {
            fprintf (stderr, "Error writing WAV header.\n");
            quit(1);
         }
      }
   }
   return fout;
}

void usage(void)
{
   printf ("Usage: opusdec [options] input_file.opus [output_file]\n");
   printf ("\n");
   printf ("Decodes a Opus file and produce a WAV file or raw file\n");
   printf ("\n");
   printf ("input_file can be:\n");
   printf ("  filename.opus        regular Opus file\n");
   printf ("  -                    stdin\n");
   printf ("\n");
   printf ("output_file can be:\n");
   printf ("  filename.wav         Wav file\n");
   printf ("  filename.*           Raw PCM file (any extension other than .wav)\n");
   printf ("  -                    stdout (raw; unless --force-wav)\n");
   printf ("  (nothing)            Will be played to soundcard\n");
   printf ("\n");
   printf ("Options:\n");
   printf (" --rate n              Force decoding at sampling rate n Hz\n");
   printf (" --gain n              Manually adjust gain by n.nn dB (0 default)\n");
   printf (" --no-dither           Do not dither 16-bit output\n");
   printf (" --float               32-bit floating-point output\n");
   printf (" --force-wav           Force wav header on output\n");
   printf (" --packet-loss n       Simulate n %% random packet loss\n");
   printf (" --save-range file     Saves check values for every frame to a file\n");
   printf (" -h, --help            This help\n");
   printf (" -V, --version         Version information\n");
   printf (" --quiet               Quiet mode\n");
   printf ("\n");
}

void version(void)
{
   printf("opusdec %s %s (using %s)\n",PACKAGE_NAME,PACKAGE_VERSION,opus_get_version_string());
   printf("Copyright (C) 2008-2013 Xiph.Org Foundation\n");
}

void version_short(void)
{
   version();
}

/*Process an Opus header and setup the opus decoder based on it.
  It takes several pointers for header values which are needed
  elsewhere in the code.*/
static OpusMSDecoder *process_header(ogg_packet *op, opus_int32 *rate,
       int *mapping_family, int *channels, int *preskip, float *gain,
       float manual_gain, int *streams, int wav_format, int quiet)
{
   int err;
   OpusMSDecoder *st;
   OpusHeader header;

   if (opus_header_parse(op->packet, op->bytes, &header)==0)
   {
      fprintf(stderr, "Cannot parse header\n");
      return NULL;
   }

   *mapping_family = header.channel_mapping;
   *channels = header.channels;
   if(wav_format)adjust_wav_mapping(*mapping_family, *channels, header.stream_map);

   if(!*rate)*rate=header.input_sample_rate;
   /*If the rate is unspecified we decode to 48000*/
   if(*rate==0)*rate=48000;
   if(*rate<8000||*rate>192000){
     fprintf(stderr,"Warning: Crazy input_rate %d, decoding to 48000 instead.\n",*rate);
     *rate=48000;
   }

   *preskip = header.preskip;
   st = opus_multistream_decoder_create(48000, header.channels, header.nb_streams, header.nb_coupled, header.stream_map, &err);
   if(err != OPUS_OK){
     fprintf(stderr, "Cannot create decoder: %s\n", opus_strerror(err));
     return NULL;
   }
   if (!st)
   {
      fprintf (stderr, "Decoder initialization failed: %s\n", opus_strerror(err));
      return NULL;
   }

   *streams=header.nb_streams;

   if(header.gain!=0 || manual_gain!=0)
   {
      /*Gain API added in a newer libopus version, if we don't have it
        we apply the gain ourselves. We also add in a user provided
        manual gain at the same time.*/
      int gainadj = (int)(manual_gain*256.)+header.gain;
#ifdef OPUS_SET_GAIN
      err=opus_multistream_decoder_ctl(st,OPUS_SET_GAIN(gainadj));
      if(err==OPUS_UNIMPLEMENTED)
      {
#endif
         *gain = pow(10., gainadj/5120.);
#ifdef OPUS_SET_GAIN
      } else if (err!=OPUS_OK)
      {
         fprintf (stderr, "Error setting gain: %s\n", opus_strerror(err));
         return NULL;
      }
#endif
   }

   if (!quiet)
   {
      fprintf(stderr, "Decoding to %d Hz (%d channel%s)", *rate,
        *channels, *channels>1?"s":"");
      if(header.version!=1)fprintf(stderr, ", Header v%d",header.version);
      fprintf(stderr, "\n");
      if (header.gain!=0)fprintf(stderr,"Playback gain: %f dB\n", header.gain/256.);
      if (manual_gain!=0)fprintf(stderr,"Manual gain: %f dB\n", manual_gain);
   }

   return st;
}

opus_int64 audio_write(float *pcm, int channels, int frame_size, FILE *fout, SpeexResamplerState *resampler,
                       int *skip, shapestate *shapemem, int file, opus_int64 maxout, int fp)
{
   opus_int64 sampout=0;
   int i,ret,tmp_skip;
   unsigned out_len;
   short *out;
   float *buf;
   float *output;
   out=alloca(sizeof(short)*MAX_FRAME_SIZE*channels);
   buf=alloca(sizeof(float)*MAX_FRAME_SIZE*channels);
   maxout=maxout<0?0:maxout;
   do {
     if (skip){
       tmp_skip = (*skip>frame_size) ? (int)frame_size : *skip;
       *skip -= tmp_skip;
     } else {
       tmp_skip = 0;
     }
     if (resampler){
       unsigned in_len;
       output=buf;
       in_len = frame_size-tmp_skip;
       out_len = 1024<maxout?1024:maxout;
       speex_resampler_process_interleaved_float(resampler, pcm+channels*tmp_skip, &in_len, buf, &out_len);
       pcm += channels*(in_len+tmp_skip);
       frame_size -= in_len+tmp_skip;
     } else {
       output=pcm+channels*tmp_skip;
       out_len=frame_size-tmp_skip;
       frame_size=0;
     }

     if(!file||!fp)
     {
        /*Convert to short and save to output file*/
        if (shapemem){
          shape_dither_toshort(shapemem,out,output,out_len,channels);
        }else{
          for (i=0;i<(int)out_len*channels;i++)
            out[i]=(short)float2int(fmaxf(-32768,fminf(output[i]*32768.f,32767)));
        }
        if ((le_short(1)!=1)&&file){
          for (i=0;i<(int)out_len*channels;i++)
            out[i]=le_short(out[i]);
        }
     }

     if(maxout>0)
     {
#if defined WIN32 || defined _WIN32
       if(!file){
         ret=WIN_Play_Samples (out, sizeof(short) * channels * (out_len<maxout?out_len:maxout));
         if(ret>0)ret/=sizeof(short)*channels;
         else fprintf(stderr, "Error playing audio.\n");
       }else
#elif defined HAVE_LIBSNDIO
       if(!file){
         ret=sio_write (hdl, out, sizeof(short) * channels * (out_len<maxout?out_len:maxout));
         if(ret>0)ret/=sizeof(short)*channels;
         else fprintf(stderr, "Error playing audio.\n");
       }else
#endif
         ret=fwrite(fp?(char *)output:(char *)out, (fp?4:2)*channels, out_len<maxout?out_len:maxout, fout);
       sampout+=ret;
       maxout-=ret;
     }
   } while (frame_size>0 && maxout>0);
   return sampout;
}

int main(int argc, char **argv)
{
   int c;
   int option_index = 0;
   char *inFile, *outFile;
   FILE *fin, *fout=NULL, *frange=NULL;
   float *output;
   int frame_size=0;
   OpusMSDecoder *st=NULL;
   opus_int64 packet_count=0;
   int total_links=0;
   int stream_init = 0;
   int quiet = 0;
   int forcewav = 0;
   ogg_int64_t page_granule=0;
   ogg_int64_t link_out=0;
   struct option long_options[] =
   {
      {"help", no_argument, NULL, 0},
      {"quiet", no_argument, NULL, 0},
      {"version", no_argument, NULL, 0},
      {"version-short", no_argument, NULL, 0},
      {"rate", required_argument, NULL, 0},
      {"gain", required_argument, NULL, 0},
      {"no-dither", no_argument, NULL, 0},
      {"float", no_argument, NULL, 0},
      {"force-wav", no_argument, NULL, 0},
      {"packet-loss", required_argument, NULL, 0},
      {"save-range", required_argument, NULL, 0},
      {0, 0, 0, 0}
   };
   ogg_sync_state oy;
   ogg_page       og;
   ogg_packet     op;
   ogg_stream_state os;
   int close_in=0;
   int eos=0;
   ogg_int64_t audio_size=0;
   double last_coded_seconds=0;
   float loss_percent=-1;
   float manual_gain=0;
   int channels=-1;
   int mapping_family;
   int rate=0;
   int wav_format=0;
   int preskip=0;
   int gran_offset=0;
   int has_opus_stream=0;
   int has_tags_packet=0;
   ogg_int32_t opus_serialno;
   int dither=1;
   int fp=0;
   shapestate shapemem;
   SpeexResamplerState *resampler=NULL;
   float gain=1;
   int streams=0;
   size_t last_spin=0;
#ifdef WIN_UNICODE
   int argc_utf8;
   char **argv_utf8;
#endif

   if(query_cpu_support()){
     fprintf(stderr,"\n\n** WARNING: This program with compiled with SSE%s\n",query_cpu_support()>1?"2":"");
     fprintf(stderr,"            but this CPU claims to lack these instructions. **\n\n");
   }

#ifdef WIN_UNICODE
   (void)argc;
   (void)argv;

   init_console_utf8();
   init_commandline_arguments_utf8(&argc_utf8, &argv_utf8);
#endif

   output=0;
   shapemem.a_buf=0;
   shapemem.b_buf=0;
   shapemem.mute=960;
   shapemem.fs=0;

   /*Process options*/
   while(1)
   {
      c = getopt_long (argc_utf8, argv_utf8, "hV",
                       long_options, &option_index);
      if (c==-1)
         break;

      switch(c)
      {
      case 0:
         if (strcmp(long_options[option_index].name,"help")==0)
         {
            usage();
            quit(0);
         } else if (strcmp(long_options[option_index].name,"quiet")==0)
         {
            quiet = 1;
         } else if (strcmp(long_options[option_index].name,"version")==0)
         {
            version();
            quit(0);
         } else if (strcmp(long_options[option_index].name,"version-short")==0)
         {
            version_short();
            quit(0);
         } else if (strcmp(long_options[option_index].name,"no-dither")==0)
         {
            dither=0;
         } else if (strcmp(long_options[option_index].name,"float")==0)
         {
            fp=1;
         } else if (strcmp(long_options[option_index].name,"force-wav")==0)
         {
            forcewav=1;
         } else if (strcmp(long_options[option_index].name,"rate")==0)
         {
            rate=atoi (optarg);
         } else if (strcmp(long_options[option_index].name,"gain")==0)
         {
            manual_gain=atof (optarg);
         }else if(strcmp(long_options[option_index].name,"save-range")==0){
          frange=fopen_utf8(optarg,"w");
          if(frange==NULL){
            perror(optarg);
            fprintf(stderr,"Could not open save-range file: %s\n",optarg);
            fprintf(stderr,"Must provide a writable file name.\n");
            quit(1);
          }
         } else if (strcmp(long_options[option_index].name,"packet-loss")==0)
         {
            loss_percent = atof(optarg);
         }
         break;
      case 'h':
         usage();
         quit(0);
         break;
      case 'V':
         version();
         quit(0);
         break;
      case '?':
         usage();
         quit(1);
         break;
      }
   }
   if (argc_utf8-optind!=2 && argc_utf8-optind!=1)
   {
      usage();
      quit(1);
   }
   inFile=argv_utf8[optind];

   /*Output to a file or playback?*/
   if (argc_utf8-optind==2){
     /*If we're outputting to a file, should we apply a wav header?*/
     int i;
     char *ext;
     outFile=argv_utf8[optind+1];
     ext=".wav";
     i=strlen(outFile)-4;
     wav_format=i>=0;
     while(wav_format&&ext&&outFile[i]) {
       wav_format&=tolower(outFile[i++])==*ext++;
     }
     wav_format|=forcewav;
   }else {
     outFile="";
     wav_format=0;
     /*If playing to audio out, default the rate to 48000
       instead of the original rate. The original rate is
       only important for minimizing surprise about the rate
       of output files and preserving length, which aren't
       relevant for playback. Many audio devices sound
       better at 48kHz and not resampling also saves CPU.*/
     if(rate==0)rate=48000;
     /*Playback is 16-bit only.*/
     fp=0;
   }
   /*If the output is floating point, don't dither.*/
   if(fp)dither=0;

   /*Open input file*/
   if (strcmp(inFile, "-")==0)
   {
#if defined WIN32 || defined _WIN32
      _setmode(_fileno(stdin), _O_BINARY);
#endif
      fin=stdin;
   }
   else
   {
      fin = fopen_utf8(inFile, "rb");
      if (!fin)
      {
         perror(inFile);
         quit(1);
      }
      close_in=1;
   }

   /* .opus files use the Ogg container to provide framing and timekeeping.
    * http://tools.ietf.org/html/draft-terriberry-oggopus
    * The easiest way to decode the Ogg container is to use libogg, so
    *  thats what we do here.
    * Using libogg is fairly straight forward-- you take your stream of bytes
    *  and feed them to ogg_sync_ and it periodically returns Ogg pages, you
    *  check if the pages belong to the stream you're decoding then you give
    *  them to libogg and it gives you packets. You decode the packets. The
    *  pages also provide timing information.*/
   ogg_sync_init(&oy);

   /*Main decoding loop*/
   while (1)
   {
      char *data;
      int i, nb_read;
      /*Get the ogg buffer for writing*/
      data = ogg_sync_buffer(&oy, 200);
      /*Read bitstream from input file*/
      nb_read = fread(data, sizeof(char), 200, fin);
      ogg_sync_wrote(&oy, nb_read);

      /*Loop for all complete pages we got (most likely only one)*/
      while (ogg_sync_pageout(&oy, &og)==1)
      {
         if (stream_init == 0) {
            ogg_stream_init(&os, ogg_page_serialno(&og));
            stream_init = 1;
         }
         if (ogg_page_serialno(&og) != os.serialno) {
            /* so all streams are read. */
            ogg_stream_reset_serialno(&os, ogg_page_serialno(&og));
         }
         /*Add page to the bitstream*/
         ogg_stream_pagein(&os, &og);
         page_granule = ogg_page_granulepos(&og);
         /*Extract all available packets*/
         while (ogg_stream_packetout(&os, &op) == 1)
         {
            /*OggOpus streams are identified by a magic string in the initial
              stream header.*/
            if (op.b_o_s && op.bytes>=8 && !memcmp(op.packet, "OpusHead", 8)) {
               if(has_opus_stream && has_tags_packet)
               {
                 /*If we're seeing another BOS OpusHead now it means
                   the stream is chained without an EOS.*/
                 has_opus_stream=0;
                 if(st)opus_multistream_decoder_destroy(st);
                 st=NULL;
                 fprintf(stderr,"\nWarning: stream %" I64FORMAT " ended without EOS and a new stream began.\n",(long long)os.serialno);
               }
               if(!has_opus_stream)
               {
                 if(packet_count>0 && opus_serialno==os.serialno)
                 {
                   fprintf(stderr,"\nError: Apparent chaining without changing serial number (%" I64FORMAT "==%" I64FORMAT ").\n",
                     (long long)opus_serialno,(long long)os.serialno);
                   quit(1);
                 }
                 opus_serialno = os.serialno;
                 has_opus_stream = 1;
                 has_tags_packet = 0;
                 link_out = 0;
                 packet_count = 0;
                 eos = 0;
                 total_links++;
               } else {
                 fprintf(stderr,"\nWarning: ignoring opus stream %" I64FORMAT "\n",(long long)os.serialno);
               }
            }
            if (!has_opus_stream || os.serialno != opus_serialno)
               break;
            /*If first packet in a logical stream, process the Opus header*/
            if (packet_count==0)
            {
               st = process_header(&op, &rate, &mapping_family, &channels, &preskip, &gain, manual_gain, &streams, wav_format, quiet);
               if (!st)
                  quit(1);

               if(ogg_stream_packetout(&os, &op)!=0 || og.header[og.header_len-1]==255)
               {
                  /*The format specifies that the initial header and tags packets are on their
                    own pages. To aid implementors in discovering that their files are wrong
                    we reject them explicitly here. In some player designs files like this would
                    fail even without an explicit test.*/
                  fprintf(stderr, "Extra packets on initial header page. Invalid stream.\n");
                  quit(1);
               }

               /*Remember how many samples at the front we were told to skip
                 so that we can adjust the timestamp counting.*/
               gran_offset=preskip;

               /*Setup the memory for the dithered output*/
               if(!shapemem.a_buf)
               {
                  shapemem.a_buf=calloc(channels,sizeof(float)*4);
                  shapemem.b_buf=calloc(channels,sizeof(float)*4);
                  shapemem.fs=rate;
               }
               if(!output)output=malloc(sizeof(float)*MAX_FRAME_SIZE*channels);

               /*Normal players should just play at 48000 or their maximum rate,
                 as described in the OggOpus spec.  But for commandline tools
                 like opusdec it can be desirable to exactly preserve the original
                 sampling rate and duration, so we have a resampler here.*/
               if (rate != 48000 && resampler==NULL)
               {
                  int err;
                  resampler = speex_resampler_init(channels, 48000, rate, 5, &err);
                  if (err!=0)
                     fprintf(stderr, "resampler error: %s\n", speex_resampler_strerror(err));
                  speex_resampler_skip_zeros(resampler);
               }
               if(!fout)fout=out_file_open(outFile, &wav_format, rate, mapping_family, &channels, fp);
            } else if (packet_count==1)
            {
               if (!quiet)
                  print_comments((char*)op.packet, op.bytes);
               has_tags_packet=1;
               if(ogg_stream_packetout(&os, &op)!=0 || og.header[og.header_len-1]==255)
               {
                  fprintf(stderr, "Extra packets on initial tags page. Invalid stream.\n");
                  quit(1);
               }
            } else {
               int ret;
               opus_int64 maxout;
               opus_int64 outsamp;
               int lost=0;
               if (loss_percent>0 && 100*((float)rand())/RAND_MAX<loss_percent)
                  lost=1;

               /*End of stream condition*/
               if (op.e_o_s && os.serialno == opus_serialno)eos=1; /* don't care for anything except opus eos */

               /*Are we simulating loss for this packet?*/
               if (!lost){
                  /*Decode Opus packet*/
                  ret = opus_multistream_decode_float(st, (unsigned char*)op.packet, op.bytes, output, MAX_FRAME_SIZE, 0);
               } else {
                  /*Extract the original duration.
                    Normally you wouldn't have it for a lost packet, but normally the
                    transports used on lossy channels will effectively tell you.
                    This avoids opusdec squaking when the decoded samples and
                    granpos mismatches.*/
                  opus_int32 lost_size;
                  lost_size = MAX_FRAME_SIZE;
                  if(op.bytes>0){
                    opus_int32 spp;
                    spp=opus_packet_get_nb_frames(op.packet,op.bytes);
                    if(spp>0){
                      spp*=opus_packet_get_samples_per_frame(op.packet,48000/*decoding_rate*/);
                      if(spp>0)lost_size=spp;
                    }
                  }
                  /*Invoke packet loss concealment.*/
                  ret = opus_multistream_decode_float(st, NULL, 0, output, lost_size, 0);
               }

               if(!quiet){
                  /*Display a progress spinner while decoding.*/
                  static const char spinner[]="|/-\\";
                  double coded_seconds = (double)audio_size/(channels*rate*(fp?4:2));
                  if(coded_seconds>=last_coded_seconds+1){
                     fprintf(stderr,"\r[%c] %02d:%02d:%02d", spinner[last_spin&3],
                             (int)(coded_seconds/3600),(int)(coded_seconds/60)%60,
                             (int)(coded_seconds)%60);
                     fflush(stderr);
                     last_spin++;
                     last_coded_seconds=coded_seconds;
                  }
               }

               /*If the decoder returned less than zero, we have an error.*/
               if (ret<0)
               {
                  fprintf (stderr, "Decoding error: %s\n", opus_strerror(ret));
                  break;
               }
               frame_size = ret;

               /*If we're collecting --save-range debugging data, collect it now.*/
               if(frange!=NULL){
                 OpusDecoder *od;
                 opus_uint32 rngs[256];
                 for(i=0;i<streams;i++){
                   ret=opus_multistream_decoder_ctl(st,OPUS_MULTISTREAM_GET_DECODER_STATE(i,&od));
                   ret=opus_decoder_ctl(od,OPUS_GET_FINAL_RANGE(&rngs[i]));
                 }
                 save_range(frange,frame_size*(48000/48000/*decoding_rate*/),op.packet,op.bytes,
                            rngs,streams);
               }

               /*Apply header gain, if we're not using an opus library new
                 enough to do this internally.*/
               if (gain!=0){
                 for (i=0;i<frame_size*channels;i++)
                    output[i] *= gain;
               }

               /*This handles making sure that our output duration respects
                 the final end-trim by not letting the output sample count
                 get ahead of the granpos indicated value.*/
               maxout=((page_granule-gran_offset)*rate/48000)-link_out;
               outsamp=audio_write(output, channels, frame_size, fout, resampler, &preskip, dither?&shapemem:0, strlen(outFile)!=0,0>maxout?0:maxout,fp);
               link_out+=outsamp;
               audio_size+=(fp?4:2)*outsamp*channels;
            }
            packet_count++;
         }
         /*We're done, drain the resampler if we were using it.*/
         if(eos && resampler)
         {
            float *zeros;
            int drain;

            zeros=(float *)calloc(100*channels,sizeof(float));
            drain = speex_resampler_get_input_latency(resampler);
            do {
               opus_int64 outsamp;
               int tmp = drain;
               if (tmp > 100)
                  tmp = 100;
               outsamp=audio_write(zeros, channels, tmp, fout, resampler, NULL, &shapemem, strlen(outFile)!=0, ((page_granule-gran_offset)*rate/48000)-link_out,fp);
               link_out+=outsamp;
               audio_size+=(fp?4:2)*outsamp*channels;
               drain -= tmp;
            } while (drain>0);
            free(zeros);
            speex_resampler_destroy(resampler);
            resampler=NULL;
         }
         if(eos)
         {
            has_opus_stream=0;
            if(st)opus_multistream_decoder_destroy(st);
            st=NULL;
         }
      }
      if (feof(fin)) {
         if(!quiet) {
           fprintf(stderr, "\rDecoding complete.        \n");
           fflush(stderr);
         }
         break;
      }
   }

   /*If we were writing wav, go set the duration.*/
   if (strlen(outFile)!=0 && fout && wav_format>0 && audio_size<0x7FFFFFFF)
   {
      if (fseek(fout,4,SEEK_SET)==0)
      {
         int tmp;
         tmp = le_int(audio_size+20+wav_format);
         if(fwrite(&tmp,4,1,fout)!=1)fprintf(stderr,"Error writing end length.\n");
         if (fseek(fout,16+wav_format,SEEK_CUR)==0)
         {
            tmp = le_int(audio_size);
            if(fwrite(&tmp,4,1,fout)!=1)fprintf(stderr,"Error writing header length.\n");
         } else
         {
            fprintf (stderr, "First seek worked, second didn't\n");
         }
      } else {
         fprintf (stderr, "Cannot seek on wav file output, wav size chunk will be incorrect\n");
      }
   }

   /*Did we make it to the end without recovering ANY opus logical streams?*/
   if(!total_links)fprintf (stderr, "This doesn't look like a Opus file\n");

   if (stream_init)
      ogg_stream_clear(&os);
   ogg_sync_clear(&oy);

#if defined WIN32 || defined _WIN32
   if (strlen(outFile)==0)
      WIN_Audio_close ();
#endif

   if(shapemem.a_buf)free(shapemem.a_buf);
   if(shapemem.b_buf)free(shapemem.b_buf);

   if(output)free(output);

   if(frange)fclose(frange);

   if (close_in)
      fclose(fin);
   if (fout != NULL)
      fclose(fout);

#ifdef WIN_UNICODE
   free_commandline_arguments_utf8(&argc_utf8, &argv_utf8);
   uninit_console_utf8();
#endif

   return 0;
}
