/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdlsound.hpp,v 1.17 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: This class implements audio output thru the SDL library
 ** as available.
 **********************************************************************************/

#ifndef SDLSOUND_HPP
#define SDLSOUND_HPP

/// Includes
#include "types.h"
#include "sdlclient.hpp"
#include "sound.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_OPENAUDIO
#include <SDL2/SDL.h>
#endif
///

/// Class SDLSound
// This class implements the interface towards audio output
#if HAVE_SDL2_SDL_H && HAVE_SDL_OPENAUDIO
class SDLSound : public Sound, public SDLClient {
  //
  bool             SoundInit;   // set if the sound system is running.
  bool             Paused;      // set if audio output is still paused
  bool             MayRunPokey; // set if the side thread may run pokey sound processing
  bool             UpdateBuffer;// set if the buffer must be recalculated and cannot be delayed
  bool             ForceStereo; // enforce stereo output even if mono is sufficient.
  //
  // Various sound configuration issues: This is very
  // similar to most other audio output classes.
  //
  // Fragment size: This is the size of the DMA buffer in bytes
  // as the exponent of the base two. Note that differs from the
  // SDL mechanism that requires here an ordinary number. We convert
  // as required, but remain compatible to all other SDL classes.
  LONG             FragSize;
  //
  // Number of fragments here.
  LONG             NumFrags;
  //
  // frequency carry-over from last computation loop
  LONG             CycleCarry;
  //
  // The following data is entered by the setup algorithm:
  //
  ULONG            BufferSize;    // audio output buffer in samples (as computed by SDL)
  ULONG            FragSamples;   // samples in a fragment
  //
  LONG             ConsoleVolume; // volume of the console speaker
  //  
  // The effective frequency for buffer refill
  LONG             EffectiveFreq;
  //
  // Total number of samples in the output buffer.
  ULONG            BufferedSamples;
  //
  // Total number of buffers we delayed for update
  ULONG            UpdateSamples;
  //
  // The SDL Callback hook that computes more samples here. This is just the 
  // wrapper that loads the "this" poiner and calls the real hook function.
  static void CallBackEntry(void *data,Uint8 *stream,int len);
  //
  // The real callback hook called to compute more samples.
  void CallBack(UBYTE *stream,int size);
  //
  // Generate the given number (not in bytes, but in number) of audio samples
  // and place them into the tail of the ready buffer list.
  void GenerateSamples(ULONG numsamples);
  //
  // Open resp. close the SDL frontend for this class. This gets done for
  // enabled audio.
  void OpenSound(void);
  void CloseSound(void);
  //  
  // Signal a buffer overrun
  void AdjustOverrun(void);
  //
  // Signal a buffer underrun
  void AdjustUnderrun(void);
  //
public:
  // Open the SDL Sound class
  SDLSound(class Machine *mach);
  ~SDLSound(void);
  //
  // Update the output sound, feed new data into the DSP.
  // Delay by the timer or don't delay at all if no 
  // argument given.
  virtual void UpdateSound(class Timer *delay = NULL);
  //  
  // Let the sound driver know that 1/15Khz seconds passed.
  // This might be required for resynchronization of the
  // sound driver.
  virtual void HBI(void);
  //
  // Turn the console speaker on or off:
  virtual void ConsoleSpeaker(bool);
  //
  // the following are imported by the Chip class:
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Display the current settings of the sound module
  virtual void DisplayStatus(class Monitor *mon);
  // 
  // Parse off arguments relevant for us
  virtual void ParseArgs(class ArgParser *args);
  //
};
#endif
///

///
#endif
  
