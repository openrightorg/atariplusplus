/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdlclient.hpp,v 1.7 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: A class using SDL must be derived from this
 ** class to register and get SDL support from various SDL subsystems
 **********************************************************************************/

#ifndef SDL_CLIENT_HPP
#define SDL_CLIENT_HPP

/// Includes
#include "types.h"
#include "list.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_INITSUBSYSTEM
#include <SDL2/SDL.h>
#endif
///

/// Forwards
class Machine;
class SDLPort;
///

/// Class SDLClient
// Any class making use of SDL must be derived fromt this base class.
// This base keeps care of opening SDL with all the proper parameters
// and the subsystem flags setup correctly.
#if HAVE_SDL2_SDL_H && HAVE_SDL_INITSUBSYSTEM
class SDLClient : public Node<class SDLClient> {
  //
  // Not much data here. This is really just an administration interface class
  // that provides some SDL goodies.
  class SDL_Port *Port;
  //
  // The subsystem mask we need for this client
  Uint32 SubSystemMask;
  //
  // A boolean that indicates whether this subsystem has been initialized or not
  bool   IsInit;
  //
protected:
  // We cannot destroy the SDLClient directly. Instead, we must go thru
  // The base class for that.
  ~SDLClient(void);
public:
  // Build an SDL interface: We require the mask of all
  // SDL subsystems we need to allocate in here.
  SDLClient(class Machine *mach,Uint32 subsystem);
  //
  // Open all the SDL subsystems we need. This does not
  // yet *create* the corresponding SDL front-ends, though.
  void OpenSDL(void);
  //
  // Close the SDL subsystem this client requires.
  void CloseSDL(void);
  //
  // Return the mask of active subsystems.
  Uint32 ActiveMask(void) const
  {
    return (IsInit)?(SubSystemMask):(0);
  }
};
#endif
///

///
#endif


