/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: timer.cpp,v 1.26 2015/10/01 16:55:14 thor Exp $
 **
 ** In this module: Support for various timing related issues
 **********************************************************************************/

/// Includes
#include "types.h"
#include "timer.hpp"
#include "string.hpp"
#include "exceptions.hpp"
#ifdef OS2
# define USE_SDL_TIMER 1
#endif
#if HAVE_SDL2_SDL_H && USE_SDL_TIMER
# include <SDL2/SDL.h>
#else
# ifndef HAS_STRUCT_TIMEVAL
# endif
# if !HAVE_GETTIMEOFDAY && ! defined(_WIN32)
#  error "system does not provide gettimeofday required by the emulator, sorry."
# endif
#endif
#include "unistd.hpp"
#include <errno.h>
///

/// Statics
#ifndef HAS_MEMBER_INIT 
const LONG Timer::UsecsPerSec = 1000000; // micro seconds in a second
#endif
///

/// Win32 includes
#if defined(_WIN32) && !defined(USE_SDL_TIMER)
#include <winsock2.h>
#define gettimeofday(a,b) w32gettimeofday((a),(b))
inline int w32gettimeofday(struct timeval *tv, void *)
{
#ifdef USE_PERFORMANCE_COUNTER
  LARGE_INTEGER ticks,tickspersec;

  if (QueryPerformanceFrequency(&tickspersec)) {
	  if (QueryPerformanceCounter(&ticks)) {
		  ULONGLONG tpc = (ULONGLONG(tickspersec.HighPart) << 32) | tickspersec.LowPart; 
		  ULONGLONG t   = (ULONGLONG(ticks.HighPart) << 32)       | ticks.LowPart; 
		  tv->tv_sec  = LONG(t / tpc);
		  t           = (t % tpc) * 1000 * 1000;
		  tv->tv_usec = LONG(t / tpc);
		  return 0;
	  }
  }
#endif
  FILETIME systime;
  GetSystemTimeAsFileTime(&systime);
  ULARGE_INTEGER timet;
  timet.LowPart  = systime.dwLowDateTime;
  timet.HighPart = systime.dwHighDateTime;
  ULONGLONG qdiv = 10 * 1000 * 1000;
  tv->tv_sec     = (LONG) ( timet.QuadPart / qdiv );
  tv->tv_usec    = (LONG) ( ( timet.QuadPart - ( tv->tv_sec * qdiv ) ) / 10 );
  tv->tv_sec    += 1240428288;
  return 0;
}
#endif
///

/// Timer::Timer
// Timer constructor
Timer::Timer(void)
{ }
///

/// Timer::~Timer
Timer::~Timer(void)
{ }
///

/// Timer::Timeval::GetTimeOfDay
void Timer::Timeval::GetTimeOfDay(void)
{
#if HAVE_SDL2_SDL_H && USE_SDL_TIMER
  Uint32 ticks;
  // try to use the SDL timer service here.
  // We don't need an absolute timestamp,
  // SDL timing is enough.
  ticks   = SDL_GetTicks(); // this is in milliseconds
  tv_sec  = ticks / 1000;
  tv_usec = 1000 * (ticks % 1000);
#else
  // We use the linux abstraction of this here.
  if (gettimeofday((struct timeval *)this,NULL)) {
    ThrowIo("Timer::GetTimeOfDay","failed to read the system time");
  }
#endif
}
///

/// Timer::Timeval::Delay
// Wait a specific time
void Timer::Timeval::Delay(void)
{
#if HAVE_SDL2_SDL_H && USE_SDL_TIMER
  Uint32 ticks;
  // compute the ticks from the usec/sec delay counter,
  // round down, i.e. rather wait not LONG enough.
  ticks = (tv_usec / 1000) + (tv_sec * 1000);
  SDL_Delay(ticks);
#elif HAVE_SELECT
  // we use usleep here since it is more precise.
  // usleep can only sleep fractions of a second, hence
  // we need to take special care if the seconds difference
  // is larger than zero. 
  // FIX: On my machine, select gives a smoother timing
  // than usleep, even though both should be equivalent.
  // FIX: To keep it running, we do not run into this if the
  // delay is smaller than the jiffies of the kernel, typically
  // 10ms.
  if (tv_sec || tv_usec >= 10000)
    select(0,NULL,NULL,NULL,(struct timeval *)this);
#endif
}
///

/// Timer::Timeval::WaitForOutput
// Wait for a specific timeout for the indicated
// output file to complete
// Return 0 on timeout, or non-zero if the file
// I/O returned
int Timer::Timeval::WaitForOutput(AsyncFileHandle file)
{
#if HAVE_SDL2_SDL_H && USE_SDL_TIMER
  return file;
#elif HAVE_SELECT
  fd_set fileset;
  //
  // Clear the file set and add the file to it
  FD_ZERO(&fileset);
  FD_SET(file,&fileset);
  // Now use select to wait either for the event or for the IO
  return select(file+1,NULL,&fileset,NULL,(struct timeval *)this);
#else
  return file;
#endif
}
///

/// Timer::StartTimer
// Run a timing event once a while: Initialize the timer for this mode.
// The period is given in a seconds/microseconds pair. Returns true in
// case of success.
void Timer::StartTimer(long secs,long usecs)
{
  Increment.tv_sec  = secs;
  Increment.tv_usec = usecs;
  // Normalize the requested time interval such that the usecs are
  // in range.
  NormalizeTimeVal(&Increment);
  //
  Reference.GetTimeOfDay();
  // Add to the target waiting time.
  AddToTime(&Reference,&Increment);
}
///

/// Timer::EventIsOver
// Check whether the periodic waiting event has already passed over.
// Returns true if so.
bool Timer::EventIsOver(void)
{
  struct Timeval now;
  //
  now.GetTimeOfDay();
  return IsLater(&now,&Reference);
}
///

/// Timer::WaitForEvent
// Wait until the specified period is over or return immediately
// if the event is over already.
void Timer::WaitForEvent(void)
{
  struct Timeval now;
  //
  for(;;) {
    now.GetTimeOfDay();
    if (IsLater(&now,&Reference))
      break;
    //
    // Get the amount of time we have to wait to make the event
    // happen.
    SubtractFromTime(&now,&Reference);
    now.Delay();
  }
}
///

/// Timer::WaitForIO
// Wait until either the timer event happens or the async IO returns
// This call returns true if the IO happens earlier than the event.
bool Timer::WaitForIO(AsyncFileHandle file)
{
#if HAVE_SELECT
  struct Timeval now;
  int event;
  //
  //
  now.GetTimeOfDay();
  // If the current time is already later than what we wait for, then
  // bail out and return false, indicated that the IO has not been checked.
  if (IsLater(&now,&Reference))
    return false;
  // Compute the difference
  SubtractFromTime(&now,&Reference);
  // Now use select to wait either for the event or for the IO
  event = now.WaitForOutput(file);
  // Now check whether this is due to time-out or due to the IO
  // that became ready
  if (event == 0) {
    // The time out applied. Return this condition.
    return false;
  } else if (event > 0) {
    // The IO returned.
    return true;
  } else {
    // Do not treat a ^C as an error. Rather, accept it as an IO-return
    if (errno == EINTR)
      return true;
    //
    // an error condition happened.
    ThrowIo("Timer::WaitForIO","select() failed");
  }
#else
  // Otherwise, if there is no select, ignore the IO done. This means that audio will be screwed, but
  // such is life.
  WaitForEvent();
  return false;
#endif
}
///

/// Timer::CheckIO
// Check whether the IO handle returned already. Do not wait for
// any time. This is similar to WaitForIO except that we do not
// wait at all.
bool Timer::CheckIO(AsyncFileHandle file)
{
#if HAVE_SELECT
  struct Timeval now;
  int event;
  //
  // Initialize the waiting time to zero.
  now.tv_sec  = 0;
  now.tv_usec = 0;
  //
  // Now use select to wait either for the event or for the IO
  event = now.WaitForOutput(file);
  // Now check whether this is due to time-out or due to the IO
  // that became ready
  if (event == 0) {
    // The time out applied. Return this condition: Hence,
    // the IO did not return
    return false;
  } else if (event > 0) {
    // The IO returned.
    return true;
  } else {
    // Do not treat a ^C as an error. Rather, accept it.
    if (errno == EINTR)
      return true;
    // an error condition happened. 
    ThrowIo("Timer::CheckIO","select() failed");
    return false;
  }
#else
  // signal the IO as "done". This is not quite true, but we cannot really make all this working
  // without a working "select()" call.
  return true;
#endif
}
///

/// Timer::TriggerNextEvent
// TriggerNextEvent: trigger/advance the timer for the next event
void Timer::TriggerNextEvent(void) 
{
  AddToTime(&Reference,&Increment);
}
///

/// Timer::GetMicroDelay
// Convert the required delay into milliseconds (approximately)
// This is a pretty rough approximation as it is only 1/1000th of
// the possible resolution... *sigh*
long Timer::GetMicroDelay(void)
{
  struct Timeval now;
  //
  now.GetTimeOfDay();
  if (IsLater(&now,&Reference))
    return 0;
  //
  // Now get the amount of time we have to wait to make
  // the event happen.
  SubtractFromTime(&now,&Reference);
  return (now.tv_sec * 1000 + now.tv_usec / 1000);
}
///
