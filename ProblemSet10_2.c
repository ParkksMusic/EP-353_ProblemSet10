#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include <math.h>
#include <portaudio.h>
#include <sndfile.h>

//Compile with:
//gcc AudioProcessor.c -o AudioProcessor -lportaudio
//Run with:
//./AudioProcessor
//------------------------------------------------------------------------------------
//Constants
#define kInputDeviceIndex 0 //Built-in input
#define kOutputDeviceIndex 1 //Built-in output
#define kNumFramesPerBuffer 512
#define kSamplingRate 44100.0
#define kNumChannels 2
#define kDefaultFrequency 440.0
#define kTableSize (1<<7) //128
#define kInputFileName "Sine.aif"
//------------------------------------------------------------------------------------
//Struct for wavetable
typedef struct Wavetable {
  float *table;
  unsigned long size;
  float curIndex;
  float delta;
  bool isPlaying;
  float amplitude;
} Wavetable;
//------------------------------------------------------------------------------------
//Function prototypes
int openInputSndFile();
int initPortAudio();
int closePortAudio();
void printPaDevices();
void process(float *buffer, unsigned long numFrames, void *userData);
void createWavetable(Wavetable *wavetable);
float next(Wavetable *wavetable);
//------------------------------------------------------------------------------------
//Audio render callback function
int renderCallback(
  const void *input,
  void *output,
  unsigned long frameCount,
  const PaStreamCallbackTimeInfo* timeInfo,
  PaStreamCallbackFlags statusFlags,
  void *userData 
);
//------------------------------------------------------------------------------------
//Declare user data that holds SNDFILE and SF_INFO
//so that we can use them inside audio render callback
typedef struct SoundFile {
    SNDFILE *file;
    SF_INFO *info;
}   SoundFile;
//------------------------------------------------------------------------------------
//Global variables
SNDFILE *gInFile = NULL;
SF_INFO gSfInfo;
//------------------------------------------------------------------------------------

int main(int argc, char *argv[]){
  PaStream *pStream = NULL; //For port audio streaming
	PaStreamParameters inputParameters; //Parameters for input of a stream
  PaStreamParameters outputParameters; //Parameters for output of a stream

  //------------------------------------------------------------------------------------
  openInputSndFile();
  //------------------------------------------------------------------------------------

  //Set up synthesizer
  Wavetable *wavetable;
  wavetable = (Wavetable *) malloc(sizeof(Wavetable));
  wavetable->table = (float *) calloc(kTableSize,sizeof(float));
  wavetable->size = gSfInfo.frames;
  wavetable->delta = kInputFile * (gSfInfo.frames / gSfInfo.samplerate);//set frequecy
  wavetable->curIndex = 0.0f;
  wavetable->isPlaying = true;
  wavetable->amplitude = 1.0f;
  createWavetable(wavetable);

  //Initialize port audio
  if(initPortAudio()) return 1;

  //Print available audio devices
  printPaDevices();

  //Configure port audio input streaming setup
  memset(&inputParameters, 0, sizeof(PaStreamParameters));
  inputParameters.channelCount = kNumChannels;
	inputParameters.device = kInputDeviceIndex;
	inputParameters.sampleFormat = paFloat32;
	inputParameters.suggestedLatency = 0.0;
  
  //Configure port audio output streaming setup
  memset(&outputParameters, 0, sizeof(PaStreamParameters));
  outputParameters.channelCount = kNumChannels;
	outputParameters.device = kOutputDeviceIndex;
	outputParameters.sampleFormat = paFloat32;
	outputParameters.suggestedLatency = 0.0;

  //Open port audio streaming
  PaError error = Pa_OpenStream(
    &pStream,
    &inputParameters, //input
    &outputParameters, //output
    kSamplingRate,
    kNumFramesPerBuffer, //frames per buffer
    paNoFlag,
    renderCallback,
    wavetable
  );

  //Check for error in opening port audio streaming
  if(error != paNoError){
    printf("Error: Failed to open port audio stream. %s\n",Pa_GetErrorText(error));
    closePortAudio();
    return 1;
  }

  //Start port audio streaming
  error = Pa_StartStream(pStream);
  if(error != paNoError){
    printf("Error: Failed to start port audio stream. %s\n",Pa_GetErrorText(error));
  }
  else {
    printf("Rendering audio... Press enter to stop streaming audio\n");
    getchar();
    error = Pa_StopStream(pStream);
    if(error != paNoError){
      printf("Error: Failed to stop port audio stream. %s", Pa_GetErrorText(error));
    }
  }

  //Close port audio streaming
  if(closePortAudio()) return 1;

  //Free memory
  free(wavetable->table);
  free(wavetable);

  return 0;
}
//------------------------------------------------------------------------------------
// void createWavetable(Wavetable *wavetable){
//   //Square wave approximation
//   for (int i = 1; i <= 15; i+=2){ //Up to 15th harmonics, but only odd harmonics
//     double delta = (2.0 * M_PI) / (wavetable->size - 1) * i;
//     double angle = 0.0;
//     for (int n = 0; n < wavetable->size; n++){
//       wavetable->table[n] += (1.0f / i) * sin(angle);
//       angle += delta;
//     }
//   }
// }
//------------------------------------------------------------------------------------
float next(Wavetable *wavetable){
  unsigned long index0 = (unsigned long) wavetable->curIndex;
  unsigned long index1 = index0 + 1;
  
  //Calculate the interpolation value
  float fraction = wavetable->curIndex - (float)index0;
  
  //get values from the wavetable
  float value0 = wavetable->table[index0];
  float value1 = wavetable->table[index1];
  
  //Calculate the interpolated value
  float sample = value0 + fraction * (value1 - value0);
  
  //increment the angle delta of the table and wrap
  wavetable->curIndex += wavetable->delta;
  if(wavetable->curIndex >= (float) wavetable->size){
      wavetable->curIndex -= (float) wavetable->size;
  }
  
  return sample;
}
//------------------------------------------------------------------------------------
void process(float *buffer, unsigned long numFrames, void *userData){
  Wavetable * wavetable = (Wavetable *) userData;
  if(wavetable->isPlaying){
    float sample = 0;
    for(unsigned long n=0; n < numFrames; n+=kNumChannels){
      sample = wavetable->amplitude * next(wavetable);

      for(int c=0;c<kNumChannels;c++){
        buffer[n+c] = sample;
      }
    }
  }
}
//------------------------------------------------------------------------------------
int renderCallback(
  const void *input,
  void *output,
  unsigned long frameCount,
  const PaStreamCallbackTimeInfo* timeInfo,
  PaStreamCallbackFlags statusFlags,
  void *userData 
){
  float *inBuffer = (float *) input;
  float *outBuffer = (float *) output;
  unsigned long numFrames = frameCount * kNumChannels;

  //Samples are interleaved so increment by two with kNumChannels
  for(unsigned long n=0; n < numFrames; n+=kNumChannels){//iterate over samples in channels
    for(int c=0;c<kNumChannels;c++){//Iterate over number of channels
      outBuffer[n+c] = inBuffer[n+c]; //Copy input to output
    }
  }

  process(outBuffer, numFrames, userData);

  return 0;
}
//------------------------------------------------------------------------------------
int initPortAudio(){ //Initialize Port Audio
  PaError error = Pa_Initialize();
  if(error != paNoError){
    printf("Error: Pa_Initialize() failed with %s\n", Pa_GetErrorText(error));
    return 1;
  }
  return 0;
}
//------------------------------------------------------------------------------------
int closePortAudio(){ //Terminate Port Audio
  PaError error = Pa_Terminate();
  if(error != paNoError){
    printf("Error: Pa_Terminate() failed with %s\n",Pa_GetErrorText(error));
    return 1;
  }
  return 0;
}
//------------------------------------------------------------------------------------
void printPaDevices(){
  //Get number of port audio devices available
  PaDeviceIndex numDevices = Pa_GetDeviceCount(); 
  PaDeviceIndex curDeviceID;

  const PaDeviceInfo *pDeviceInfo; 
  const PaHostApiInfo *pHostApiInfo;

  //Iterate over each device and print out information about them
  for(curDeviceID = 0; curDeviceID < numDevices; curDeviceID++){
    pDeviceInfo = Pa_GetDeviceInfo(curDeviceID);
    pHostApiInfo = Pa_GetHostApiInfo(pDeviceInfo->hostApi);
    printf("--------------------------------------------\n");
    printf("ID: %d, Name: %s, ", curDeviceID, pDeviceInfo->name);
    printf("API name: %s\n", pHostApiInfo->name);
    printf("Max output channels: %d\t", pDeviceInfo->maxOutputChannels);
    printf("Max input channels: %d\n\n", pDeviceInfo->maxInputChannels);
  }
}