/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: directxsound.cpp,v 1.14 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Sound frontend for direct X sound output under Win32
 ** and related M$ "operating systems".
 **********************************************************************************/

/// Includes
#include "types.h"
#include "sound.hpp"
#include "machine.hpp"
#include "monitor.hpp"
#include "argparser.hpp"
#include "timer.hpp"
#include "pokey.hpp"
#include "cpu.hpp"
#include "exceptions.hpp"
#include "directxsound.hpp"
#include "audiobuffer.hpp"
#include "unistd.hpp"
#include "new.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_INITSUBSYSTEM
///

/// DirectXSound::DirectXSound
DirectXSound::DirectXSound(class Machine *mach)
  : Sound(mach), SDLClient(mach,0),
    SoundStream(NULL), current(NULL),
    FragSize(8), NumFrags(6),
    BufferedSamples(0), UpdateBuffer(false), UpdateSamples(0)
{
  SamplingFreq = 22050;
}
///

/// DirectXSound::~DirectXSound
DirectXSound::~DirectXSound(void) 
{
  if (current)
    FreeBuffers.AddTail(current);
  if (SoundStream)
    delete SoundStream;
  //
  // Audio buffers get disposed by the Sound
}
///

/// DirectXSound::GenerateSamples
// Generate the given number (not in bytes, but in number) of audio samples
// and place them into the tail of the ready buffer list.
void DirectXSound::GenerateSamples(ULONG numsamples)
{	
  // Let the parent class do the job
  BufferedSamples += Sound::GenerateSamples(numsamples,FragSamples);
}
///

/// DirectXSound::FeedDevice
// Feed data into the dsp device by taking buffered bytes from the queue
// and removing the sample buffers, putting them back into the free list.
// This returns false on a buffer underrun
bool DirectXSound::FeedDevice(class Timer *delay)
{
  void *buffer;
  int size;
  bool result = true;
  //
  if (delay) {
    // we have enough time to wait for the availibility of
    // buffers, so do now.
    buffer = SoundStream->NextBuffer(size,delay->GetMicroDelay());
  } else {
    // Otherwise, there's not much to delay, so don't delay at
    // all and do not provide data if the buffer doesn't take
    // data.
    buffer = SoundStream->NextBuffer(size,0);
  }
  if (buffer) {
    UBYTE *out = (UBYTE *)buffer;
    ULONG copy;
    ULONG bytes = ULONG(size);
    // The device is ready to take more samples. Hence, pull them from the ready queue
    while(bytes) {
      // Check whether we still have an active input buffer. If so, use this one,
      // otherwise fetch a new buffer.
      if (current == NULL) {
	current = ReadyBuffers.RemHead();
      }
      if (current == NULL) {
	// Generate one fragment of data.
	GenerateSamples(FragSamples);
	result = false;
	continue;
      }
      copy = current->ReadyBytes();
      if (copy == 0L) {
	FreeBuffers.AddTail(current);
	current = NULL;
	continue;
      }
      if (copy > bytes)
	copy = bytes;
      //
      // Copy the data of this buffer over now.
      memcpy(out,current->ReadPtr,copy);
      BufferedSamples  -= copy >> current->SampleShift;
      // And advance the buffer position.
      out              += copy;
      current->ReadPtr += copy;
      bytes            -= copy;
      if (current->ReadPtr >= current->BufferEnd) {
	// This buffer is used up, release it and fetch the next one.
	FreeBuffers.AddTail(current);
	current = NULL;
      }
    }
    SoundStream->ReleaseBuffer(buffer,size);
  } else if (SoundStream->IsActive() == false) {
    // No new data required, but not playing -> play buffer is now
    // filled: Now run the buffer.
    SoundStream->Start();
  }
  return result;
}
///

/// DirectXSound::ColdStart
// Run a coldstart. Also initializes the dsp device if we haven't done so
// before. Or at least, it tries to.
void DirectXSound::ColdStart(void)
{
  LeftPokey  = machine->Pokey(0);
  RightPokey = machine->Pokey(1);
  //
  // Now run for the warmstart.
  WarmStart();
}
///

/// DirectXSound::WarmStart
// Run a simple reset here.
void DirectXSound::WarmStart(void)
{
  ConsoleSpeakerStat = false;
  //
  // Dispose the old buffers now.
  CleanBuffer();
  EffectiveFreq		 = SamplingFreq;
  DifferentialAdjust = 0;
  BufferedSamples    = 0;
}
///

/// DirectXSound::ConsoleSpeaker
// Turn the console speaker on or off:
void DirectXSound::ConsoleSpeaker(bool onoff)
{
  if (ConsoleSpeakerStat != onoff) {
    ConsoleSpeakerStat = onoff;
    UpdateBuffer       = true;
    UpdateSound(NULL);
  }
}
///

/// DirectXSound::InitializeDsp
// Opens the sound device /dev/dsp and installs all
// user parameters here. Call this only if the sound
// is enabled.
// Return true on success, false on error.
bool DirectXSound::InitializeDsp(void)
{
  //
  if (SoundStream == NULL) {
    void *window = NULL;
    // Ok, the stream is not yet open. Do now.
    //
    // Grab the output window from SDL. This is unfortunately
    // required as this #§!""^!!! win32 API requires a window
    // to play sound (now, that makes sense...)
    window = DXSound::GetSDLWindowHandle();
    //
    //
    if (window) {
      // Ok, we now have a window. This allows us finally to open
      // the SDL interface.
      SoundStream = new DXSound;
      //
      // Initialize the sound stream. Try to use stereo if available.
      if (!SoundStream->SetupDXSound(window,(RightPokey)?2:1,SamplingFreq,8,FragSize,NumFrags)) {
	// Did not. Bummer.
	machine->PutWarning("Audio Setup:\n"
			    "Couldn't start the DirectSound audio output, disabling it for now.\n"
			    "For the next time, either make DirectX available or disable the\n"
			    "sound output.\n");
	return false;
      }
      //
      // Now check what we really got. Might be different...
      switch (SoundStream->ChannelDepthOf()) {
      case 8:
	SignedSamples = false;
	SixteenBit    = false;
	LittleEndian  = true; // we are wintel world, we are always little endian.
	break;
      case 16:
	SignedSamples = true; // actually, I don't know...
	SixteenBit    = true;
	LittleEndian  = true;
	break;
      default:
	machine->PutWarning("Audio Setup:\n"
			    "Unsupported sample format for audio output, disabling it for now.\n");
	return false;
      }
      //
      // Check now for the number of channels we really got.
      switch(SoundStream->ChannelsOf()) {
      case 1:
	// Must do mono output.
	Stereo      = false;
	Interleaved = false;
	FragSamples = SoundStream->ChunkSizeOf();
	break;
      case 2:
	if (RightPokey) {
	  Stereo      = false; // This is actually only a channel duplication flag.
	  Interleaved = true;
	} else {
	  Stereo      = true;
	  Interleaved = false;
	}
	FragSamples = SoundStream->ChunkSizeOf() >> 1;
	break;
      default:
	machine->PutWarning("Audio Setup:\n"
			    "Unsupported number of channels for audio output, disabling it for now.\n");
	return false;
      }
      //
      // Also fix up the fragment size from bytes to samples for 16 bit
      // audio.
      if (SixteenBit)
	FragSamples >>= 1;
      //
      // Also adjust the number of fragments.
      NumFrags = SoundStream->NumBuffersOf();
      //
      // Setup the effective buffering frequency.
      EffectiveFreq   = SamplingFreq;
      CycleCarry      = 0;
      UpdateBuffer    = false;
      UpdateSamples   = 0;
      //
      return true;
    } else {
      // No window pointer. Bummer.
      machine->PutWarning("Audio Setup:\n"
			  "Unable to retrieve the window handle for audio output, disabling it for now.\n");
      return false;
    }
  }
  return false;
}
///

/// DirectXSound::HBI
// Let the sound driver know that 1/15Khz seconds passed.
// This might be required for resynchronization of the
// sound driver.
void DirectXSound::HBI(void)
{
  if (EnableSound && SoundStream) {
    LONG  remaining,samples;
    ULONG cycles   = machine->CPU()->ElapsedCycles();
    // Compute the number of samples we need to generate this time.
    // Note that the pokey base frequency is the HBI frequency. The number of clocks
    // per HBI is 114.
    remaining      = EffectiveFreq + DifferentialAdjust; // the number of sampling cycles left this time.
    // number of samples to generate this time.
    samples        = (remaining * cycles + CycleCarry) / (PokeyFreq * 114);         
    // keep the number of samples we did not take due to round-off   
    CycleCarry    += remaining  * cycles - samples * PokeyFreq * UQUAD(114); 
    UpdateSamples += samples;
    //
    if (UpdateSamples > 0) {
      // Compute this number of samples, and put it into the buffer.
      GenerateSamples(UpdateSamples);
      UpdateSamples     = 0;
      UpdateBuffer      = false;
    }
  }
}
///

/// DirectXSound::VBI
// Provide a custom VBI that disables the sound in case
// we are only pausing here. Otherwise, perform the same
// operation as the super class.
void DirectXSound::VBI(class Timer *time,bool quick,bool pause)
{
  if (quick == false) {
    if (pause) {
      // No sound output on a paused machine
      if (SoundStream && SoundStream->IsActive())
	SoundStream->Stop();
      time->WaitForEvent();
    } else {
      UpdateSound(time);
    }
  }
}
///

/// DirectXSound::UpdateSound
// Update the output sound, feed new data into the DSP.
// Delay by the timer or don't delay at all if no 
// argument given.
void DirectXSound::UpdateSound(class Timer *delay)
{
  if (SoundStream == NULL && EnableSound) {
    if (!InitializeDsp()) {
      delete SoundStream;
      SoundStream = NULL;
      EnableSound = false;
    }
  }
  // Check whether we want the sound at all. In case we don't, we
  // do not need all this and delay with the timer.
  if (EnableSound) {
    // Signal that we must now re-generate some samples as the audio
    // setting changed.
    UpdateBuffer       = true;
    DifferentialAdjust = 0;
    // Ok, re-feed the device. In case of underrun, generate some samples manually.
    do {
	  if (!FeedDevice(delay)) {
		GenerateSamples((FragSamples << 1) - BufferedSamples);
		AdjustUnderrun();
	  }
      // If there is nothing to delay, bail out.
      if (delay == NULL) break;
      // Otherwise, delay until the delay time is over.
    } while(!delay->EventIsOver());
    //
    // Now check for the number of bytes in the buffer. This should not
    // grow endless. If it is too large, signal an overrun and adjust the
    // frequency accordingly.
    if (BufferedSamples > ULONG(NumFrags-2) * FragSamples) {
      AdjustOverrun();
    }
    //
    // If there is only one fragment in the buffer, signal underrun as well
    if (delay && BufferedSamples < (FragSamples << 1)) {
      GenerateSamples((FragSamples << 1) - BufferedSamples);
      AdjustUnderrun();
    }
  } else {
    // Sound has been disabled. Just do the wait if we have to wait.
    if (delay) {
      delay->WaitForEvent();
    }
  }
}
///

/// DirectXSound::AdjustOverrun
// Signal a buffer overrun
void DirectXSound::AdjustOverrun(void)
{
  LONG newfreq;
  // The buffer is running too full. This means we are
  // generating samples too fast. Reduce the sampling frequency.
  // We must do this very carefully as overruns accumulate data
  newfreq = (EffectiveFreq * 16383) >> 14;
  if (newfreq >= EffectiveFreq && newfreq > 0)
    newfreq--;
  EffectiveFreq = newfreq; 
  DifferentialAdjust = -(LONG(BufferedSamples - FragSamples * NumFrags) * newfreq) >> 12;
  if (-DifferentialAdjust >= (newfreq >> 1))
    DifferentialAdjust = -(newfreq >> 1);
  //
  // Drop buffer bytes we should have generated so far.
  UpdateSamples = 0;
  //printf("Overrun! Freq now=%ld\n",EffectiveFreq);
}
///

/// DirectXSound::AdjustUnderrun
// Signal a buffer underrun
void DirectXSound::AdjustUnderrun(void)
{
  LONG newfreq;
  // The buffer is running empty. We are generating
  // too few samples. Enlarge the effective frequency to keep track.
  newfreq = (EffectiveFreq << 12) / 4094;
  if (newfreq <= EffectiveFreq)
    newfreq++;
  EffectiveFreq = newfreq;
  // We underrrun, or are near to underrun. We'd better update the
  // the buffer and flush all buffered bytes to prevent the worst.
  UpdateBuffer = true;
  //printf("Underrun! Freq now=%ld\n",EffectiveFreq);
}
///

/// DirectXSound::DisplayStatus
// Display the status of the sound over the monitor
void DirectXSound::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Audio Output Status:\n"
		   "\tAudio output enable            : %s\n"
		   "\tConsole speaker enable         : %s\n"
		   "\tConsole speaker volume         : " LD "\n"
		   "\tSampling frequency             : " LD "Hz\n"
		   "\tFragment size exponent         : " LD "\n"
		   "\tNumber of fragments            : " LD "\n"
		   "\tNumber of samples in the queue : " LD "\n"
		   "\tEffective sampling frequency   : " LD "Hz\n"
		   "\tChannel duplication            : %s\n"
		   "\tStereo sound                   : %s\n"
		   "\tChannel bit depth              : %d\n"
		   "\tAudio data is                  : %s\n",
		   (EnableSound)?("on"):("off"),
		   (EnableConsoleSpeaker)?("on"):("off"),
		   ConsoleVolume,
		   SamplingFreq,
		   FragSize,
		   NumFrags,
		   BufferedSamples,
		   EffectiveFreq,
		   Stereo?("on"):("off"),
		   Interleaved?("on"):("off"),
		   SixteenBit?16:8,
		   SignedSamples?("signed"):("unsigned")
		   );
}
///

/// DirectXSound::ParseArgs
// Parse off command line arguments for the sound handler
void DirectXSound::ParseArgs(class ArgParser *args)
{
  bool enable = EnableSound;
  
  LeftPokey  = machine->Pokey(0);
  RightPokey = machine->Pokey(1);
  
  args->DefineTitle("DirectXSound");
  args->DefineBool("EnableSound","enable audio output",enable);
  args->DefineBool("EnableConsoleSpeaker","enable the console speaker",
		   EnableConsoleSpeaker);
  args->DefineLong("ConsoleSpeakerVolume","set volume of the console speaker",
		   0,64,ConsoleVolume);
  args->DefineLong("SampleFreq","set audio sampling frequency",
		   4000,48000,SamplingFreq);
  args->DefineLong("FragSize","set the exponent of the fragment size",
		   2,12,FragSize);
  args->DefineLong("NumFrags","specify the number of fragments",
		   6,16,NumFrags);  

  // Re-read the base frequency
  PokeyFreq = LeftPokey->BaseFrequency();
  // Close the sound stream to be able to re-open it.
  if (SoundStream) {
    delete SoundStream;
    SoundStream = NULL;
  }
}
///

///
#endif
///

