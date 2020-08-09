/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdl_frontend.cpp,v 1.94 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: A frontend using the sdl library
 **
 ** NOTE: Set the shell variable SDL_VIDEODRIVER to svgalib for svga access
 **********************************************************************************/

/// Includes
#include "types.h"
#include "sdl_frontend.hpp"
#include "sdlclient.hpp"
#include "antic.hpp"
#include "gtia.hpp"
#include "monitor.hpp"
#include "timer.hpp"
#include "keyboard.hpp"
#include "screendump.hpp"
#include "string.hpp"
#include "new.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_CREATEWINDOW
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_surface.h>
///

/// SDL_FrontEnd::DiagDeblocker::statics
// static members for the deblocker classes
template<class datatype,LONG PxWidth,LONG PxHeight>
const signed char SDL_FrontEnd::DiagDeblocker<datatype,PxWidth,PxHeight>::northwest[16] = 
  { 0,-1, 0, 0, 0, 0, 0, 0, 0,-1, 0, 0, 0, 0, 0, 0 };

template<class datatype,LONG PxWidth,LONG PxHeight>
const signed char SDL_FrontEnd::DiagDeblocker<datatype,PxWidth,PxHeight>::northeast[16] = 
  { 0, 0,+1, 0, 0, 0,+1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

template<class datatype,LONG PxWidth,LONG PxHeight>
const signed char SDL_FrontEnd::DiagDeblocker<datatype,PxWidth,PxHeight>::southwest[16] = 
  { 0, 0, 0, 0,-1, 0,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

template<class datatype,LONG PxWidth,LONG PxHeight>
const signed char SDL_FrontEnd::DiagDeblocker<datatype,PxWidth,PxHeight>::southeast[16] =
  { 0, 0, 0, 0, 0, 0, 0, 0,+1,+1, 0, 0, 0, 0, 0, 0 };
///

/// SDL_FrontEnd::SDL_FrontEnd
SDL_FrontEnd::SDL_FrontEnd(class Machine *mach,int unit)
  : AtariDisplay(mach,unit), SDLClient(mach,SDL_INIT_VIDEO), // this requires SDL video output
    screen(NULL), sdl_initialized(false),
    colors(NULL), colormap(NULL),
    ScreenBaseName(new char[strlen("ScreenDump")+1]), Format(ScreenDump::PNM),
    truecolor(false), dumpcnt(1), dump(false), keyboardfocus(false),
    ActiveFrame(NULL), AlternateFrame(NULL), Row(NULL),
    InputBuffer(new UBYTE[Antic::DisplayModulo]), ModifiedLines(new UBYTE[Antic::PALTotal]),
    ActiveRGBFrame(NULL), AlternateRGBFrame(NULL), RGBRow(NULL),
    RGBInputBuffer(new PackedRGB[Antic::DisplayModulo]),
    fullrefresh(true), alternatefullrefresh(true),
    usedbuf(true), showpointer(false), quickvbi(false), capsup(0), ScrolledLines(0),
    KeypadStick(NULL), MouseStick(mach,"MouseStick",false), RelMouseStick(mach,"RelMouseStick",true),
    LeftEdge(unit?0:16), TopEdge(0), 
    Width(unit?80*8:Antic::WindowWidth), Height(unit?25*8:Antic::WindowHeight),
    PixelWidth(unit?1:2), PixelHeight(2), ShieldCursor(false), 
    FullScreen(false), DoubleBuffer(false), Deblocking(false),
    Doubler(new UWORD[256]), Quadrupler(new ULONG[256]), Deblocker(NULL)
{
  int i;
  if (Unit == 0) {
    strcpy(ScreenBaseName,"ScreenDump");
  } else {
    strcpy(ScreenBaseName,"XEPDump");
  }
  //
  // Setup the doubler and quadrupler arrays.
  for(i=0;i<256;i++) {
    UWORD dbl     = UWORD((i   << 8 ) | i);
    Doubler[i]    = dbl;
    Quadrupler[i] = (dbl << 16) | dbl;
  }  
  // Set the flag array keeping the lines that have been changed; this indicates
  // that we have to perform a full refresh anyhow.
  memset(ModifiedLines,1,Antic::PALTotal); 

  MouseX          = 0;
  MouseY          = 0;
  MouseButton     = false;
  MouseSpeedLimit = 1;
  fullscreen      = false;
#if defined _WIN32
  // Actually, SDL should do this itself, but doesn't
  // on win.
  ShieldCursor    = true;
  FullScreen      = false;
#endif
}
///

/// SDL_FrontEnd::~SDL_FrontEnd
SDL_FrontEnd::~SDL_FrontEnd(void)
{   
  delete[] RGBInputBuffer;
  delete[] AlternateRGBFrame;
  delete[] ActiveRGBFrame;
  delete[] InputBuffer;
  delete[] ModifiedLines;
  delete[] ActiveFrame;
  delete[] AlternateFrame;
  delete[] colors;
  delete[] ScreenBaseName;
  delete[] Doubler;
  delete[] Quadrupler;
  delete Deblocker;
}
///

/// SDL_FrontEnd::CreateDisplay
// Initialize and setup the display towards SDL
void SDL_FrontEnd::CreateDisplay(void)
{
  const struct ColorEntry *cmap;
  SDL_Color *cp;
  int i,x,y,buttons;
  UBYTE appstatus;
  //
  // Not yet. If we are here, we must rebuild the
  // SDL config.
  sdl_initialized = false;
  // Get the keyboard from the machine.
  keyboard    = machine->Keyboard();
  KeypadStick = machine->KeypadStick();
  //
  // Associate the keyboard keys.
  KeypadStick = machine->KeypadStick();
  //
  // Associate the keyboard codes.
  KeypadStick->AssociateKey(KeyboardStick::ArrowLeft    ,SDLK_LEFT);
  KeypadStick->AssociateKey(KeyboardStick::ArrowRight   ,SDLK_RIGHT);
  KeypadStick->AssociateKey(KeyboardStick::ArrowUp      ,SDLK_UP);
  KeypadStick->AssociateKey(KeyboardStick::ArrowDown    ,SDLK_DOWN);
  KeypadStick->AssociateKey(KeyboardStick::Return       ,SDLK_RETURN);
  KeypadStick->AssociateKey(KeyboardStick::Tab          ,SDLK_TAB);
  KeypadStick->AssociateKey(KeyboardStick::Backspace    ,SDLK_BACKSPACE);
  KeypadStick->AssociateKey(KeyboardStick::KP_0         ,SDLK_KP_0);
  KeypadStick->AssociateKey(KeyboardStick::KP_1         ,SDLK_KP_1);
  KeypadStick->AssociateKey(KeyboardStick::KP_2         ,SDLK_KP_2);
  KeypadStick->AssociateKey(KeyboardStick::KP_3         ,SDLK_KP_3);
  KeypadStick->AssociateKey(KeyboardStick::KP_4         ,SDLK_KP_4);
  KeypadStick->AssociateKey(KeyboardStick::KP_5         ,SDLK_KP_5);
  KeypadStick->AssociateKey(KeyboardStick::KP_6         ,SDLK_KP_6);
  KeypadStick->AssociateKey(KeyboardStick::KP_7         ,SDLK_KP_7);
  KeypadStick->AssociateKey(KeyboardStick::KP_8         ,SDLK_KP_8);
  KeypadStick->AssociateKey(KeyboardStick::KP_9         ,SDLK_KP_9);
  KeypadStick->AssociateKey(KeyboardStick::KP_Divide    ,SDLK_KP_DIVIDE);
  KeypadStick->AssociateKey(KeyboardStick::KP_Times     ,SDLK_KP_MULTIPLY);
  KeypadStick->AssociateKey(KeyboardStick::KP_Minus     ,SDLK_KP_MINUS);
  KeypadStick->AssociateKey(KeyboardStick::KP_Plus      ,SDLK_KP_PLUS);
  KeypadStick->AssociateKey(KeyboardStick::KP_Enter     ,SDLK_KP_ENTER);
  KeypadStick->AssociateKey(KeyboardStick::KP_Digit     ,SDLK_KP_PERIOD);
  KeypadStick->AssociateKey(KeyboardStick::SP_Insert    ,SDLK_INSERT);
  KeypadStick->AssociateKey(KeyboardStick::SP_Delete    ,SDLK_DELETE);
  KeypadStick->AssociateKey(KeyboardStick::SP_Home      ,SDLK_HOME);
  KeypadStick->AssociateKey(KeyboardStick::SP_End       ,SDLK_END);
  KeypadStick->AssociateKey(KeyboardStick::SP_ScrollUp  ,SDLK_PAGEUP);
  KeypadStick->AssociateKey(KeyboardStick::SP_ScrollDown,SDLK_PAGEDOWN);
  //
  // Open the SDL frontend now in case we don't have it yet.
  OpenSDL();
  //
  // Must now shut down SDL on the destructor
  //sdl_initialized = true;
  truecolor       = machine->GTIA()->SuggestTrueColor();

  SDL_Init(SDL_INIT_VIDEO);

  window = SDL_CreateWindow("Atari++",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          Width * PixelWidth,Height * PixelHeight,
			  ((FullScreen)?SDL_WINDOW_FULLSCREEN:0));

  if(truecolor) {
    screen = SDL_CreateRGBSurface(SDL_SWSURFACE, Width * PixelWidth,Height * PixelHeight, 32, 0, 0, 0, 0);
  } else {
    screen = SDL_CreateRGBSurface(SDL_SWSURFACE, Width * PixelWidth,Height * PixelHeight, 8, 0, 0, 0, 0);
  }

  if (screen == NULL) {
      //sdl_initialized = false;
    Throw(ObjectDoesntExist,"SDL_FrontEnd::CreateDisplay",
	  "Failed to setup the SDL display.");
  }
  //      
  colormap = cmap = machine->GTIA()->ActiveColorMap();
  //
  if (truecolor) {
    // No colormap in true color mode
    delete[] colors;
    colors = NULL;
  } else {
    // Now create the palette and fetch it from the GTIA
    if (colors == NULL) {
      colors = new SDL_Color[256];
    }
    //
    cp       = colors;
    for(i=0;i<256;i++) {
      cp->r = cmap->red;
      cp->g = cmap->green;
      cp->b = cmap->blue;
      cp->a = 255;
      cp++;
      cmap++;
    }
    if(SDL_SetPaletteColors(screen->format->palette, colors, 0, 256) != 0) {
      Throw(ObjectDoesntExist,"SDL_FrontEnd::CreateDisplay",
	    "Failed to setup the color palette for SDL");
    }
  }
  //
  // Enforce a full display refresh now
  fullrefresh          = true;
  alternatefullrefresh = true;
  //  
  // Set or disable the pointer now from the cached boolean marker.
  SDL_ShowCursor((showpointer)?(SDL_ENABLE):(SDL_DISABLE)); 
  //
  // Build or delete the deblocking filter.
  // We currently only provide all sizes from 2x2 to 3x3,
  // more dimensions to come...
  delete Deblocker;
  Deblocker = NULL;
  if (Deblocking) {
    if (truecolor) {
      if (PixelWidth == 2 && PixelHeight == 2) {
	Deblocker = new DiagDeblocker<PackedRGB,2,2>;
      } else if (PixelWidth == 2 && PixelHeight == 3) {
	Deblocker = new DiagDeblocker<PackedRGB,2,3>;
      } else if (PixelWidth == 3 && PixelHeight == 2) {
	Deblocker = new DiagDeblocker<PackedRGB,3,2>;
      } else if (PixelWidth == 3 && PixelHeight == 3) {
	Deblocker = new DiagDeblocker<PackedRGB,3,3>;
      }
    } else {      
      if (PixelWidth == 2 && PixelHeight == 2) {
	Deblocker = new DiagDeblocker<UBYTE,2,2>;
      } else if (PixelWidth == 2 && PixelHeight == 3) {
	Deblocker = new DiagDeblocker<UBYTE,2,3>;
      } else if (PixelWidth == 3 && PixelHeight == 2) {
	Deblocker = new DiagDeblocker<UBYTE,3,2>;
      } else if (PixelWidth == 3 && PixelHeight == 3) {
	Deblocker = new DiagDeblocker<UBYTE,3,3>;
      }
    }
  }
  //
  appstatus = SDL_GetWindowFlags(window);
  
  keyboardfocus = (appstatus & SDL_WINDOW_INPUT_FOCUS)?true:false;

  buttons     = SDL_GetMouseState(&x,&y);
  MouseX      = x / PixelWidth;
  MouseY      = y / PixelHeight;
  MouseButton = buttons?true:false;
  //
  // SDL is now ready for use.
  sdl_initialized = true;
}
///

/// SDL_FrontEnd::ActiveBuffer
// Return the next active buffer and swap buffers on double-buffered
// screens. 
UBYTE *SDL_FrontEnd::ActiveBuffer(void)
{
  //
  // Check whether SDL is already initialized. If not, do now.
  // Do not enforce a cold reset unless required.
  if (!sdl_initialized) {
    CreateDisplay();
  }
  //
  // Check whether we have an active buffer. If not, we have to build it now.
  if (ActiveFrame == NULL || (ActiveRGBFrame == NULL && truecolor)) {
    UWORD w,h;
    ULONG dim;
    //
    // Get the dimensions required for the buffer from antic
    machine->Antic()->DisplayDimensions(w,h);
    //
    // Compute the dimension of the new buffer.
    // Allow a 1 pixel frame around the data since the
    // deblocking filter requires this.
    dim = (w + 2) * (h + 2);
    //
    // and build it.
    if (ActiveFrame == NULL) {
      ActiveFrame = new UBYTE[dim];
      // to shut up valgrind: erase me.
      memset(ActiveFrame,0,dim);
    }
    if (ActiveRGBFrame == NULL && truecolor) {
      ActiveRGBFrame = new PackedRGB[dim];
      memset(ActiveRGBFrame,0,dim*sizeof(PackedRGB));
    }
    //
    // Keep the modulo to ensure the refresh is right.
    FrameModulo          = w+2;
    fullrefresh          = true;
    alternatefullrefresh = true;
  }
  
  // Remember to return the address with an offset that
  // has been added for the deblocking filter.
  return ActiveFrame + 1 + FrameModulo;
}
///

/// SDL_FrontEnd::PushLine
// Push the output buffer back into the display process. This signals that the
// currently generated display line is ready.
void SDL_FrontEnd::PushLine(UBYTE *buffer,int size)
{
  if (CurrentLine >= TopEdge && CurrentLine < TopEdge + Height) {
    // The line is visible. Compare whether it is identical to the row in the last buffer.
    if (memcmp(buffer,Row,size)) {
      // Keep the data and the result for the final refresh.
      memcpy(Row,buffer,size);
      ModifiedLines[CurrentLine] = 1;
      //
      // If deblocking is on, we must also rebuild the line on top and below
      // since they interact with this line.
      if (Deblocking) {
	if (CurrentLine > TopEdge)
	  ModifiedLines[CurrentLine-1] = 1;
	if (CurrentLine + 1 < TopEdge + Height)
	  ModifiedLines[CurrentLine+1] = 1;
      }
    }
  }
  CurrentLine++;
  RGBRow += FrameModulo;
  Row    += FrameModulo;
}
///

/// SDL_FrontEnd::PushRGBLine
// Push the output buffer back into the display process. This signals that the
// currently generated display line is ready.
void SDL_FrontEnd::PushRGBLine(PackedRGB *buffer,int size)
{
  if (CurrentLine >= TopEdge && CurrentLine < TopEdge + Height) {
    // The line is visible. Compare whether it is identical to the row in the last buffer.
    if (memcmp(buffer,RGBRow,size * sizeof(PackedRGB))) {
      // Keep the data and the result for the final refresh.
      memcpy(RGBRow,buffer,size * sizeof(PackedRGB));
      // Must do this for consistency.
      memcpy(Row,InputBuffer,size);
      ModifiedLines[CurrentLine] = 1; 
      //
      // If deblocking is on, we must also rebuild the line on top and below
      // since they interact with this line.
      if (Deblocking) {
	if (CurrentLine > TopEdge)
	  ModifiedLines[CurrentLine-1] = 1;
	if (CurrentLine + 1 < TopEdge + Height)
	  ModifiedLines[CurrentLine+1] = 1;
      }
    }
  }
  CurrentLine++;
  RGBRow += FrameModulo;
  Row    += FrameModulo;
}
///

/// SDL_FrontEnd::SignalRect
// Signal the requirement for refresh in the indicated region. This is only
// used by the GUI engine as the emulator core uses PushLine for that.
void SDL_FrontEnd::SignalRect(LONG leftedge,LONG topedge,LONG width,LONG height)
{
  if (topedge < TopEdge) {
    LONG delta = TopEdge - topedge;
    topedge   -= delta;
    height    -= delta;
  }
  if (topedge + height > TopEdge + Height) {
    LONG delta = topedge + height - (TopEdge + Height);
    height    -= delta;
  }
  if (height > 0 && topedge >= 0) {
    memset(ModifiedLines + topedge, 1, height);
  }
  if (truecolor && colormap) {
    // Translate the indicated region into truecolor since we
    // are a truecolor buffer. Yikes!
    if (leftedge < LeftEdge) {
      LONG delta = LeftEdge - leftedge;
      leftedge  -= delta;
      width     -= delta;
    }
    if (leftedge + width > LeftEdge + Width) {
      LONG delta = leftedge + width - (LeftEdge + Width);
      width     -= delta;
    }
    if (height > 0 && width > 0 && leftedge >= 0 && topedge >= 0) {
      LONG mod = FrameModulo - width;
      UBYTE     *src = &ActiveFrame[leftedge + 1 + FrameModulo * (topedge + 1)];
      PackedRGB *dst = &ActiveRGBFrame[leftedge + 1 + FrameModulo * (topedge + 1)];
      do {
	LONG c = width;
	do {
	  *dst = colormap[*src].XPackColor();
	  dst++,src++;
	} while(--c);
	dst   += mod;
	src   += mod;
      } while(--height);
    }
  }
}
///

/// SDL_FrontEnd::EnableDoubleBuffer
// Disable or enable double buffering temporarely for the user
// menu. Default is to have it enabled.
void SDL_FrontEnd::EnableDoubleBuffer(bool enable)
{
  if (DoubleBuffer && screen && sdl_initialized && enable != usedbuf) {
    LONG x,y;
    bool button;
    // Fetch the current mouse position. SDL seems to like to overwrite
    // this
    SDL_ShowCursor(SDL_DISABLE);
    MousePosition(x,y,button);
    usedbuf = enable;
    CreateDisplay();
    SDL_ShowCursor(SDL_DISABLE);
    SDL_WarpMouseInWindow(window, Uint16(x),Uint16(y));
    HandleEventQueue();
    //
    // Force-back the mouse button state. SDL doesn't notice if the
    // mouse button is still pressed on re-opening the screen.
    MouseButton = button;
    EnforceFullRefresh();
  }
}
///

/// SDL_FrontEnd::ResetVertical
// Signal that we start the display generation again from top. Hence, this implements
// some kind of "vertical sync" signal for the display generation.
void SDL_FrontEnd::ResetVertical(void)
{
  CurrentLine = 0;
  Row         = ActiveBuffer();
  RGBRow      = ActiveRGBFrame + 1 + FrameModulo;
}
///

/// SDL_FrontEnd::DiagDeblocker::MagnifyLine
// Magnification filters, templated by data type.
template<class datatype,LONG PxWidth,LONG PxHeight>
void SDL_FrontEnd::DiagDeblocker<datatype,PxWidth,PxHeight>::MagnifyLine(void *out,void *in,LONG width,LONG,
									 LONG pitch,LONG modulo)
{
  datatype *hwp = (datatype *)out;
  datatype *swp = (datatype *)in;
  datatype *north,*south;
  datatype *up,*down;
  //
  north  = swp - modulo;
  south  = swp + modulo;
  up     = hwp;
  if (PxHeight == 3) {
    // Need a pointer to the middle line as well.
    hwp += pitch;
  }
  down   = hwp + pitch;
  do {
    int lut   = 0; // lookup value, depends on equality of the diagonals.
    lut      |= (north[0] == swp[-1])?1:0; // northwest test
    lut      |= (north[0] == swp[+1])?2:0; // northeast test
    lut      |= (south[0] == swp[-1])?4:0; // southwest test
    lut      |= (south[0] == swp[+1])?8:0; // southeast test
    //
    // Get new pixel colors dependent on whether diagonals are equal: Thus,
    // test for 45 degree lines.
    *up++      = swp[northwest[lut]];
    if (PxWidth == 3) {      
      *up++    = swp[0];
    }
    *up++      = swp[northeast[lut]];
    //
    // Now the center line if we have one.
    if (PxHeight == 3) {
      *hwp++   = swp[0];
      if (PxWidth == 3) {
	*hwp++ = swp[0];
      }
      *hwp++   = swp[0];
    }
    //
    // Now the lower line.
    *down++    = swp[southwest[lut]];
    if (PxWidth == 3) {
      *down++  = swp[0];
    }
    *down++    = swp[southeast[lut]];
    //
    //
    swp++,north++,south++;
  } while(--width);
}
///

/// SDL_FrontEnd::RefreshPaletteLine
// Update the contents of the hardware buffer for a single line
// hwp is the hardware pointer where the data should go. swp is the software pointer
// where the data came from
// height is the remaining height in hardware lines. 
// pitch is the hardware screen modulo
void SDL_FrontEnd::RefreshPaletteLine(UBYTE *hwp,UBYTE *swp,LONG width,LONG height,LONG pitch)
{
  size_t hcnt = PixelHeight;

  if (Deblocker) {
    Deblocker->MagnifyLine(hwp,swp,width,height,pitch,FrameModulo);
    return;
  }

  if (PixelWidth > 1) {
    UBYTE *sws = swp;
    UBYTE *hwd = hwp;
    LONG w     = width;
    do {
      switch(PixelWidth) {
      case 8:
	*((ULONG *)hwd) = Quadrupler[*sws];
	hwd            += 4;
      case 4:
	*((ULONG *)hwd) = Quadrupler[*sws];
	hwd            += 4;
	break;	    
      case 2:
	*((UWORD *)hwd) = Doubler[*sws];
	hwd            += 2;
	break;	  
      case 6:
	*((UWORD *)hwd) = Doubler[*sws];
	hwd            += 2;
	*((UWORD *)hwd) = Doubler[*sws];
	hwd            += 2;
	*((UWORD *)hwd) = Doubler[*sws];
	hwd            += 2;
	break;
      case 7:
	*hwd++ = *sws;
	*hwd++ = *sws;
      case 5:
	*hwd++ = *sws;
	*hwd++ = *sws;
      case 3:
	*hwd++ = *sws;
	*hwd++ = *sws;
	*hwd++ = *sws;
      }
      sws++;
    } while(--w);
    height--;
    if (--hcnt && height) {
      w = width * PixelWidth;
      do {
	memcpy(hwp + pitch,hwp,w);
	hwp += pitch;
	height--;
      } while(--hcnt && height);
    }	    
    hwp += pitch;
  } else {
    do {
      memcpy(hwp,swp,width);
      hwp += pitch;
      height--;
    } while(--hcnt && height);
  }
}
///

/// SDL_FrontEnd::RefreshTruecolorLine
// Similar to the above, but specialized to RGB
// true color buffers.
void SDL_FrontEnd::RefreshTruecolorLine(PackedRGB *hwp,PackedRGB *swp,LONG width,LONG height,LONG pitch)
{
  size_t hcnt = PixelHeight;
 
  if (Deblocker) {
    Deblocker->MagnifyLine(hwp,swp,width,height,pitch,FrameModulo);
    return;
  }

  if (PixelWidth > 1) {
    PackedRGB *sws = swp;
    PackedRGB *hwd = hwp;
    LONG w     = width;
    do {
      switch(PixelWidth) {
      case 8:
	*hwd++          = *sws;
      case 7:
	*hwd++          = *sws;
      case 6:
 	*hwd++          = *sws;
      case 5:
 	*hwd++          = *sws;
      case 4:
 	*hwd++          = *sws;
      case 3:
 	*hwd++          = *sws;
      case 2:
 	*hwd++          = *sws;
 	*hwd++          = *sws;
      }
      sws++;
    } while(--w);
    height--;
    if (--hcnt && height) {
      w = width * PixelWidth;
      do {
	memcpy(hwp + pitch,hwp,w * sizeof(PackedRGB));
	hwp += pitch;
	height--;
      } while(--hcnt && height);
    }	    
    hwp += pitch;
  } else {
    do {
      memcpy(hwp,swp,width * sizeof(PackedRGB));
      hwp += pitch;
      height--;
    } while(--hcnt && height);
  }
}
///

/// SDL_FrontEnd::RebuildScreen
// Refresh the contents of the screen by blitting into the VideoRAM
void SDL_FrontEnd::RebuildScreen(void)
{  
  SDL_Rect full;
  int rects = 0;
  //
  // In case the screen setup failed, we might have entered
  // here with a NULL pointer to the screen.
  if (!sdl_initialized || screen == NULL)
    return;
  // Check whether there is any line to refresh in first
  // case. If not, we can just bail out.
  if (memchr(ModifiedLines,1,Antic::PALTotal) == NULL)
    return;
  //
  // Shield the cursor if we have to
  if (showpointer && ShieldCursor)
    SDL_ShowCursor(SDL_DISABLE);
  // Lock the SDL screen to perform the HW access. If this fails, we forget
  // the refresh here.
  if (SDL_LockSurface(screen) == 0) {
    if (truecolor) {
      rects = RebuildTruecolorScreen(full);
    } else {
      rects = RebuildPaletteScreen(full);
    }
    SDL_UnlockSurface(screen);
  }
  //
  //
  if (fullrefresh) {
    // Make a complete refresh over all of the screen.
    SDL_BlitSurface(screen, NULL, SDL_GetWindowSurface(window), NULL);
  } else {
    for(int i=0; i < rects; i++) {
      SDL_BlitSurface(screen, &UpdateRects[i], SDL_GetWindowSurface(window), &UpdateRects[i]);
    }
  }
  SDL_UpdateWindowSurface(window);
  //
  // Undo the cursor shielding.
  if (showpointer && ShieldCursor)
    SDL_ShowCursor(SDL_ENABLE);  
  //
  // If double buffering enabled, flip now screens.
  if (usedbuf && DoubleBuffer) {
    UBYTE *tmp;
    PackedRGB *rgb;

    // Swap active and inactive buffers as well.
    tmp                  = ActiveFrame;
    ActiveFrame          = AlternateFrame;
    AlternateFrame       = tmp;
    rgb                  = ActiveRGBFrame;
    ActiveRGBFrame       = AlternateRGBFrame;
    AlternateRGBFrame    = rgb;
    fullrefresh          = alternatefullrefresh;
    alternatefullrefresh = false;
  } else {
    fullrefresh          = false;
  }
  //
  // Clean the flag array keeping the lines that have been changed.
  memset(ModifiedLines,0,Antic::PALTotal);
}
///

/// SDL_FrontEnd::RebuildPaletteScreen
// Refresh the contents of the screen by blitting into the VideoRAM
// This part here is specialized to palette video buffers and called
// by the above.
int SDL_FrontEnd::RebuildPaletteScreen(SDL_Rect &full)
{
  UBYTE *hwp; // pointer into video memory
  UBYTE *swp; // pointer into source memory
  UBYTE *mfp; // modification flags
  size_t width,height;
  int y,ri = 0; // current rectangle index.
  bool activerect = false;
  LONG hwheight,hwwidth;
  //
  // Get pointers to the screen contents
  //
  hwp      = (UBYTE *)screen->pixels;
  swp      = &ActiveFrame[LeftEdge + 1 + FrameModulo * (TopEdge + 1)];
  mfp      = ModifiedLines;
  // Pre-compute some dimensions after scale-up
  hwwidth  = Width  * PixelWidth;
  hwheight = Height * PixelHeight;
  if (hwwidth < screen->w) {
    // Hardware screen is larger than atari output buffer.
    // Offset the hardware pointer such that we blit somewhere into the center
    // Note that we requested an 8bpp screen.
    full.x = Sint16((screen->w - hwwidth) >> 1);
    full.w = Uint16(hwwidth);
    hwp   += Sint16((screen->w - hwwidth) >> 1);
    width  = Uint16(Width);
  } else {
    // Offset the software pointer such that we see only the center of the
    // screen.
    full.x = 0;
    full.w = Uint16(screen->w);
    swp   += ((hwwidth - screen->w) / PixelWidth) >> 1;
    width  = screen->w / PixelWidth;
  }
  // Now the same game with the height.
  if (hwheight < screen->h) {
    full.y = Sint16((screen->h  - hwheight) >> 1);
    full.h = Uint16(hwheight);
    hwp   += ((screen->h - hwheight) >> 1) * (screen->pitch);
    height = hwheight;
  } else {
    full.y = 0;
    full.h = Uint16(screen->h);
    swp   += (((hwheight - screen->h) / PixelHeight) >> 1) * FrameModulo;
    mfp   += (((hwheight - screen->h) / PixelHeight) >> 1);
    height = screen->h;
  }
  //
  // Now blit the screen, assuming height!=0
  y = 0;
  do {
    bool refreshline = true;
    // Check whether the current line changed at all. If not, so just
    // skip it and leave it unrefreshed.
    if (fullrefresh == false) {
      if (*mfp == 0) {
	refreshline  = false;
	// Line did not change. If we have no active rectangle, just forget
	// this. Otherwise, start a new rectangle for the next possibly
	// modified line and note that we have no active rectangle
	// anymore.
	if (activerect) {
	  // Current rectangle is complete. Increment ri and check
	  // wether we have any further rectangles.
	  if (++ri >= Rects) {
	    fullrefresh = true; // Enforce a full refresh. Doesn't make
	    // sense to split the screen into tiny snippets.
	  }
	  activerect = false;
	}
      } else {
	// Line did change. Check whether we have an active rectangle. If
	// not so, start a new on here. Otherwise, enlarge the current
	// rectangle.
	if (activerect) {
	  UpdateRects[ri].h += Uint16(PixelHeight);
	} else {
	  // Start a new rectangle. Fill in the basics.
	  UpdateRects[ri].x = 0;
	  UpdateRects[ri].y = Sint16(y);
	  UpdateRects[ri].w = Uint16(screen->w);
	  UpdateRects[ri].h = Uint16(PixelHeight); // this line
	  activerect   = true;
	}
      }
    }
    if (refreshline) {
      // Update the line if required.
      RefreshPaletteLine(hwp,swp,LONG(width),LONG(height),screen->pitch);
    }
    hwp    += screen->pitch * PixelHeight;
    height -= (height > (size_t)PixelHeight)?PixelHeight:height;
    swp    += FrameModulo;
    mfp    ++;
    y      += PixelHeight;
  } while(height);
  
  if (activerect)
    ri++;

  return ri;
}
///

/// SDL_FrontEnd::RebuildTruecolorScreen
// Refresh the contents of the screen by blitting into the VideoRAM
// This is the true color version of the above.
int SDL_FrontEnd::RebuildTruecolorScreen(SDL_Rect &full)
{
  PackedRGB *hwp; // pointer into video memory
  PackedRGB *swp; // pointer into source memory
  UBYTE     *mfp; // modification flags
  size_t width,height;
  int y,ri = 0;   // current rectangle index.
  bool activerect = false;
  LONG hwheight,hwwidth;
  int pitch       = screen->pitch / sizeof(PackedRGB); // this is in bytes, not in entries.
  //
  // Get pointers to the screen contents
  //
  hwp      = (PackedRGB *)screen->pixels; // Let's hope this is correct....
  swp      = &ActiveRGBFrame[LeftEdge + 1 + FrameModulo * (TopEdge + 1)];
  mfp      = ModifiedLines;
  // Pre-compute some dimensions after scale-up
  hwwidth  = Width  * PixelWidth;
  hwheight = Height * PixelHeight;
  if (hwwidth < screen->w) {
    // Hardware screen is larger than atari output buffer.
    // Offset the hardware pointer such that we blit somewhere into the center
    // Note that we requested an 8bpp screen.
    full.x = Sint16((screen->w - hwwidth) >> 1);
    full.w = Uint16(hwwidth);
    hwp   += Sint16((screen->w - hwwidth) >> 1);
    width  = Uint16(Width);
  } else {
    // Offset the software pointer such that we see only the center of the
    // screen.
    full.x = 0;
    full.w = Uint16(screen->w);
    swp   += ((hwwidth - screen->w) / PixelWidth) >> 1;
    width  = screen->w / PixelWidth;
  }
  // Now the same game with the height.
  if (hwheight < screen->h) {
    full.y = Sint16((screen->h  - hwheight) >> 1);
    full.h = Uint16(hwheight);
    hwp   += ((screen->h - hwheight) >> 1) * pitch;
    height = hwheight;
  } else {
    full.y = 0;
    full.h = Uint16(screen->h);
    swp   += (((hwheight - screen->h) / PixelHeight) >> 1) * FrameModulo;
    mfp   += (((hwheight - screen->h) / PixelHeight) >> 1);
    height = screen->h;
  }
  //
  // Now blit the screen, assuming height!=0
  y = 0;
  do {
    bool refreshline = true;
    // Check whether the current line changed at all. If not, so just
    // skip it and leave it unrefreshed.
    if (fullrefresh == false) {
      if (*mfp == 0) {
	refreshline  = false;
	// Line did not change. If we have no active rectangle, just forget
	// this. Otherwise, start a new rectangle for the next possibly
	// modified line and note that we have no active rectangle
	// anymore.
	if (activerect) {
	  // Current rectangle is complete. Increment ri and check
	  // wether we have any further rectangles.
	  if (++ri >= Rects) {
	    fullrefresh = true; // Enforce a full refresh. Doesn't make
	    // sense to split the screen into tiny snippets.
	  }
	  activerect = false;
	}
      } else {
	// Line did change. Check whether we have an active rectangle. If
	// not so, start a new on here. Otherwise, enlarge the current
	// rectangle.
	if (activerect) {
	  UpdateRects[ri].h += Uint16(PixelHeight);
	} else {
	  // Start a new rectangle. Fill in the basics.
	  UpdateRects[ri].x = 0;
	  UpdateRects[ri].y = Sint16(y);
	  UpdateRects[ri].w = Uint16(screen->w);
	  UpdateRects[ri].h = Uint16(PixelHeight); // this line
	  activerect   = true;
	}
      }
    }
    if (refreshline) {
      // Update the line if required.
      RefreshTruecolorLine(hwp,swp,LONG(width),LONG(height),pitch);
    }
    hwp    += pitch * PixelHeight;
    height -= (height > (size_t)PixelHeight)?PixelHeight:height;
    swp    += FrameModulo;
    mfp    ++;
    y      += PixelHeight;
  } while(height); 
   
  if (activerect)
    ri++;

  return ri;
}
///

/// SDL_FrontEnd::HandleEventQueue
// Get keyboard/mouse events from SDL and run them one after another
void SDL_FrontEnd::HandleEventQueue(void)
{
  SDL_Event event;
  //
  // Poll the next event and handle it.
  while (SDL_PollEvent(&event)) {
    switch(event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      if(event.key.repeat) continue;
      // Both key events. Handle them separately
      HandleKeyEvent(&event.key);
      break;
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      HandleMouseEvent(&event);
      break;
    case SDL_WINDOWEVENT:
      switch(event.window.event) {
      case SDL_WINDOWEVENT_FOCUS_GAINED:
        keyboardfocus = true;
	break;
      case SDL_WINDOWEVENT_FOCUS_LOST:
        keyboardfocus = false;
	break;
      case SDL_WINDOWEVENT_RESIZED:
      case SDL_WINDOWEVENT_EXPOSED:
        EnforceFullRefresh();
	break;
      }
      break;
    case SDL_QUIT:
      // Short and effective...
      machine->Quit() = true;
      break;
    }
  }
}
///

/// SDL_FrontEnd::HandleKeyEvent
// Handles a key event and updates the keyboard status of the
// emulation as required
void SDL_FrontEnd::HandleKeyEvent(SDL_KeyboardEvent *event)
{
  int keysym;
  bool downflag;
  bool shift,control;

  downflag = (event->type==SDL_KEYDOWN); // set if the key is going down

  // Now get the state of the shift and control keyboard modifiers
  shift    = ((event->keysym.mod) & (KMOD_LSHIFT | KMOD_RSHIFT))?(true):(false);
  control  = ((event->keysym.mod) & (KMOD_LCTRL  | KMOD_RCTRL ))?(true):(false);
  keysym   = event->keysym.sym;
  //
  // If this looks like the X11 interface, then this is no coincidence...
  // Check whether the keypad stick uses this key. If so, perform a mouse
  // movement.
  if (KeypadStick && showpointer == false) {
    if (KeypadStick->HandleJoystickKeys(downflag,keysym)) {
      return;
    }
  }
  //
  // Now check the keyboard symbol for validty
  if (keysym < 0x100 && keysym != 0x7f) { // Handle DEL separately
    UBYTE sym = 0xff; // invalid symbol
    if (downflag && event->keysym.sym >= 0x20 && event->keysym.sym <= 0xff) {
      // We must not try to translate control codes by means of unicode. I've no idea
      // why, but SDL generates for my german keyboard \e ("ESC") as unicode for
      // ^3 (= EOF on Atari).
      sym = UBYTE(event->keysym.sym);
      // Another wierdo: If RALT is set, then the CTRL modifier might be
      // set erraneously. It is ignored here.
      if ((event->keysym.mod & KMOD_RALT) && control && (sym & 0x40)) {
	control = false;
      }
      // Another wierdo: If ctrl is held down and the symbol is in range 
      // 0x00..0x1f, then this code got generated by an alphanumeric
      // key plus control we have to re-generate now.
      if (control && /*sym >= 0x00 &&*/ sym <= 0x1f) {
	sym |= 0x40;
      }
    } else {
      if (keysym < 0x80)
	sym = UBYTE(keysym & 0x7f);
    }
    // And now for the tricky stuff: HandleKey gets the shift'ed flag from
    // the ascii code pushed in. This has the advantage that characters like
    // # that require shift on some keyboards, but not on others, come out
    // right all the time. However, it has the drawback that for SDL these
    // keys are also seen by CAPS, meaning I cannot lower-case them when CAPS
    // is pressed on the PC side and released on the Atari side. Ugly. I there
    // fore process letters by pushing the shift flag in directly, whereas
    // all other keys get their shift flag implied from their code.
    if ((sym >= 'A' && sym <= 'Z') || (sym >= 'a' && sym <= 'z') || (sym < ' ') || (shift && control)) {
      if (sym < 0x7f) keyboard->HandleSimpleKey(downflag,sym,shift,control);
    } else {
      if (sym < 0x7f) keyboard->HandleKey(downflag,sym,shift,control);
    }
  } else switch(keysym) {
  case SDLK_LSHIFT:
  case SDLK_RSHIFT:
    keyboard->HandleSpecial(downflag,Keyboard::Shift,shift,control);
    break;
  case SDLK_CAPSLOCK:
    // Wierd SDL. Caps Up/Down works differently: Caps Up means that
    // caps becomes inactive, not that the key goes up.
    // If we still have a caps up event pending, do not emulate this
    // guy here.
    if (capsup == 0) {
      keyboard->HandleSpecial(true,Keyboard::Caps,shift,control);
      // Send a caps up event five VBIs later.
      capsup = 5;
    }
    break;
  case SDLK_LALT:
    //case SDLK_RALT:
  case SDLK_LGUI:
  case SDLK_RGUI:
    keyboard->HandleSpecial(downflag,Keyboard::Atari,shift,control);
    break;
  case SDLK_APPLICATION:
  case SDLK_F1:
    if (downflag) machine->LaunchMenu() = true;
    break;
  case SDLK_F2:
    keyboard->HandleSpecial(downflag,Keyboard::Option,shift,control);
    break;
  case SDLK_F3:
    keyboard->HandleSpecial(downflag,Keyboard::Select,shift,control);
    break;
  case SDLK_F4:
    keyboard->HandleSpecial(downflag,Keyboard::Start,shift,control);
    break; 
  case SDLK_F5:
  case SDLK_HELP:
    keyboard->HandleSpecial(downflag,Keyboard::Help,shift,control);
    break;
  case SDLK_F6:
    if (downflag) machine->WarmReset();
    break;
  case SDLK_F7:
    if (downflag) machine->ColdReset() = true;
    break;
  case SDLK_F8:
    keyboard->HandleSpecial(downflag,Keyboard::Break,shift,control);
    break;
  case SDLK_F9:
  case SDLK_PRINTSCREEN:
    // Request a screen dump the next time.
    if (downflag) dump = true;
    break;
  case SDLK_F10:
    if (downflag) machine->Quit() = true;
    break;
  case SDLK_F11:
  case SDLK_PAUSE:
    if (downflag) machine->Pause() = !(machine->Pause());
    break;
  case SDLK_F12:
#ifdef BUILD_MONITOR
    if (downflag) machine->LaunchMonitor() = true;
#endif
    break;
  case SDLK_HOME:
  case SDLK_CLEAR:
    if (control && shift) {
      keyboard->HandleSimpleKey(downflag,'<',true,true); // clear screen
    } else {
      keyboard->HandleSimpleKey(downflag,'<',false,true); // clear screen
    }
    break;
  case SDLK_INSERT:
    if (shift) {
      keyboard->HandleSimpleKey(downflag,'>',true,control); // insert line
    } else {
      keyboard->HandleSimpleKey(downflag,'>',false,true); // insert character
    }
    break;
  case SDLK_BACKSPACE:
    keyboard->HandleSimpleKey(downflag,0x08,shift,control); // backspace
    break;
  case SDLK_DELETE:
    keyboard->HandleSimpleKey(downflag,0x08,shift,!control); // delete
    break;
  case SDLK_LEFT:
    keyboard->HandleSimpleKey(downflag,'+',shift,!control); // cursor left
    break;
  case SDLK_RIGHT:
    keyboard->HandleSimpleKey(downflag,'*',shift,!control); // cursor right
    break;
  case SDLK_UP:
    keyboard->HandleSimpleKey(downflag,'-',shift,!control); // cursor up
    break;
  case SDLK_DOWN:
    keyboard->HandleSimpleKey(downflag,'=',shift,!control); // cursor up
    break;
  case SDLK_ESCAPE:
    keyboard->HandleSimpleKey(downflag,0x1b,shift,control);
    break;
  case SDLK_TAB:
    keyboard->HandleSimpleKey(downflag,0x09,shift,control);
    break;
  case SDLK_RETURN:
    keyboard->HandleSimpleKey(downflag,0x0a,shift,control);
    break;
  default:
    // Shut up the compiler
    break;
  }
}
///

/// SDL_FrontEnd::HandleMouseEvent
// Handle a mouse movement or a mouse button event, required
// to feed the mousestick joystick input emulation
void SDL_FrontEnd::HandleMouseEvent(SDL_Event *event)
{
  LONG mousex,mousey;
  LONG dx,dy;
  SDL_MouseButtonEvent *mbut;
  SDL_MouseMotionEvent *mmove;

  switch(event->type) {
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP:
    mbut             = &(event->button);
    if (mbut->button == SDL_BUTTON_LEFT) {
      MouseStick.button1    = (mbut->state == SDL_PRESSED); 
      RelMouseStick.button1 = (mbut->state == SDL_PRESSED); 
    } else if (mbut->button == SDL_BUTTON_RIGHT) {
      MouseStick.button2    = (mbut->state == SDL_PRESSED); 
      RelMouseStick.button2 = (mbut->state == SDL_PRESSED); 
    }
#if defined(SDL_BUTTON_WHEELUP) && defined(SDL_BUTTON_WHEELDOWN)
    if (mbut->button == SDL_BUTTON_WHEELUP)
      ScrolledLines--;
    if (mbut->button == SDL_BUTTON_WHEELDOWN)
      ScrolledLines++;
#endif
    // we do not care about the button index
    MouseStick.x      = mbut->x;
    MouseStick.y      = mbut->y;
    RelMouseStick.x   = mbut->x;
    RelMouseStick.y   = mbut->y;
    // Also update the coordinates for the GUI frontend
    MouseButton       = (mbut->state == SDL_PRESSED);
    mousex            = mbut->x / PixelWidth;
    mousey            = mbut->y / PixelHeight;
    break;
  case SDL_MOUSEMOTION:
    mmove             = &(event->motion);
    MouseStick.button1= (mmove->state & SDL_BUTTON(1))?true:false;
    MouseStick.button2= (mmove->state & SDL_BUTTON(3))?true:false;
    RelMouseStick.button1= (mmove->state & SDL_BUTTON(1))?true:false;
    RelMouseStick.button2= (mmove->state & SDL_BUTTON(3))?true:false;
    MouseStick.x      = mmove->x;
    MouseStick.y      = mmove->y;
    RelMouseStick.x   = mmove->x;
    RelMouseStick.y   = mmove->y;
    // Also update the coordinates for the GUI frontend
    /*
    **
    ** No longer update the buttons. The problem is that SDL resets
    ** the button state on re-opening the display, which breaks the
    ** title bar menu that expects that the button remains held down.
    ** Outch!
    MouseButton       = (mmove->state & (SDL_BUTTON(1) | SDL_BUTTON(3))?
			 (true):(false));
    */
    mousex            = mmove->x / PixelWidth;
    mousey            = mmove->y / PixelHeight;
    break;
  default:
    // Shut up the compiler
    mousex            = MouseX;
    mousey            = MouseY;
    break;
  }
  //
  // Check whether the mouse "freaked out". Interestingly, SDL seems to
  // cause such situations in case we hide the pointer. Yuck!
  dx = MouseX - mousex;
  if (dx < 0) dx = -dx;
  dy = MouseY - mousey;
  if (dy < 0) dy = -dy;
  if (dx > 16 || dy > 16) {
    // Mouse freaked out. Ignore this event.
    if (--MouseSpeedLimit) {
      return;
    }
  }
  MouseX          = mousex;
  MouseY          = mousey;
  MouseSpeedLimit = 2;
}
///

/// SDL_FrontEnd::MousePosition
// For GUI frontends within this buffer: Get the position and the status
// of the mouse within this buffer measured in internal coordinates.
void SDL_FrontEnd::MousePosition(LONG &x,LONG &y,bool &button)
{
  x = MouseX,y = MouseY,button = MouseButton;
}
///

/// SDL_FrontEnd::SetMousePosition
// Set the mouse position to the indicated coordinate
void SDL_FrontEnd::SetMousePosition(LONG x,LONG y)
{
  if (sdl_initialized) {
    SDL_WarpMouseInWindow(window, Uint16(x * PixelWidth),Uint16(y * PixelHeight));
    MouseX = x;
    MouseY = y;
  }
}
///

/// SDL_FrontEnd::MouseIsAvailable
// For GUI frontends: Check whether the mouse is available as input
// device. This returns false in case the mouse is an input device
// and has been grabbed on its "GamePort".
bool SDL_FrontEnd::MouseIsAvailable(void)
{
  if (MouseStick.ControllerChain().IsEmpty() && RelMouseStick.ControllerChain().IsEmpty()) {
    // No controller attached here, we may have the mouse
    return true;
  } else {
    return false; // else not
  }
}
///

/// SDL_FrontEnd::VBI
// Implement the frequent VBI activity of the display: This should
// update the display and read inputs
void SDL_FrontEnd::VBI(class Timer *,bool quick,bool pause)
{  
  // Check whether SDL is already initialized. If not, do now.
  // Do not enforce a cold reset unless required.
  if (sdl_initialized == false)
    return;
  //
  if (quick == false) {
    // Refresh the display here unless we're out of time.
    RebuildScreen();
    //
  }
  // Keep the quick flag for "on the fly" refresh requests
  quickvbi = quick;
  //
  // Check whether we have a caps up event still pending somewhere.
  // If it is, we need to generate a caps up here manually.
  if (capsup) {
    capsup--;
    if (capsup == 0) {
      keyboard->HandleSpecial(false,Keyboard::Caps,false,false);
    }
  }
  // Now run the event loop.
  HandleEventQueue();
  //
  // If screen dumping is enabled and we do not pause and we are not
  // on a hurry, make a dump. (-;
  if (pause == false && quick == false && dump == true) {
    DumpScreen();
    //
    // We're done with it.
    dump = false;
  }
  /*
  // If we are not pausing, advance the screen buffer to make room for
  // the next update.
  if (pause == false && quick == false) {
    // We do not advance the buffer if we need to be quick. This way,
    // "last" remains always the last usable buffer
    FrameBuffer->NextBuffer();
  }
  */
  //
  // Transmit the state of the emulated keyboard joystick.
  if (KeypadStick)
    KeypadStick->TransmitStates(pause);
  // Transmit the state of the emulated mouse joystick
  MouseStick.TransmitStates(pause,   Width  * PixelWidth,Height * PixelHeight);
  RelMouseStick.TransmitStates(pause,Width  * PixelWidth,Height * PixelHeight);
}
///

/// SDL_FrontEnd::MouseMoveStick::TransmitStates
// Transmit the position of the mouse stick to the
// emulation kernel, resp. to all ports that want to listen.
void SDL_FrontEnd::MouseMoveStick::TransmitStates(bool paused,int width,int height)
{
  //
  // If we are paused, feed a silent joystick with no button pressed.
  if (paused) {
    FeedAnalog(0,0);
    FeedButton(false,0);
    FeedButton(false,1);
  } else {
    // Otherwise read the information from the class body.
    if (isrel) {
      if (!ControllerChain().IsEmpty()) {
	int dxl = int(x - lastx) << 12;
	int dyl = int(y - lasty) << 12;
	WORD dx,dy;
	//
	lastx   = x;
	lasty   = y;
	//
	dx      = (dxl > 32767)?(32767):((dxl < -32767)?(-32767):(dxl));
	dy      = (dyl > 32767)?(32767):((dyl < -32767)?(-32767):(dyl));
	//
	if (x < (width  >> 2) || x > (width  - (width >> 2)) ||
	    y < (height >> 2) || y > (height - (height >> 2))) {
//	  SDL_WarpMouseInWindow(window, width >> 1,height >> 1);
	  lastx = width  >> 1;
	  lasty = height >> 1;
	}
	//
	FeedAnalog(dx,dy);
	FeedButton(button1,0);
	FeedButton(button2,1);
      }
    } else { 
      int centerx = width  >> 1;
      int centery = height >> 1;
      
      FeedAnalog(WORD((x - centerx)*32768/centerx),
		 WORD((y - centery)*32768/centery));
      FeedButton(button1,0);
      FeedButton(button2,1);
    }
  }
}
///

/// SDL_FrontEnd::DumpScreen
// Make a screen dump from the SDL interface frontend
void SDL_FrontEnd::DumpScreen(void)
{
  class ScreenDump dumper(machine,colormap,LeftEdge,TopEdge,
			  Width,Height,FrameModulo,Format);
  FILE *dumpout;
  char buf[256];
  const char *annex;

  switch(Format) {
  case ScreenDump::PNM:
    annex = "ppm";
    break;
  case ScreenDump::BMP:
    annex = "bmp";
    break;
#ifdef USE_PNG
  case ScreenDump::PNG:
    annex = "png";
    break;
#endif  
  default:
    annex = NULL; // shut-up the compiler
    Throw(InvalidParameter,"SDL_FrontEnd::DumpScreen",
	  "invalid file format requested for the screen dump");
  }
  
  snprintf(buf,255,"%s_%d.%s",ScreenBaseName,dumpcnt++,annex);
  
  dumpout = fopen(buf,"wb");
  if (dumpout) {
    try {
      if (truecolor)
	dumper.Dump(ActiveRGBFrame + 1 + FrameModulo,dumpout);
      else
	dumper.Dump(ActiveFrame + 1 + FrameModulo,dumpout);
      fclose(dumpout);
    } catch(...) {
      // Shut up the temporary file, at least. We don't expect
      // that the Os cleans up behind us.
      fclose(dumpout);
      throw;
    }
  } else {  
    throw(AtariException(errno,"SDL_FrontEnd::DumpScreen",
			 "Unable to open screen dump output file %s.",buf));
  }
}
///

/// SDL_FrontEnd::ScrollDistance
// Return the number of lines scrolled since we asked last.
int SDL_FrontEnd::ScrollDistance(void)
{ 
  int lines     = ScrolledLines;

  ScrolledLines = 0;

  return lines;
}
///

/// SDL_FrontEnd::SetLED
// If we want to emulate a flickering drive LED, do it here.
void SDL_FrontEnd::SetLED(bool)
{
  // does nothing right now
}
///

/// SDL_FrontEnd::EnforceFullRefresh
// Enforce a full display refresh, i.e. after the monitor returnes
void SDL_FrontEnd::EnforceFullRefresh(void)
{
  fullrefresh          = true;
  alternatefullrefresh = true;
  // Set the flag array keeping the lines that have been changed; this indicates
  // that we have to perform a full refresh anyhow.
  memset(ModifiedLines,1,Antic::PALTotal); 
}
///

/// SDL_FrontEnd::SwitchScreen
// Switch the SDL screen to foreground or background
void SDL_FrontEnd::SwitchScreen(bool foreground)
{
  if (foreground) {
    if (fullscreen == false && FullScreen)
      if(SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN))
	fullscreen = true;
  } else {
    if (fullscreen == true && FullScreen)
      if(SDL_SetWindowFullscreen(window, 0))
	fullscreen = false;
  }
}
///

/// SDL_FrontEnd::ShowPointer
// Enable or disable the mouse pointer
void SDL_FrontEnd::ShowPointer(bool showit)
{
  // Do not bother if we don't have a display here.
  if (sdl_initialized) {
    SDL_ShowCursor((showit)?(SDL_ENABLE):(SDL_DISABLE));
  }
  showpointer = showit;
}
///

/// SDL_FrontEnd::ColdStart
// ColdStart the system, initialize SDL
void SDL_FrontEnd::ColdStart(void)
{
  WarmStart();
}
///

/// SDL_FrontEnd::WarmStart
// WarmStart the SDL display.
void SDL_FrontEnd::WarmStart(void)
{
  // Enforce a full display refresh now
  fullrefresh          = true;
  alternatefullrefresh = true; 
  if (KeypadStick)
    KeypadStick->Reset();
}
///

/// SDL_FrontEnd::DisplayStatus
// Display some facts about the SDL frontend
void SDL_FrontEnd::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("SDL_FrontEnd Status:\n"
		   "\tScreen Dump Base Name: %s\n"
		   "\tTrue color display   : %s\n"
		   "\tDeblocking filter    : %s\n"
		   "\tLeftEdge             : " LD "\n"
		   "\tTopEdge              : " LD "\n"
		   "\tWidth                : " LD "\n"
		   "\tHeight               : " LD "\n",
		   ScreenBaseName,
		   truecolor?("on"):("off"),
		   Deblocking?("on"):("off"),
		   LeftEdge,TopEdge,Width,Height);
}
///

/// SDL_FrontEnd::ParseArgs
void SDL_FrontEnd::ParseArgs(class ArgParser *args)
{
  LONG oldpixheight = PixelHeight;
  LONG oldpixwidth  = PixelWidth;
  LONG le           = LeftEdge;
  LONG te           = TopEdge;
  LONG w            = Width;
  LONG h            = Height;
  LONG format       = Format;
  bool fullscreen   = FullScreen;
  bool doublebuffer = DoubleBuffer;
  bool deblocking   = Deblocking;
  static const struct ArgParser::SelectionVector Formats[] = {
    {"PNM", ScreenDump::PNM},
    {"BMP", ScreenDump::BMP},
#ifdef USE_PNG
    {"PNG", ScreenDump::PNG},
#endif
    {NULL , 0}
  };
  
  if (Unit == 0) {
    args->DefineTitle("SDL_FrontEnd");  
    args->DefineLong("LeftEdge","set left edge of visible screen",0,64,LeftEdge);
    args->DefineLong("TopEdge","set top edge of visible screen",0,64,TopEdge);
    args->DefineLong("Width","set width of visible screen",
		     320,Antic::DisplayModulo,Width);
    args->DefineLong("Height","set height of visible screen",
		     192,Antic::DisplayHeight,Height);
  } else {
    args->DefineTitle("XEPSDL_FrontEnd");
  }
  args->DefineLong("PixelWidth","set width of one pixel in screen pixels",1,8,PixelWidth);
  args->DefineLong("PixelHeight","set height of one pixel in screen lines",1,8,PixelHeight);
  args->DefineString("ScreenBase","file base name for screen dumps",ScreenBaseName);
  args->DefineSelection("DumpFormat","screen dump gfx file format",Formats,format);
  args->DefineBool("FullScreen","enable full screen display",FullScreen);
  args->DefineBool("DoubleBuffer","enable double buffering",DoubleBuffer);
  args->DefineBool("ShieldCursor","bug workaround to shield cursor from overdrawing",ShieldCursor);
  args->DefineBool("DeBlocker","enable improved magnification routines",Deblocking);

  Format = (ScreenDump::GfxFormat)format;
    
  if (PixelWidth   != oldpixwidth  ||
      PixelHeight  != oldpixheight ||
      LeftEdge     != le           ||
      TopEdge      != te           ||
      Width        != w            ||
      Height       != h            ||
      Deblocking   != deblocking   ||
      FullScreen   != fullscreen   ||
      DoubleBuffer != doublebuffer ||      
      truecolor    != machine->GTIA()->SuggestTrueColor() ||
      (colormap && (colormap != machine->GTIA()->ActiveColorMap()))
      ) {    
    args->SignalBigChange(ArgParser::Reparse);
    if (sdl_initialized)
      CreateDisplay();
    /*
    **
    ** The above is totally sufficient as far as SDL
    ** is concerned... I hope...
    CloseSDL();
    sdl_initialized = false;
    */
  }
}
///

/// 
#endif // of if SDL.h available
///
