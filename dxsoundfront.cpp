/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: dxsoundfront.cpp,v 1.8 2014/02/23 10:41:08 Administrator Exp $
 **
 ** In this module: wrapper class for the DirectX sound interface
 **********************************************************************************/

/// Includes
#include "dxsoundfront.hpp"
#ifdef HAVE_DXSOUND
#define _WIN32_WINNT 0x0500
#define DIRECTSOUND_VERSION 0x0800
#include <stdio.h>
#include <Dsound.h>
#include <dsound.h>
#include <string.h>
#include <Windows.h>
#if HAVE_SDL2_SDL_H
#ifndef WIN32
#define WIN32
#endif
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL.h>
#endif
#endif
#ifndef NULL
#define NULL 0L
#endif
///

/// struct DXWrapper
// Definition of the wrapper structure that keeps the interface to the direct sound
// interface
struct DXWrapper {
  //
#ifdef HAVE_DXSOUND
  // Handle to the sound device
  struct IDirectSound8 *device;
  //
  // Handle to the sound buffer.
  struct IDirectSoundBuffer *buffer;
  //
  // Pointer to an event structure that is
  // used to wait for buffer notifications.
  HANDLE event;
#endif
  //
  // Some buffer settings.
  //
  // Number of channels in here.
  int channels;
  // Sampling frequency
  int freq;
  // Bits per sample
  int bitdepth;
  // Size of one chunk in bytes
  int chunksize;
  // Number of chunks
  int chunks;
  //
  // Number of milliseconds per buffer.
  int millisperframe;
  //
  // Currently fill in buffer #
  int fill;
  // Set if sound output is currently
  // active.
  bool active;
  //
  //
  DXWrapper(void)
  {
#ifdef HAVE_DXSOUND
    device    = NULL;
    buffer    = NULL;
    event     = NULL;
#endif
    // Fill in some suitable defaults.
    channels  = 1;
    freq      = 22050;
    bitdepth  = 8;
    chunksize = 512;
    chunks    = 9;
    active    = false;
  }
  //
  //
  ~DXWrapper(void)
  {
#ifdef HAVE_DXSOUND
    if (buffer) {
      buffer->Stop();
      buffer->Release();
      buffer = NULL;
    }
    if (event) {
      CloseHandle(event);
      event = NULL;
    }
    if (device) {
      device->Release();
      device = NULL;
    }
#endif
  }
  //
  // Allocate the device. Returns false in case the buffer could not
  // be linked.
#ifdef HAVE_DXSOUND
  bool BuildDevice(void *win)
  {
    if (device == NULL) {
      if (DirectSoundCreate8(NULL,&device,NULL) == DS_OK) {
	if (device->SetCooperativeLevel((HWND)win,DSSCL_PRIORITY) == DS_OK) {
	  return true;
	}
      }
      return false;
    }
    // Is created already. Let it go.
    return true;
  }
#else
  bool BuildDevice(void *)
  {
    return false;
  }
#endif
  //
  // Allocate the buffer with the given characteristics
  // provided these are supported. May return false in case
  // the selected buffer settings are not available.
  bool BuildBuffer(void)
  {
#ifdef HAVE_DXSOUND
    if (buffer == NULL && device) {
      WAVEFORMATEX waveformat;
      DSBUFFERDESC buftype;
      
      waveformat.wFormatTag      = WAVE_FORMAT_PCM;
      waveformat.nChannels       = channels;
      waveformat.nSamplesPerSec  = freq;
      waveformat.wBitsPerSample  = bitdepth;
      waveformat.nBlockAlign     = waveformat.nChannels * (waveformat.wBitsPerSample >> 3);
      waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec * waveformat.nBlockAlign;
      waveformat.cbSize          = 0;
      buftype.dwBufferBytes      = chunksize * chunks;
      buftype.dwFlags            = DSBCAPS_CTRLPOSITIONNOTIFY|DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_STATIC;
      buftype.dwSize             = sizeof(buftype);
      buftype.guid3DAlgorithm    = GUID_NULL;
      buftype.lpwfxFormat        = &waveformat;
      buftype.dwReserved         = 0;
      if (device->CreateSoundBuffer(&buftype,&buffer,NULL) == DS_OK) {
	// Start filling at buffer #0
	fill = 0;
	return true;
      }
      return false;
    }
    if (buffer)
      return true;
    return false;
#else
    return false;
#endif
  }
  //
  // After having opened the device, query its characteristics and adjust
  // the parameters to something the device claims to be able to.
  bool AdjustSettings(void)
  {
#ifdef HAVE_DXSOUND
    if (device) {
      DSCAPS cap;
      cap.dwSize = sizeof(cap);
      if (device->GetCaps(&cap) == DS_OK) {
	// Check whether we are within the limits. If not so, clip to the limits
	// Fix broken settings in case min/max are both zero.
	if (cap.dwMinSecondarySampleRate == 0 && cap.dwMaxSecondarySampleRate == 0) {
	  // Let's hope the best...
	  cap.dwMinSecondarySampleRate = 11025;
	  cap.dwMaxSecondarySampleRate = 48000;
	}
	if (freq > (int)cap.dwMaxSecondarySampleRate)
	  freq = cap.dwMaxSecondarySampleRate;
	if (freq < (int)cap.dwMinSecondarySampleRate)
	  freq = cap.dwMinSecondarySampleRate;
	//
	// Now check for the buffer format.
	if (cap.dwFlags & DSCAPS_PRIMARY16BIT) {
	  // 16 bit audio is supported. Ok, if so, then
	  // check whether 8 bit is not supported and we have
	  // 8 bit selected. If so, choose better 16 bit since
	  // the transformation overhead will be lower.
	  if (bitdepth == 8 && (cap.dwFlags & DSCAPS_PRIMARY8BIT) == 0)
	    bitdepth = 16;
	}
	// Check whether 8 bit samples are supported.
	if (cap.dwFlags & DSCAPS_PRIMARY8BIT) {
	  if (bitdepth == 16 && (cap.dwFlags & DSCAPS_PRIMARY16BIT) == 0) {
	    bitdepth = 8;
	  }
	}
	//
	// Correct channel settings to some suitable values.
	if (channels > 2)
	  channels = 2;
	if (channels < 1)
	  channels = 1;
	// Check whether we support mono or stereo, adjust
	// accordingly.
	if (cap.dwFlags & DSCAPS_PRIMARYMONO) {
	  // Check whether we selected stereo but this is unsupported.
	  if (channels == 2 && (cap.dwFlags & DSCAPS_PRIMARYMONO) == 0)
	    channels = 1;
	}
	if (cap.dwFlags & DSCAPS_PRIMARYSTEREO) {
	  if (channels == 1 && (cap.dwFlags & DSCAPS_PRIMARYSTEREO) == 0)
	    channels = 2;
	}
	//
	// Ok, now as we have useful suggestions, check for what the secondary
	// buffer can do for us - since this is what we allocate. This should
	// be more than what the primary buffer supports, I hope.
	if ((cap.dwFlags & DSCAPS_SECONDARY8BIT) == 0) {
	  bitdepth = 16;
	}
	if ((cap.dwFlags & DSCAPS_SECONDARY16BIT) == 0) {
	  bitdepth = 8;
	}
	if ((cap.dwFlags & DSCAPS_SECONDARYSTEREO) == 0) {
	  channels = 1;
	}
	if ((cap.dwFlags & DSCAPS_SECONDARYMONO) == 0) {
	  channels = 2;
	}
	//
	// Double buffer sizes for 16 bit and stereo. This is because
	// DirectX measures buffer sizes in bytes.
	if (channels >= 2)
	    chunksize <<= 1;
	if (bitdepth > 8)
	    chunksize <<= 1;
	//
	// First clip the chunk size to some suitable values.
	if (chunksize < 32)
	  chunksize = 32;
	while(chunksize >= DSBSIZE_MAX/3) {
	  chunksize >>= 1;
	}
	if (chunks < 3)
	  chunks = 3;
	if (chunks > 256)
	  chunks = 256;
	//
	// Then clip to the hard limits win sets.
	if (chunksize * chunks < DSBSIZE_MIN)
	  chunks = (DSBSIZE_MIN + chunksize - 1) / chunksize;
	if (chunksize * chunks > DSBSIZE_MAX)
	  chunks = DSBSIZE_MAX / chunksize;
	//
	// Check for the buffer size.
	if (cap.dwTotalHwMemBytes) {
	  // Check whether the selected buffer size is too
	  // large. If so, try to truncate the number of buffers,
	  // though not their size.
	  if (chunksize * chunks > (int)cap.dwTotalHwMemBytes) {
	    chunks = cap.dwTotalHwMemBytes / chunksize;
	  }
	}
	if (chunks < 3)
	  chunks = 3;
	//
	millisperframe = 1000 * chunksize / freq;
	// Capabilities managed correctly now.
	return true;
      }
    }
#endif
    // Device not open, or unable to query capabilities.
    return false;
  }
  //
  // Start the sound output
  bool Play(void)
  {
#ifdef HAVE_DXSOUND
    if (buffer && device) {
      HRESULT hr = buffer->Play(0,0,DSBPLAY_LOOPING);
      if (hr == DS_OK) {
	active = true;
	return true;
      } else if (hr == DSERR_BUFFERLOST) {
	if (buffer->Restore() == DS_OK) {
	  if (buffer->Play(0,0,DSBPLAY_LOOPING) == DS_OK) {
	    active = true;
	    return true;
	  }
	}
      }
    }
#endif
    return false;
  }
  //
  // Stop the sound output
  bool Stop(void)
  {
#ifdef HAVE_DXSOUND
    if (buffer && device) {
      if (buffer->Stop() == DS_OK) {
	active = false;
	return true;
      }
    }
#endif
    return false;
  }
  //
  // Install buffer notification positions to be able to wait for
  // a buffer done event.
  bool InstallBufferNotifications()
  {
#ifdef HAVE_DXSOUND
    if (buffer && device) {
      LPDIRECTSOUNDNOTIFY8 notify;
      //HRESULT hr = buffer->QueryInterface(IID_IDirectSoundNotify8,(LPVOID *)&notify);
      if (buffer->QueryInterface(IID_IDirectSoundNotify8,(LPVOID *)&notify) == DS_OK) {
	int i;
	event = CreateEvent(NULL,true,false,NULL);
	DSBPOSITIONNOTIFY posnotify;
	for(i = 0;i<chunks;i++) {
	  posnotify.dwOffset     = chunksize * i;
	  posnotify.hEventNotify = event;
	  if (notify->SetNotificationPositions(1,&posnotify) != DS_OK)
	    return false;
	}
	return true;
      }
    }
#endif
    return false;
  }
  //
  // Get a pointer and the length of the next buffer segment to
  // fill in. Return NULL in case no free buffer segment is
  // currently available and the sound card is currently playing.
  // Must be matched with a "freebuffer" call once we're done.
#ifdef HAVE_DXSOUND
  void *NextBuffer(int &size,int wait)
  {
    int loops = 0;
    size = 0;
    if (buffer) {
      DWORD play  = 0,write = 0;
      DWORD first,last;
      void *data1,*data2;
      DWORD size1=0,size2=0;
      //
      // We only use the play buffer, the write buffer
      // is notoriosly unreliable, unfortunately.
      if (active) {
	do {
	  ResetEvent(event);
	  if (buffer->GetCurrentPosition(&play,&write) == DS_OK) {
	    int delay;
	    // Get first and last buffer that is currently
	    // blocked because it is currently played, or
	    // about to be played. This is the buffer we
	    // are currently in, and the buffer immediately
	    // in front of it.
	    last  = play / chunksize;
	    first = last + 1;
	    if ((int)first >= chunks)
	      first -= chunks;
	    //
	    // Check whether we're "out of danger" and can
	    // allow to fill-in the buffer.
	    if (fill != first && fill != last)
	      break;
	    //
	    // Otherwise, check whether waiting is allowed. If
	    // not so, bail out with a "NULL" buffer pointer
	    // to indicate that we're not ready.
	    if (wait <= 0)
	      return NULL;
	    //
	    // Otherwise, wait until the event comes by and
	    // try again; unfortunately, the event mechanism
	    // is rather imprecise, so also wait at most one
	    // frame.
	    delay = wait;
	    if (delay > millisperframe)
	      delay = millisperframe;
		if (delay > 3) {
	    WaitForSingleObject(event,delay);
	    ResetEvent(event);
		}
	    wait -= delay;
	    loops++;
	  }
	} while(true);
      } else {
		  
	// Inactive, buffer is currently not playing. Allow filling
	// all buffers up to the last which we keep as
	// reserve.
	if (fill >= chunks-1)
	  return NULL;
      }
      HRESULT hr;
      //
      // Otherwise, this buffer is inactive and we can fill it.
      hr = buffer->Lock(fill * chunksize,chunksize,&data1,&size1,&data2,&size2,0);
      if (hr == DS_OK) {
	// Only return the first part. This should be enough since
	// we always align the buffers correctly. I hope...
	size = size1;
	return data1;
      } else if (hr == DSERR_BUFFERLOST) {
	if (buffer->Restore() == DS_OK) {
	  if (buffer->Lock(fill * chunksize,chunksize,&data1,&size1,&data2,&size2,0) == DS_OK){ 
	    size = size1;
	    return data1;
	  }
	}
      }
    }
    return NULL;
  }
#else
  void *NextBuffer(int &,int)
  {
    return NULL;
  }
#endif
  //
  // Release the allocated buffer again and advance to the next.
#ifdef HAVE_DXSOUND
  void ReleaseBuffer(void *buf,int size)
  {
    if (buffer) {
      void *data1 = buf;
      void *data2 = NULL;
      DWORD size1 = size;
      DWORD size2 = 0;
      if (buffer->Unlock(data1,size1,data2,size2) == DS_OK) {
	// Ok, next time go for the next buffer.
	fill++;
	if (fill >= chunks)
	  fill = 0;
      }
    }
  }
#else
  void ReleaseBuffer(void *,int)
  {
  }
#endif
};
///

/// DXSound::DXSound
// Constructor for the sound wrapper
DXSound::DXSound(void)
  : wrap(NULL)
{
}
///

/// DXSound::~DXSound
// Constructor for the sound wrapper
DXSound::~DXSound(void)
{
  if (wrap) {
    CloseSound();
  }
}
///

/// DXSound::SetupDXSound
// Setup for the given characteristics: Stereo (or not). This
// setting might not be honored if stereo is not available.
// Frequency (might not be honored) sample depth (8 or 16 bit)
// This requires a "handle" to a Window.
// The chunkexp describes the length of one buffer as the exponent
// to the power of two.
// The buffer settings the number of buffers to be allocated for it.
// Returns a true/false indicator.
bool DXSound::SetupDXSound(void *window,int nChannels,int frequency,int depth,int chunkexp,int nbuffers)
{
  if (wrap == NULL) {
    wrap = new DXWrapper;
    //
    if (wrap->BuildDevice(window)) {
      // Install buffer settings into the wrapper class.
      wrap->channels  = nChannels;
      wrap->bitdepth  = depth;
      wrap->chunks    = nbuffers;
      wrap->chunksize = 1<<chunkexp;
      wrap->freq      = frequency;
      if (wrap->AdjustSettings()) {
	if (wrap->BuildBuffer()) {
	  if (wrap->InstallBufferNotifications()) {
	    return true;
	  }
	}
      }
    }
    delete wrap;
    wrap = NULL;
    return false;
  }
  //
  // Is already open.
  return true;
}
///

/// DXSound::ChannelsOf
// Return the number of available channels. Might be one or two.
int DXSound::ChannelsOf(void)
{
  if (wrap) {
    return wrap->channels;
  }
  return 0;
}
///

/// DXSound::FrequencyOf
// Return the sampling frequency in Hz
int DXSound::FrequencyOf(void)
{
  if (wrap) {
    return wrap->freq;
  }
  return 0;
}
///

/// DXSound::ChunkSizeOf
// Return the size of one buffer in bytes
int DXSound::ChunkSizeOf(void)
{
  if (wrap) {
    return wrap->chunksize;
  }
  return 0;
}
///

/// DXSound::NumBuffersOf
// Return the number of buffers.
int DXSound::NumBuffersOf(void)
{
  if (wrap) {
    return wrap->chunks;
  }
  return 0;
}
///

/// DXSound::ChannelDepthOf
// Return the selected channel depth in bits,
// i.e. the precision of the samples.
int DXSound::ChannelDepthOf(void)
{
  if (wrap) {
    return wrap->bitdepth;
  }
  return 0;
}
///

/// DXSound::CloseSound
// Shutdown the sound
void DXSound::CloseSound(void)
{
  if (wrap) {
    wrap->Stop();
    delete wrap;
    wrap = NULL;
  }
}
///

/// DXSound::NextBuffer
// Return the next available buffer for fill-in, or NULL in case
// we are currently playing and enough buffers are filled. Must
// release this buffer again when done with the fill-in
void *DXSound::NextBuffer(int &size,int wait)
{
  if (wrap) {
    return wrap->NextBuffer(size,wait);
  }
  size = 0;
  return NULL;
}
///

/// DXSound::ReleaseBuffer
// After having filled in data, release it with the following
// call.
void DXSound::ReleaseBuffer(void *buffer,int size)
{
  if (wrap) {
    wrap->ReleaseBuffer(buffer,size);
  }
}
///

/// DXSound::Start
// Start playing the sound
bool DXSound::Start(void)
{
  if (wrap) {
    if (wrap->active) {
      return true;
    } else {
      return wrap->Play();
    }
  }
  return false;
}
///

/// DXSound::Stop
// Stop sound output
void DXSound::Stop(void)
{
  if (wrap) {
    if (wrap->active)
      wrap->Stop();
  }
}
///

/// DXSound::IsActive
// Return a boolean indicator whether the sound
// is currently playing.
bool DXSound::IsActive(void)
{
  if (wrap) {
    return wrap->active;
  }
  return false;
}
///

/// DXSound::GetSDLWindowHandle
// return a handle to the current SDL window in case we have it
void *DXSound::GetSDLWindowHandle(void)
{
#if HAVE_SDL2_SDL_H && defined(HAVE_DXSOUND)
  SDL_SysWMinfo info;
  memset(&info,0,sizeof(info));
  if (SDL_GetWMInfo(&info)) {
    // Yes, SDL info is implemented. Now lets see...
    return (void *)info.window;
  }
#endif
  return NULL;
}
///
