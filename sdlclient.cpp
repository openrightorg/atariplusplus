/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdlclient.cpp,v 1.6 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: A class using SDL must be derived from this
 ** class to register and get SDL support from various SDL subsystems
 **********************************************************************************/

/// Includes
#include "types.h"
#include "sdlclient.hpp"
#include "sdlport.hpp"
#include "machine.hpp"
#include "list.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_INITSUBSYSTEM
///

/// SDLClient::SDLClient
// Build a new SDL client from a subsystem mask.
SDLClient::SDLClient(class Machine *mach,Uint32 subsystem)
  : SubSystemMask(subsystem), IsInit(false)
{
  // For that, register us.
  Port = mach->SDLPort();
  Port->RegisterClient(this,subsystem);
}
///

/// SDLClient::~SDLClient
// Done with the client, remove us
SDLClient::~SDLClient(void)
{
  CloseSDL();
  Remove();
}
///

/// SDLClient::OpenSDL
// Open the SDL system here: This forwards the
// request to the SDL port to open SDL globally
// and then initialized the requested subsystems.
void SDLClient::OpenSDL(void)
{
  if (IsInit == false) {
    Port->OpenSDL(this);
    Port->InitSubSystem(SubSystemMask);
    IsInit = true;
  }
}
///

/// SDLClient::CloseSDL
// Close the SDL subsystem this client requires.
void SDLClient::CloseSDL(void)
{
  if (IsInit) {    
    IsInit = false;
    Port->QuitSubSystem(SubSystemMask);
  }
}
///

///
#endif


