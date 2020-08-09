/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdldigital.cpp,v 1.6 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: SDL digitial el-cheapo joystick interface
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "sdlclient.hpp"
#include "chip.hpp"
#include "vbiaction.hpp"
#include "gameport.hpp"
#include "sdldigital.hpp"
#include "monitor.hpp"
#include "exceptions.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_JOYSTICKOPEN
#include <SDL2/SDL.h>
///

/// SDLDigital::SDLDigial
SDLDigital::SDLDigital(class Machine *mach,int id)
  : Chip(mach,"SDLDigital"), VBIAction(mach), GamePort(mach,"SDLDigital",id), SDLClient(mach,SDL_INIT_JOYSTICK), 
    unit(id), enable(true), handle(NULL), 
    up(false), down(false), left(false), right(false), button(false),
    UpButton(2), DownButton(0), LeftButton(3), RightButton(1),
    TriggerAxis(0), CalibrationAxis(1), TriggerThres(16384), InvertTrigger(false)
{
}
///

/// SDLDigital::~SDLDigital
SDLDigital::~SDLDigital(void)
{
  if (handle) {
    SDL_JoystickClose(handle);
    CloseSDL();
  }
}
///

/// SDLDigital::IsAvailable
// Check whether the indicated joystick is available. Return true
// if so.
bool SDLDigital::IsAvailable(void)
{
  // If the stream is already open, then the joystick must
  // be available (simple logic)
  if (handle)
    return true;

  OpenSDL();
  // Check whether the unit number is larger than the number of available
  // joysticks. If so, then this joystick is clearly not in the system.
  if (unit >= SDL_NumJoysticks()) {
    enable = false;
  } else {
    //
    // Check whether the joystick opens.
    handle = SDL_JoystickOpen(unit);
    if (handle == NULL) {
      enable = false;
    } else {
      //
      // Check whether we have at least four buttons (for the four digital inputs)
      if (SDL_JoystickNumButtons(handle) < 4) {
	enable = false;
      }
    }
  }
  //
  // that's all for the test
  SDL_JoystickClose(handle);
  handle = NULL;
  CloseSDL();
  return enable;
}
///

/// SDLDigital::VBI
// Read off the joystick state here
void SDLDigital::VBI(class Timer *,bool,bool)
{
  WORD dx,dy;
  //
  // Check whether the joystick has been enabled and we are not
  // in paused state. No longer, the menu requires it.
  if (enable /*&& pause == false*/) {
    LONG value;
    // If the stream is not yet open, open it now.
    if (handle == NULL) {
      OpenSDL();
      handle = SDL_JoystickOpen(unit);
      if (handle == NULL) {
	enable = false;
	Throw(ObjectDoesntExist,"SDLDigital::VBI","cannot reopen the analog joystick, disabling it");
      }
    }
    //
    SDL_JoystickUpdate();
    //
    // Get the button states for the directions now.
    up      = SDL_JoystickGetButton(handle,UpButton)?(true):(false);
    down    = SDL_JoystickGetButton(handle,DownButton)?(true):(false);
    left    = SDL_JoystickGetButton(handle,LeftButton)?(true):(false);
    right   = SDL_JoystickGetButton(handle,RightButton)?(true):(false);
    //
    // And now for the button at the analog line.
    value   = SDL_JoystickGetAxis(handle,TriggerAxis) - SDL_JoystickGetAxis(handle,CalibrationAxis);
    if (InvertTrigger) {
      button = (value > TriggerThres)?(false):(true);
    } else {
      button = (value > TriggerThres)?(true):(false);
    }
    //
    // Now generate dx and dy from the data.
    if (left) {
      dx = -32767;
    } else if (right) {
      dx = +32767;
    } else {
      dx = 0;
    }
    if (up) {
      dy = -32767;
    } else if (down) {
      dy = +32767;
    } else {
      dy = 0;
    }
  } else {
    // On pausing, hold all joysticks.
    dx = 0, dy = 0;
    button = false;
  }
  //
  // Now feed all joystics on this port with the new and updated data.
  FeedAnalog(dx,dy);
  FeedButton(button);
}
///

/// SDLDigital::ColdStart
// Cleanup the joystick by shutting it down.
// This forces a re-open of the joystick devices
void SDLDigital::ColdStart(void)
{
  if (handle) {
    SDL_JoystickClose(handle);
    CloseSDL();
    handle = NULL;
  }
}
///

/// SDLDigital::WarmStart
// Warmstart the joystick, does nothing.
void SDLDigital::WarmStart(void)
{
}
///

/// SDLDigital::ParseArgs
// Parse arguments for the joystick devices
void SDLDigital::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector buttonvector[] = 
    { {"Button.1",0},
      {"Button.2",1},
      {"Button.3",2},
      {"Button.4",3},
      {NULL ,0}
    };  
  static const struct ArgParser::SelectionVector axisvector[] = 
    { {"XAxis.1" ,0},
      {"YAxis.1" ,1}, 
      {"XAxis.2" ,2},
      {"YAxis.2" ,3},
      {NULL      ,0}
    };
  char name[32];
  char upbuttonname[32];
  char downbuttonname[32];
  char leftbuttonname[32];
  char rightbuttonname[32];
  char triggeraxisname[32];
  char calibrationaxisname[32];
  char triggerthresname[32];
  char invertname[32];

  sprintf(name,"SDLDigital.%d",unit);
  sprintf(upbuttonname,"SDL_UpButton.%d",unit);
  sprintf(downbuttonname,"SDL_DownButton.%d",unit);
  sprintf(leftbuttonname,"SDL_LeftButton.%d",unit);
  sprintf(rightbuttonname,"SDL_RightButton.%d",unit);
  sprintf(triggeraxisname,"SDL_TriggerAxis.%d",unit);
  sprintf(calibrationaxisname,"SDL_CalibrationAxis.%d",unit);
  sprintf(triggerthresname,"SDL_TriggerThres.%d",unit);
  sprintf(invertname,"SDL_InvertTrigger.%d",unit);

  args->DefineTitle(name);
  args->DefineSelection(upbuttonname,
			"set the button input line for upwards movement",
			buttonvector,UpButton);
  args->DefineSelection(downbuttonname,
			"set the button input line for downwards movement",
			buttonvector,DownButton);
  args->DefineSelection(leftbuttonname,
			"set the button input line for leftwards movement",
			buttonvector,LeftButton);
  args->DefineSelection(rightbuttonname,
			"set the button input line for rightwards movement",
			buttonvector,RightButton);
  args->DefineSelection(triggeraxisname,
			"set the input axis for the trigger input",
			axisvector,TriggerAxis);
  args->DefineSelection(calibrationaxisname,
			"set the input axis for the trigger input",
			axisvector,CalibrationAxis);
  args->DefineLong(triggerthresname,"set the button press/release threshold",
		   -32768,32768,TriggerThres);
  args->DefineBool(invertname,"invert the trigger input",InvertTrigger);
}
///

/// SDLDigital::DisplayStatus
// Display the status of the joystick device
void SDLDigital::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Joystick #%d status:\n"
		   "\tJoystick available: %s\n"
		   "\tUp button line    : " LD "\n"
		   "\tDown button line  : " LD "\n"
		   "\tLeft button line  : " LD "\n"
		   "\tRight button line : " LD "\n"
		   "\tTrigger input axis: %s\n"
		   "\tCalibration axis  : %s\n"
		   "\tTrigger threshold : " LD "\n"
		   "\tInvert trigger    : %s\n",
		   unit,
		   IsAvailable()?("yes"):("no"),
		   UpButton+1,DownButton+1,LeftButton+1,RightButton+1,
		   (TriggerAxis)?("YAxis"):("XAxis"),
		   (CalibrationAxis)?("YAxis"):("XAxis"),
		   TriggerThres,
		   (InvertTrigger)?("yes"):("no")
		   );
}
///

///
#endif
