/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdldigital.hpp,v 1.5 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: SDL digitial el-cheapo joystick interface
 **********************************************************************************/

#ifndef SDLDIGITAL_HPP
#define SDLDIGITAL_HPP

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

/// Class SDLDigital
// This class implements the interface towards SDL digital joysticks
#if HAVE_SDL2_SDL_H && HAVE_SDL_JOYSTICKOPEN
class SDLDigital : public Chip, public VBIAction, public GamePort, public SDLClient {
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
  bool up,down,left,right;
  bool button;
  //
  // Joystick Preferences:
  //  
  // Various button numbers to emulate the directions.
  LONG UpButton,DownButton,LeftButton,RightButton;
  //
  // The axis that is responsible for the trigger input
  LONG TriggerAxis;
  //
  // The axis that is used for calibration.
  LONG CalibrationAxis;
  //
  // The threshold above which the trigger is read as pressed
  LONG TriggerThres;
  //
  // If this boolean is set, then the trigger is read in negative
  // logic.
  bool InvertTrigger;
  //
  // Frequent activities: Poll the joystick device
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
  //
public:
  SDLDigital(class Machine *mach,int unit);
  ~SDLDigital(void);
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
