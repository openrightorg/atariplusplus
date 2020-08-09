/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdlport.cpp,v 1.8 2015/05/21 18:52:42 thor Exp $
 **
 ** In this mdoule: The hosting port for all SDL type front-ends
 ** This keeps care about using the various SDL features
 **********************************************************************************/

/// Includes
#include "types.h"
#include "list.hpp"
#include "exceptions.hpp"
#include "sdlport.hpp"
#include "sdlclient.hpp"
#include "sighandler.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_INIT
///

/// SDL_Port::SDL_Port
// Initialize the SDL_Port with defaults.
SDL_Port::SDL_Port(void)
  : Initialized(false)
{ }
///

/// SDL_Port::~SDL_Port
// Cleanup the SDL_Library. Do not dispose clients
// neither unlink them. This is up to the clients itself.
SDL_Port::~SDL_Port(void)
{
  if (Initialized)
    SDL_Quit();
}
///

/// SDL_Port::RegisterClient
// Register a client for SDL and remember its subsystem mask, i.e.
// what it requires from SDL.
void SDL_Port::RegisterClient(class SDLClient *client,Uint32)
{
  Clients.AddTail(client);
}
///

/// SDL_Port::OpenSDL
// Open the SDL system with all the registered clients.
void SDL_Port::OpenSDL(class SDLClient *)
{
  if (!Initialized) {
    // Ok, we need to initialize now. Try the best!
    if (SDL_Init(0) < 0) {
      Throw(ObjectDoesntExist,"SDL_Port::OpenSDL",
	    "Failed to initialize SDL");
    }
    Initialized = true;
    SigHandler::RestoreCoreDump();
  }
}
///

/// SDL_Port::InitSubSystem
// Init a subsystem given its mask. If the subsystem is already initialized by another
// client, do not init it again.
void SDL_Port::InitSubSystem(Uint32 subsystemmask)
{
  class SDLClient *client;
  //
  // Iterate thru all known clients and remove the already active components from the
  // activation mask.
  client = Clients.First();
  while(client) {
    subsystemmask &= ~(client->ActiveMask());
    client = client->NextOf();
  }
  //
  // If something is still left, then activate the required subsystems now.
  if (subsystemmask) {
    SDL_InitSubSystem(subsystemmask);
  }
#ifdef USE_SIGNAL
  RestoreCoreDump();
#endif
}
///

/// SDL_Port::QuitSubSystem
// Quit a subsystem. Do not quit it if some other client still uses the same subsystem.
void SDL_Port::QuitSubSystem(Uint32 subsystemmask)
{
  class SDLClient *client;
  //
  // Iterate thru the list of known clients and remove all those subsystems from
  // quitting that are still in use by some clients.
  client = Clients.First();
  while(client) {
    subsystemmask &= ~(client->ActiveMask());
    client = client->NextOf();
  }
  //
  // If any no longer required subsystems remain, close them now.
  if (subsystemmask) {
    SDL_QuitSubSystem(subsystemmask);
  }
#ifdef USE_SIGNAL
  RestoreCoreDump();
#endif
}
///

///
#endif
