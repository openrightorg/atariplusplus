/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdlanalog.hpp,v 1.5 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: SDL analog joystick interface
 **********************************************************************************/

#ifndef SDLANALOG_HPP
#define SDLANALOG_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "sdlclient.hpp"
#include "chip.hpp"
#include "vbiaction.hpp"
#include "gameport.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_JOYSTICKOPEN
#include <SDL2/SDL.h>
#endif
///

/// Class SDLAnalog
// This class implements the interface towards SDL analog joysticks
#if HAVE_SDL2_SDL_H && HAVE_SDL_JOYSTICKOPEN
class SDLAnalog : public Chip, public VBIAction, public GamePort, public SDLClient {
  //
  // The unit number of the device. This is the last letter of
  // the joystick class; it is also the number of the joystick within SDL.
  int unit;
  //
  // Set to TRUE if this joystick works.
  bool enable;
  //
  // Pointer to the sdl handle for the joystick.
  SDL_Joystick *handle;
  //
  // Current movement and axis position.
  WORD dx,dy;
  bool button[4];
  //
  // Joystick Preferences:
  //
  // The number of the button that should trigger the joystick
  // button.
  LONG ButtonId[4];
  //
  // The axis positions for the joystick device, horizontal and
  // vertical axis
  LONG HAxis,VAxis;
  //
  //
  // Frequent activities: Poll the joystick device
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
  //
public:
  SDLAnalog(class Machine *mach,int unit);
  ~SDLAnalog(void);
  //
  // 
  // Check whether the joystick is available and if so, open it.
  bool IsAvailable(void);
  //
  // Implement the interfaces of the chip class.
  //
  // Cold and warmstart activity.
  virtual void WarmStart();
  virtual void ColdStart();
  //
  // Parse off arguments for this class.
  virtual void ParseArgs(class ArgParser *args);
  //
  // Display the status of this joystick class
  virtual void DisplayStatus(class Monitor *mon);
  //
};
#endif
///

///
#endif
