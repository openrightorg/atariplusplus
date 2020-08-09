/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdlanalog.cpp,v 1.10 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: SDL analog joystick interface
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "sdlclient.hpp"
#include "chip.hpp"
#include "vbiaction.hpp"
#include "gameport.hpp"
#include "sdlanalog.hpp"
#include "monitor.hpp"
#include "exceptions.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_JOYSTICKOPEN
#include <SDL2/SDL.h>
///

/// SDLAnalog::SDLAnalog
SDLAnalog::SDLAnalog(class Machine *mach,int id)
  : Chip(mach,"SDLAnalog"), VBIAction(mach), GamePort(mach,"SDLAnalog",id), SDLClient(mach,SDL_INIT_JOYSTICK), 
    unit(id), enable(true), handle(NULL), dx(0), dy(0),
    HAxis(0), VAxis(1)
{
  int i;
  for(i = 0;i < 4;i++) {
    button[i]   = false;
    ButtonId[i] = i;
  }
}
///

/// SDLAnalog::~SDLAnalog
SDLAnalog::~SDLAnalog(void)
{
  if (handle) {
    SDL_JoystickClose(handle);
    CloseSDL();
  }
}
///

/// SDLAnalog::IsAvailable
// Check whether the indicated joystick is available. Return true
// if so.
bool SDLAnalog::IsAvailable(void)
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

/// SDLAnalog::VBI
// Read off the joystick state here
void SDLAnalog::VBI(class Timer *,bool,bool)
{
  int i;
  //
  // Check whether the joystick has been enabled and we are not
  // in paused state. No longer the menu requires it.
  if (enable /*&& pause == false*/) {
    // If the stream is not yet open, open it now.
    if (handle == NULL) {
      OpenSDL();
      handle = SDL_JoystickOpen(unit);
      if (handle == NULL) {
	enable = false;
	Throw(ObjectDoesntExist,"SDLAnalog::VBI","cannot reopen the analog joystick, disabling it");
      }
    }
    //
    // Check for the state of the joystick buttons.
    SDL_JoystickUpdate();
    //
    for(i = 0;i < 4;i++) {
      button[i] = SDL_JoystickGetButton(handle,ButtonId[i])?(true):(false);
    }
    //
    // And now for the joystick axis
    dx      = SDL_JoystickGetAxis(handle,HAxis);
    dy      = SDL_JoystickGetAxis(handle,VAxis);
  } else {
    // On pausing, hold all joysticks.
    dx = 0, dy = 0;
    for(i = 0;i < 4;i++) {
      button[i] = false;
    }
  }
  //
  // Now feed all joystics on this port with the new and updated data.
  FeedAnalog(dx,dy);
  for(i = 0;i < 4;i++) {
    FeedButton(button[i],i);
  }
}
///

/// SDLAnalog::ColdStart
// Cleanup the joystick by shutting it down.
// This forces a re-open of the joystick devices
void SDLAnalog::ColdStart(void)
{
  if (handle) {
    SDL_JoystickClose(handle);
    CloseSDL();
    handle = NULL;
  }
}
///

/// SDLAnalog::WarmStart
// Warmstart the joystick, does nothing.
void SDLAnalog::WarmStart(void)
{
}
///

/// SDLAnalog::ParseArgs
// Parse arguments for the joystick devices
void SDLAnalog::ParseArgs(class ArgParser *args)
{
  static const struct ArgParser::SelectionVector axisvector[] = 
    { {"XAxis.1" ,0},
      {"YAxis.1" ,1},
      {"XAxis.2" ,2},
      {"YAxis.2" ,3},
      {NULL      ,0}
    };
  char name[32];
  char buttonname1[32];
  char buttonname2[32];
  char buttonname3[32];
  char buttonname4[32];
  char haxisname[32];
  char vaxisname[32];
  LONG button[4];
  int i;
  
  sprintf(name,"SDLAnalog.%d",unit);
  sprintf(buttonname1,"SDL_First_Button.%d",unit);
  sprintf(buttonname2,"SDL_Second_Button.%d",unit);
  sprintf(buttonname3,"SDL_Third_Button.%d",unit);
  sprintf(buttonname4,"SDL_Forth_Button.%d",unit);
  sprintf(haxisname,"SDL_HAxis.%d",unit);
  sprintf(vaxisname,"SDL_VAxis.%d",unit);

  for(i = 0;i < 4;i++)
    button[i] = ButtonId[i] + 1;

  args->DefineTitle(name);
  args->DefineLong(buttonname1,"set the first joystick input button",1,16,button[0]);
  args->DefineLong(buttonname2,"set the second joystick input button",1,16,button[1]);
  args->DefineLong(buttonname3,"set the third joystick input button",1,16,button[2]);
  args->DefineLong(buttonname4,"set the fourth joystick input button",1,16,button[3]);
  args->DefineSelection(haxisname,"set the horizontal joystick axis",
			axisvector,HAxis);
  args->DefineSelection(vaxisname,"set the vertical joystick axis",
			axisvector,VAxis);

  for(i = 0;i < 4;i++)
    ButtonId[i] = button[i] - 1;
}
///

/// SDLAnalog::DisplayStatus
// Display the status of the joystick device
void SDLAnalog::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("SDL Joystick #%d status:\n"
		   "\tJoystick available     : %s\n"
		   "\tFirst Polled button  # : " LD "\n"
		   "\tSecond Polled button # : " LD "\n"
		   "\tThird Polled button  # : " LD "\n"
		   "\tFourth Polled button # : " LD "\n"
		   "\tHorizontal Axis      # : " LD "\n"
		   "\tVertical Axis        # : " LD "\n",
		   unit,
		   IsAvailable()?("yes"):("no"),
		   ButtonId[0],ButtonId[1],ButtonId[2],ButtonId[3],
		   HAxis,VAxis);
}
///

///
#endif
