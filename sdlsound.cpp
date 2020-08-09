/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdlsound.cpp,v 1.34 2015/11/23 20:24:01 thor Exp $
 **
 ** In this mdoule: This class implements audio output thru the SDL library
 ** as available.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "exceptions.hpp"
#include "sdlclient.hpp"
#include "sound.hpp"
#include "monitor.hpp"
#include "sdlclient.hpp"
#include "sdlsound.hpp"
#include "audiobuffer.hpp"
#include "pokey.hpp"
#include "string.hpp"
#include "new.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_OPENAUDIO
#include <SDL2/SDL.h>
///

/// SDLSound::SDLSound
// Build up the SDL sound now.
SDLSound::SDLSound(class Machine *mach)
  : Sound(mach), SDLClient(mach,SDL_INIT_AUDIO),
    SoundInit(false), Paused(true), MayRunPokey(false), UpdateBuffer(false), ForceStereo(false),
    FragSize(9), NumFrags(6),
    CycleCarry(0), ConsoleVolume(32), BufferedSamples(0), UpdateSamples(0)
{
#if defined _WIN32
  // Windows DirectSound/WaveOut doesn't seem to be able
  // to catch up with higher frequencies...
  FragSize     = 10;
  SamplingFreq = 22050;
#endif
#if defined OS2
  // OS/2 doesn't seem to be able
  // to catch up with higher frequencies...
  SamplingFreq = 20000;
#endif
}
///

/// SDLSound::~SDLSound
SDLSound::~SDLSound(void)
{
  // Close the sound down
  CloseSound();
}
///

/// SDLSound::GenerateSamples
// Generate the given number (not in bytes, but in number) of audio samples
// and place them into the tail of the ready buffer list.
void SDLSound::GenerateSamples(ULONG numsamples)
{	
  BufferedSamples += Sound::GenerateSamples(numsamples,FragSamples);
}
///

/// SDLSound::CloseSound
// Shut down the sound system by quitting the
// corresponding SDL system.
void SDLSound::CloseSound(void)
{
  if (SoundInit) {  
    // Give the side-thread a chance to leave
    // the lock here.    
    // not any more...
    SoundInit = false;
    // Ok, the sound is running.
    CloseSDL();
  }  
  // Dispose the audio buffer nodes now.
  Sound::CleanBuffer();
}
///

/// SDLSound::OpenSound
// Open the SDL Sound system and initialize it. Figure
// out all the settings we need to forward for sample
// configuration.
void SDLSound::OpenSound(void)
{
  SDL_AudioSpec as,real;
  //
#if CHECK_LEVEL > 0
  if (SoundInit) {
    Throw(ObjectExists,"SDLSound::OpenSound",
	  "The sound system is already open");
  }
#endif
  //
  // Now init the SDL subsystem here.
  OpenSDL();
  SoundInit = true; // now we need to close down.
  //
  // Now initialize the sdl specifications.
  memset(&as,0,sizeof(as));             // clear to shut up valgrind
  as.freq     = SamplingFreq;           // the sampling frequency
  as.format   = AUDIO_U8;               // we prefer unsigned eight bit smaples
  as.channels = (RightPokey || ForceStereo)?2:1;       // mono or stereo
  as.silence  = 128;                    // if we are using unsigned channels, then this is the medium volume: silence
  as.samples  = Uint16(1<<FragSize);    // size of the buffer in samples
  as.size     = 1<<FragSize;            // ditto (mono = #of bytes)
  as.callback = &CallBackEntry;         // call this if we require more data
  as.userdata = this;                   // the object this belongs to: The callback hook requires this
  //
  // Copy the data such that we get the right audio data back, guaranteed.
  real        = as;
  // Open the audio handle. If this fails, throw an error.
  if (SDL_OpenAudio(&as,&real) != 0) {
    Throw(ObjectDoesntExist,"SDLSound::OpenSound",
	  "failed to get the audio specification");
  }
  // Read out the data we get. This might differ from what we requested.
  SamplingFreq = real.freq;
  switch(real.format) {
  case AUDIO_S8:
    SignedSamples = true;
    SixteenBit    = false;
    LittleEndian  = false;
    break;
  case AUDIO_U8:
    SignedSamples = false;
    SixteenBit    = false;
    LittleEndian  = false;
    break;
  case AUDIO_U16LSB:
    SignedSamples = false;
    SixteenBit    = true;
    LittleEndian  = true;
    break;
  case AUDIO_U16MSB:
    SignedSamples = false;
    SixteenBit    = true;
    LittleEndian  = false;
    break;
  case AUDIO_S16LSB:
    SignedSamples = true;
    SixteenBit    = true;
    LittleEndian  = true;
    break;
  case AUDIO_S16MSB:
    SignedSamples = true;
    SixteenBit    = true;
    LittleEndian  = false;
    break;
  default:
    Throw(InvalidParameter,"SDLSound::OpenSound","unknown audio output device");
    break;
  }
  // Get the number of channels. Might be mono or stereo
  switch(real.channels) {
  case 1:
    Stereo        = false;
    Interleaved   = false;
    break;
  case 2:    
    if (RightPokey) {
      Stereo      = false; // this is actually a channel duplication flag
      Interleaved = true;
    } else {
      Stereo      = true;
      Interleaved = false;
    }
    break;
  default:
    Throw(InvalidParameter,"SDLSound::OpenSound","unsupported number of channels");
    break;
  }
  // Get the size of a fragment in samples.
  FragSamples     = as.samples;
  BufferSize      = FragSamples * NumFrags;
  EffectiveFreq   = as.freq;
  BufferedSamples = 0;
  CycleCarry      = 0;
  UpdateBuffer    = false;
  UpdateSamples   = 0;
  Paused          = true;
  //
}
///

/// SDLSound::UpdateSound
// Update the output sound, feed new data into the DSP.
// Delay by the timer or don't delay at all if no 
// argument given.
void SDLSound::UpdateSound(class Timer *delay)
{
  if (EnableSound) {
    // Signal that we must update the sound now since the pokey parameters changed.
    UpdateBuffer = true;
    if (delay) {    
      // Here we are in the VBI state. Check now at the end of the VBI
      // how many bytes are left.
      SDL_LockAudio();
      // Check how many bytes we got buffered. If too many, cut the frequency down.
      if (BufferedSamples > (BufferSize + (FragSamples<<1))) {
	AdjustOverrun();
      }
      SDL_UnlockAudio();
      MayRunPokey = true; // as no one is working with it, go ahead!
      delay->WaitForEvent();    
      MayRunPokey = false;
      SDL_LockAudio();
      // Check whether the buffered bytes are getting too short.
      if (BufferedSamples < FragSamples<<2) {
	// Better enlarge the frequency to avoid the trouble...
	AdjustUnderrun();
	// Refill the buffer immediately to avoid a near buffer stall
	GenerateSamples(FragSamples);
      }    
      SDL_UnlockAudio();
    }
  } else if (delay) {
    // No sound enabled, just wait.
    delay->WaitForEvent();
  }
}
///

/// SDLSound::HBI
// Let the sound driver know that 1/15Khz seconds passed.
void SDLSound::HBI(void)
{  
  if (SoundInit) {
    LONG remaining,samples;
    // Compute the number of samples we need to generate this time.
    remaining      = EffectiveFreq + CycleCarry; // the number of sampling cycles left this time.
    samples        = remaining / PokeyFreq;      // number of samples to generate this time.
    CycleCarry     = remaining - samples * PokeyFreq; // keep the number of samples we did not take due to round-off
    // Just note that we could update the buffer here. We delay this to avoid computation of tiny amounts
    // of samples.
    UpdateSamples += samples;
    if (UpdateSamples >= FragSamples) {
      // Ok, we collected enough.
      UpdateBuffer = true;
    }
    if (UpdateBuffer) {
      // Compute this number of samples, and put it into the buffer. This must not
      // interact with the audio sample request.
      SDL_LockAudio();
      GenerateSamples(UpdateSamples);
      SDL_UnlockAudio();
      UpdateBuffer = false;
      UpdateSamples  = 0;
    }
    //
    // Start the audio playback now if we haven't done so before.
    if (Paused && BufferedSamples > BufferSize) {
      SDL_PauseAudio(0);
      Paused = false;
    }
    /*
    static LONG int effective;
    if (effective != EffectiveFreq) {
      effective = EffectiveFreq;
      // New effective frequency is now
      printf("%ldHz\n",effective);
    }
    */
  }
}
///

/// SDLSound::AdjustOverrun
// Signal a buffer overrun
void SDLSound::AdjustOverrun(void)
{
  LONG newfreq;
  // The buffer is running too full. This means we are
  // generating samples too fast. Reduce the sampling frequency.
  // We must do this very carefully as overruns accumulate data
  newfreq = (EffectiveFreq * 8191) >> 13;
  if (newfreq >= EffectiveFreq)
    newfreq--;
  EffectiveFreq = newfreq;
  // Drop all samples we delayed generation for.
  UpdateSamples = 0;
}
///

/// SDLSound::AdjustUnderrun
// Signal a buffer underrun
void SDLSound::AdjustUnderrun(void)
{
  LONG newfreq;
  // The buffer is running empty. We are generating
  // too few samples. Enlarge the effective frequency to keep track.
  newfreq = (EffectiveFreq << 13) / 8191;
  if (newfreq <= EffectiveFreq)
    newfreq++;
  if (newfreq <= (SamplingFreq << 1))
    EffectiveFreq = newfreq;
  // Enforce that we write a new buffer immediately in case
  // we are short on samples.
  UpdateBuffer  = true;
}
///

/// SDLSound::CallBackEntry
// The SDL Callback hook that computes more samples here. This is just the 
// wrapper that loads the "this" poiner and calls the real hook function.
void SDLSound::CallBackEntry(void *data,Uint8 *stream,int len)
{
  class SDLSound *sound = (class SDLSound *)data; // get the "this" pointer.
  sound->CallBack((UBYTE *)stream,len);
}
///

/// SDLSound::CallBack
// The real callback hook called to compute more samples.
// This uses pokey to generate the samples required by SDL.
void SDLSound::CallBack(UBYTE *stream, int bytes)
{
  // Do not perform any computation in case the sound is no longer active.
  if (SoundInit) {
    // Check whether we have an active sample. If so, use it first to satisfy the request.
    while(bytes) {
      if (PlayingBuffer == NULL) {
	// No playing buffer. Pull a new one from the list of ready buffers.
	PlayingBuffer = ReadyBuffers.RemHead();
	if (PlayingBuffer == NULL) {
	  // Ok, check whether we may launch pokey directly here. This might
	  // be valid if the main thread is waiting in the VBI anyhow.
	  if (MayRunPokey) {
	    GenerateSamples(FragSamples);
	    continue;
	  } else {
	    // Unfortunately, we cannot directly call pokey
	    // here since we don't know the state of it and whether something
	    // else is currently playing with it.
	    AdjustUnderrun();
	    return;
	  }
	}
      }
      if (PlayingBuffer) {
	int cpy;
	// We do have a buffer now. Ok, copy as many bytes as required into the output buffer.
	cpy = PlayingBuffer->ReadyBytes();
	if (cpy > bytes)
	  cpy = bytes;
	memcpy(stream,PlayingBuffer->ReadPtr,cpy);
	// Adjust the playing buffer pointers now.
	PlayingBuffer->ReadPtr += cpy;
	stream                 += cpy;
	bytes                  -= cpy;
	BufferedSamples        -= cpy >> PlayingBuffer->SampleShift;
	//
	// Check whether anything remains. If not, put the buffer
	// back into the free list.
	if (PlayingBuffer->IsEmpty()) {
	  // We're done with it. Dispose it.
	  FreeBuffers.AddTail(PlayingBuffer);
	  PlayingBuffer = NULL;
	}
      }
    }    
    if (BufferedSamples < FragSamples) {
      // Better enlarge the frequency to avoid the trouble...
      AdjustUnderrun();
    }
  }
}
///

/// SDLSound::ConsoleSpeaker
// Turn the console speaker on or off:
void SDLSound::ConsoleSpeaker(bool onoff)
{  
  if (ConsoleSpeakerStat != onoff) {
    ConsoleSpeakerStat = onoff;
    UpdateBuffer       = true;
    UpdateSound(NULL);
  }
}
///

/// SDLSound::ColdStart
// Coldstart the audio system. Uhm. Should we do something here?
void SDLSound::ColdStart(void)
{
  LeftPokey  = machine->Pokey(0);
  RightPokey = machine->Pokey(1);
  // Check whether the user requested output. If so, run the SDL
  // frontend
  if (EnableSound && SoundInit==false) {
    OpenSound();
  }
  //
  // Now run for the warmstart.
  WarmStart();
}
///

/// SDLSound::WarmStart
// Warm start the sound. There is not much to do.
void SDLSound::WarmStart(void)
{
  ULONG minsamples   = (NumFrags - 2)<<FragSize;
  ConsoleSpeakerStat = false;  
  //
  // Dispose the old audio buffer nodes now.
  CleanBuffer();
  BufferedSamples    = 0;
  //
  // Reset generation frequency
  EffectiveFreq   = SamplingFreq;
  // Fill the audio buffer here.
  GenerateSamples(minsamples);
}
///

/// SDLSound::DisplayStatus
// Display the current settings of the sound module
void SDLSound::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Audio Output Status:\n"
		   "\tAudio output enable     : %s\n"
		   "\tConsole speaker enable  : %s\n"
		   "\tConsole speaker volume  : " LD "\n"
		   "\tSampling frequency      : " LD "Hz\n"
		   "\tFragment size exponent  : " LD "\n"
		   "\tChannel duplication     : %s\n"
		   "\tStereo sound            : %s\n"
		   "\tChannel bit depth       : %d\n"
		   "\tAudio data is           : %s\n",
		   (EnableSound)?("on"):("off"),
		   (EnableConsoleSpeaker)?("on"):("off"),
		   ConsoleVolume,
		   SamplingFreq,
		   FragSize,
		   Stereo?("on"):("off"),
		   Interleaved?("on"):("off"),
		   SixteenBit?16:8,
		   SignedSamples?("signed"):("unsigned")
		   );
}
///

/// SDLSound::ParseArgs
// Parse off arguments relevant for us
void SDLSound::ParseArgs(class ArgParser *args)
{
  LeftPokey  = machine->Pokey(0);
  RightPokey = machine->Pokey(1);
  
  args->DefineTitle("SDLSound");
  args->DefineBool("EnableSound","enable audio output",EnableSound);
  args->DefineBool("EnableConsoleSpeaker","enable the console speaker",
		   EnableConsoleSpeaker);
  args->DefineBool("ForceStereo","enforce stereo output for broken ALSA interfaces",ForceStereo);
  args->DefineLong("ConsoleSpeakerVolume","set volume of the console speaker",
		   0,64,ConsoleVolume);
  args->DefineLong("SampleFreq","set audio sampling frequency",
		   4000,48000,SamplingFreq);
  args->DefineLong("FragSize","set the exponent of the fragment size",
		   2,16,FragSize);
  args->DefineLong("NumFrags","specify the number of fragments",
		   4,256,NumFrags);  
  
  // Re-read the base frequency
  PokeyFreq = LeftPokey->BaseFrequency();
  // Close the SDL output device now, ready for re-opening it if required.
  CloseSound();
  if (EnableSound) {
    OpenSound();
  }
}
///

///
#endif
  
