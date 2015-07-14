#ifndef DEPENDENCY_AUDIO_DECODE
//Stubs:
//(none required)
#else

#define DEPENDENCY_AUDIO_DECODE_OGG
#define DEPENDENCY_AUDIO_DECODE_MP3
#define DEPENDENCY_AUDIO_DECODE_WAV

#ifdef QB64_BACKSLASH_FILESYSTEM
 #ifdef DEPENDENCY_AUDIO_DECODE_MP3  
  #include "mp3_mini\\src.c"
 #endif
 #ifdef DEPENDENCY_AUDIO_DECODE_WAV
  #include "wav\\src.c"
 #endif
 #ifdef DEPENDENCY_AUDIO_DECODE_OGG
  #include "ogg\\src.c"
 #endif
#else
 #ifdef DEPENDENCY_AUDIO_DECODE_MP3
  #include "mp3_mini/src.c"
 #endif
 #ifdef DEPENDENCY_AUDIO_DECODE_WAV
  #include "wav/src.c"
 #endif
 #ifdef DEPENDENCY_AUDIO_DECODE_OGG
  #include "ogg/src.c"
 #endif
#endif

//forward refs:
void sub__sndvol(int32 handle,float volume);
void sub__sndclose(int32 handle);

int32 func__sndopen(qbs* filename,qbs* requirements,int32 passed){
sndsetup();
if (new_error) return 0;

static qbs *s1=NULL;
if (!s1) s1=qbs_new(0,0);
static qbs *req=NULL;
if (!req) req=qbs_new(0,0);
static qbs *s3=NULL;
if (!s3) s3=qbs_new(0,0);

static uint8 r[32];
static int32 i,i2,i3;
//check requirements
memset(r,0,32);
if (passed){
if (requirements->len){
i=1;
qbs_set(req,qbs_ucase(requirements));//convert tmp str to perm str
nextrequirement:
i2=func_instr(i,req,qbs_new_txt(","),1);
if (i2){
qbs_set(s1,func_mid(req,i,i2-i,1));
}else{
qbs_set(s1,func_mid(req,i,req->len-i+1,1));
}
qbs_set(s1,qbs_rtrim(qbs_ltrim(s1)));
if (qbs_equal(s1,qbs_new_txt("SYNC"))){r[0]++; goto valid;}
if (qbs_equal(s1,qbs_new_txt("VOL"))){r[1]++; goto valid;}
if (qbs_equal(s1,qbs_new_txt("PAUSE"))){r[2]++; goto valid;}
if (qbs_equal(s1,qbs_new_txt("LEN"))){r[3]++; goto valid;}
if (qbs_equal(s1,qbs_new_txt("SETPOS"))){r[4]++; goto valid;}
error(5); return 0;//invalid requirements
valid:
if (i2){i=i2+1; goto nextrequirement;}
for (i=0;i<32;i++) if (r[i]>1){error(5); return 0;}//cannot define requirements twice
}//->len
}//passed
qbs_set(s1,qbs_add(filename,qbs_new_txt_len("\0",1)));//s1=filename+CHR$(0)

if (!r[0]){//NOT SYNC
 if (snd_stream_handle){error(5); return 0;}//stream in use
}

//load file
if (s1->len==1) return 0;//return invalid handle if null length string
static int32 fh,result;
static int64 lof;
fh=gfs_open(s1,1,0,0);
if (fh<0) return 0;
lof=gfs_lof(fh);
static uint8* content;
content=(uint8*)malloc(lof); if (!content){gfs_close(fh); return 0;}
result=gfs_read(fh,-1,content,lof);
gfs_close(fh);
if (result<0){free(content); return 0;}

//identify file format
static snd_sequence_struct *seq;

//OGG?
#ifdef DEPENDENCY_AUDIO_DECODE_OGG
if (lof>=3){
if (content[0]==79){ if (content[1]==103){ if (content[2]==103){//"Ogg"
   seq=snd_decode_ogg(content,lof);
   goto got_seq;
}}}
}//3
#endif

//WAV?
#ifdef DEPENDENCY_AUDIO_DECODE_WAV
if (lof>=12){
  if ((*(uint32*)&content[8])==0x45564157){//WAVE
   seq=snd_decode_wav(content,lof);
   goto got_seq;
  }//WAVE
}
#endif

//assume mp3!
//MP3?
#ifdef DEPENDENCY_AUDIO_DECODE_MP3
seq=snd_decode_mp3(content,lof);
#endif

got_seq:
free(content);
if (seq==NULL) return 0;

//convert sequence (includes sample rate conversion etc etc)

//just perform sample_rate fix for now...

//1. 8->16bit conversion and/or edian conversion
static int32 incorrect_format;
incorrect_format=0;
if (seq->bits_per_sample!=16) incorrect_format=1;
if (seq->is_unsigned) incorrect_format=1;
//todo... if (seq->endian==???)

//this section does not fix the frequency, only the bits per sample
//and signed-ness of the data
if (incorrect_format){
 static int32 bps; bps=seq->bits_per_sample/8;
 static int32 samples; samples=seq->data_size/bps;
 static uint8 *new_data;
 if (bps!=2){
  new_data=(uint8*)malloc(samples*2);
 }else{
  new_data=seq->data;
 }
 static int32 i,v;
 for (i=0;i<samples;i++){
  //read original value
  v=0;
  if (bps==1){
   if (seq->is_unsigned){
    v=*(uint8*)(seq->data+i*1);
    v=(v-128)*256;
   }else{
    v=*(int8*)(seq->data+i*1);
    v=v*128;
   }
  }
  if (bps==2){
   if (seq->is_unsigned){
    v=*(uint16*)(seq->data+i*2);
    v=v-32768;
   }else{
    v=*(int16*)(seq->data+i*2);
   }
  }
  //place new value into array
  ((int16*)new_data)[i]=v;
 }//i
 if (bps!=2){free(seq->data); seq->data=new_data; seq->data_size=samples*2;}
 //update seq info
 seq->bits_per_sample=16;
 seq->is_unsigned=0;
}//incorrect format


//2. samplerate conversion
if (seq->sample_rate != snd_frequency) { //need to resample seq->data
  //create new resampler
  SpeexResamplerState *state;
  state = speex_resampler_init(seq->channels, seq->sample_rate, snd_frequency, SPEEX_RESAMPLER_QUALITY_MIN, NULL);
  if (!state) { //NULL means failure
    free(seq->data);
    return 0;
  }
  
  //allocate new memory for output
  int32 out_samples_max = ((double)seq->data_size / seq->channels / 2) * ((((double)snd_frequency) / ((double)seq->sample_rate)) + 0.1) + 100;//10%+100 extra samples as a buffer-zone
  int16 *resampled = (int16 *)malloc(out_samples_max * seq->channels * sizeof(int16));
  if (!resampled) {
    free(seq->data);
    return 0;
  }
  
  //establish data sizes
  //in_len will be set by the resampler to number of samples processed
  spx_uint32_t in_len = seq->data_size / seq->channels / 2; // divide by 2 because 2byte samples, divive by #channels because function wants it per-channel
  //out_len will be set to the number of samples written
  spx_uint32_t out_len;
  
  //resample!
  if (speex_resampler_process_interleaved_int(state, (spx_int16_t *)seq->data, &in_len, (spx_int16_t *)resampled, &out_len) != RESAMPLER_ERR_SUCCESS) {
    //Error
    free(resampled);
    free(seq->data);
    speex_resampler_destroy(state);
    return 0;
  }
  
  //destroy the resampler anyway
  speex_resampler_destroy(state);
  
  //establish real size of new data and update seq
  free(seq->data); //That was the old data
  seq->data_size = out_len * seq->channels * 2; //remember out_len is perchannel, and each sample is 2 bytes
  seq->data = (uint8_t *)realloc(resampled, seq->data_size); //we overestimated the array size before, so make it the correct size now
  if (!seq->data) { //realloc could fail
    free(resampled);
    return 0;
  }
  seq->sample_rate = snd_frequency;
}

if (seq->channels==1){
seq->data_mono=seq->data;
seq->data_mono_size=seq->data_size;
}
if (seq->channels==2){
seq->data_stereo=seq->data;
seq->data_stereo_size=seq->data_size;
}
if (seq->channels>2) return 0;

//attach sequence to handle (& inc. refs)
//create snd handle
static int32 handle; handle=list_add(snd_handles);
static snd_struct *snd; snd=(snd_struct*)list_get(snd_handles,handle);

snd->internal=0;
snd->type=2;
snd->seq=seq;
snd->volume=1.0;
snd->capability=r[0]*SND_CAPABILITY_SYNC+r[1]*SND_CAPABILITY_VOL+r[2]*SND_CAPABILITY_PAUSE+r[3]*SND_CAPABILITY_LEN+r[4]*SND_CAPABILITY_SETPOS;
if (!r[0]){
 snd->streamed=1;//NOT SYNC
 snd_stream_handle=handle;
}

return handle;
}


void sub__sndplayfile(qbs *filename,int32 sync,double volume,int32 passed){
if (new_error) return;
sndsetup();
static int32 handle;
static int32 setvolume;
static qbs *syncstr=NULL; if (!syncstr) syncstr=qbs_new(0,0);
setvolume=0;
if (passed&2){
if ((volume<0)||(volume>1)){error(5); return;}
if (volume!=1) setvolume=1;
}
if ((!setvolume)&&(!sync)) syncstr->len=0;
if ((setvolume)&&(!sync)) qbs_set(syncstr,qbs_new_txt("VOL"));
if ((!setvolume)&&(sync)) qbs_set(syncstr,qbs_new_txt("SYNC"));
if ((setvolume)&&(sync)) qbs_set(syncstr,qbs_new_txt("SYNC,VOL"));
if (syncstr->len){
handle=func__sndopen(filename,syncstr,1);
}else{
handle=func__sndopen(filename,NULL,0);
}
if (handle==0) return;
if (setvolume) sub__sndvol(handle,volume);
sub__sndplay(handle);
sub__sndclose(handle);
}




#endif

