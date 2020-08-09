/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdlport.hpp,v 1.6 2015/05/21 18:52:42 thor Exp $
 **
 ** In this mdoule: The hosting port for all SDL type front-ends
 ** This keeps care about using the various SDL features
 **********************************************************************************/

#ifndef SDL_PORT_HPP
#define SDL_PORT_HPP

/// Includes
#include "types.h"
#include "list.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_INIT
#include <SDL2/SDL.h>
#endif
///

/// Forwards
class SDLClient; // a generic SDL client
///

/// Class SDLPort
#if HAVE_SDL2_SDL_H && HAVE_SDL_INIT
// This class registers SDL front-ends, and, as a service opens the SDL
// library with the proper parameters, specifically SDL_Init
class SDL_Port {
  // This boolean gets set as soon as SDL is up.
  bool                        Initialized;
  //
  // The list of registered SDL clients.
  List<SDLClient>             Clients;
  //
public:
  SDL_Port(void);
  ~SDL_Port(void);
  //
  // Register an SDL client within this port. This also requires a client-mask, namely
  // the SDL subsystems the client wants to make use of.
  void RegisterClient(class SDLClient *client,Uint32 subsystemmask);
  //
  // Open the SDL library (for the client). Throw in case of no success.
  void OpenSDL(class SDLClient *client);
  //
  //
  // Init a subsystem given its mask. If the subsystem is already initialized by another
  // client, do not init it again.
  void InitSubSystem(Uint32 subsystemmask);
  // Quit a subsystem. Do not quit it if some other client still uses the same subsystem.
  void QuitSubSystem(Uint32 subsystemmask);
};
#endif
///

///
#endif
