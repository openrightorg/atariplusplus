/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sdl_frontend.hpp,v 1.41 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: A frontend using the sdl library
 **********************************************************************************/

#ifndef SDL_FRONTEND_HPP
#define SDL_FRONTEND_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "machine.hpp"
#include "chip.hpp"
#include "display.hpp"
#include "keyboardstick.hpp"
#include "gameport.hpp"
#include "string.hpp"
#include "colorentry.hpp"
#include "screendump.hpp"
#include "sdlclient.hpp"
#if HAVE_SDL2_SDL_H && HAVE_SDL_CREATEWINDOW
#include <SDL2/SDL.h>
#endif
///

/// Forwards
class ArgParser;
class Monitor;
class Keyboard;
class KeypadStick;
///

/// Class SDL_FrontEnd
// This class offers a graphical frontend using SDL 
#if HAVE_SDL2_SDL_H && HAVE_SDL_CREATEWINDOW
class SDL_FrontEnd : public AtariDisplay, public SDLClient {
  //
  // Import the packed RGB type.
  typedef AtariDisplay::PackedRGB PackedRGB;
  //
  // Points to the SDL drawing plane
  SDL_Window    *window;
  SDL_Surface    *screen;
  //
  // This flag gets set as soon as SDL got initialized
  bool            sdl_initialized;
  //
  // Pointer to the color palette of the SDL screen
  SDL_Color      *colors;
  // Clone from the GTIA here, used to get notified about
  // color table changes.
  const struct ColorEntry *colormap;
  //
  // The base name for the screen dumps goes here
  char           *ScreenBaseName;
  //
  // Gfx File Format for the screen dump
  ScreenDump::GfxFormat Format;
  //
  // The following boolean is set to true in case
  // we have a true-color display.
  bool            truecolor;
  //
  // Counts the number of screendumps, gets incremented each time
  int             dumpcnt;
  //
  // Is set to true if the user requests a screen dump
  bool            dump;
  //
  // If this boolean is true, we're currently in the foreground
  bool            fullscreen;
  //
  // True if we have keyboard input focus.
  bool            keyboardfocus;
  //
  // Pointer to the active video frame to be constructed by
  // GTIA and ANTIC.
  UBYTE          *ActiveFrame;
  // Pointer to the alternate (currently displaying, but not
  // rendering) frame for double-buffering screens.
  UBYTE          *AlternateFrame;
  // The pointer to the current row that is getting filled in.
  UBYTE          *Row;
  // The pointer to a temporary row buffer GTIA fills data into
  UBYTE          *InputBuffer;
  // The pointer to a (boolean) array indicating the rows that have changed.
  UBYTE          *ModifiedLines;
  //
  // Similar buffers for the true color operation.
  PackedRGB      *ActiveRGBFrame;
  PackedRGB      *AlternateRGBFrame;
  PackedRGB      *RGBRow;
  PackedRGB      *RGBInputBuffer;
  // The line modification buffer is shared between true color
  // and palettized mode.
  //
  // Boolean indicating whether a full refresh is necessary
  bool            fullrefresh;  
  // The same flag for the hidden buffer of a double-buffered
  // screen
  bool            alternatefullrefresh;
  //
  // If this is set, then an enabled double buffer is used.
  bool            usedbuf;
  //
  // If this boolean is set, then the pointer is visible. This is an internal
  // cached value that gets set whenever the pointer must be hidden with the
  // display still disabled.
  bool            showpointer;
  //
  // If this boolean is true, then the last VBI required QUICK processing and we
  // cannot refresh the display on the fly.
  bool            quickvbi;
  // This counts the VBI until a delayed CAPS up is generated.
  int             capsup;
  //
  // Modulo of this frame buffer, i.e. add this to go to the next line
  int             FrameModulo;
  //
  // Number of scrolled lines/entries since last query.
  int             ScrolledLines;
  //
  // Link to the keyboard for feeding key events into
  class Keyboard *keyboard;
  //
  // Sub-classes for joystick input
  class KeyboardStick *KeypadStick;
  //
  // This game port class reads from the mouse input
  struct MouseMoveStick : public GamePort {
    //
    // Current position of the mouse to be transmitted
    WORD x,y;
    //
    // Previous location of the mouse.
    WORD lastx,lasty;
    //
    // Current state of the joytick button
    bool button1,button2;
    //
    bool isrel;
    //
    // Constructor: This class is called MouseStick
    MouseMoveStick(class Machine *mach,const char *name,bool relative)
      : GamePort(mach,name,0), 
	x(0), y(0), lastx(0), lasty(0), 
	button1(false), button2(false), 
	isrel(relative)
    { }
    //
    ~MouseMoveStick(void)
    { }
    //
    // Just transmit data to the input chain here.
    void TransmitStates(bool paused,int width,int height);
    //
  }               MouseStick,RelMouseStick;
  //
  // Internal backups of the last mouse position. This is kept here as temporaries
  // for the GUI frontend.
  LONG            MouseX,MouseY;
  bool            MouseButton;
  int             MouseSpeedLimit;   // SDL bug workaround against mouse rushes
  //
  // Counts the current line
  LONG            CurrentLine;
  //
  // Preferences
  //
  LONG            LeftEdge,TopEdge;  // orientation in respect to the atari display
  LONG            Width,Height;      // display dimensions of the SDL screen as requested
  //
  // Dimensions of a pixel
  LONG            PixelWidth;
  LONG            PixelHeight;
  //
  // Shield cursor from getting overdrawn. Actually, SDL should do
  // that, but it doesn't.
  bool            ShieldCursor;
  //
  // Set to get the full-screen display. This is the default.
  bool            FullScreen;
  //
  // Set to get a double buffering.
  bool            DoubleBuffer;
  //
  // Set this to get a deblocking filter.
  bool            Deblocking;
  //
  // Helper arrays for faster screen-build up
  UWORD          *Doubler;
  ULONG          *Quadrupler;
  //
  // Number of update rectangles for SDL
#ifdef HAS_MEMBER_INIT
  static const int Rects = 32; // number of rectangles we keep for updating
#else
#define            Rects   32
#endif
  //
  SDL_Rect         UpdateRects[Rects];
  //
  //
  // Base type for deblocking filters.
  class DeblockerBase {
    //
  protected:
    DeblockerBase(void)
    { }
  public:
    virtual ~DeblockerBase(void)
    { }
    //
    // This is the real function call we need to implement for the
    // job.
    virtual void MagnifyLine(void *out,void *in,LONG width,LONG height,LONG outmod,LONG inmod) = 0;
  }               *Deblocker; // pointer to the deblocker, if any.
  //  
  // Refresh the contents of the screen by blitting into the VideoRAM
  void RebuildScreen(void);
  // Refresh the contents of the screen by blitting into the VideoRAM
  // This part here is specialized to palette video buffers and called
  // by the above.
  int RebuildPaletteScreen(SDL_Rect &displayrect);
  // The same for true-color displays.
  int RebuildTruecolorScreen(SDL_Rect &displayrect);
  //
  // Update the contents of the hardware buffer for a single line
  // hwp is the hardware pointer where the data should go. swp is the software pointer
  // where the data came from
  // height is the remaining height in hardware lines. This will be counted down
  // pitch is the hardware screen modulo
  void RefreshPaletteLine(UBYTE *hwp,UBYTE *swp,LONG width,LONG height,LONG pitch);
  // The same again for true-color displays.
  void RefreshTruecolorLine(PackedRGB *hwp,PackedRGB *swp,LONG width,LONG height,LONG pitch);
  //
  // The currently one and only implementation of the above.
  // Magnification filters, templated by data type.
  template<class datatype,LONG PxWidth,LONG PxHeight>
  class DiagDeblocker : public DeblockerBase {
    //
    // lookup tables for the deblocking filter.
    static const signed char northwest[16];
    static const signed char northeast[16];
    static const signed char southwest[16];
    static const signed char southeast[16];
  public:
    DiagDeblocker(void)
    { }
    virtual ~DiagDeblocker(void)
    { }
    //
    void MagnifyLine(void *out,void *in,LONG width,LONG height,LONG outmod,LONG inmod);
  };
  //
  // Create the SDL display frontend
  void CreateDisplay(void);
  //
  // Get keyboard/mouse events from SDL and run them one after another
  void HandleEventQueue(void);
  //
  // Handle keyboard events specifically.
  void HandleKeyEvent(SDL_KeyboardEvent *event);
  //
  // Handle a mouse movement or a mouse button event, required
  // to feed the mousestick joystick input emulation
  void HandleMouseEvent(SDL_Event *event);
  //
  // Make a screen dump into a file
  void DumpScreen(void);
  //
  // Implement the frequent VBI activity of the display: This should
  // update the display and read inputs
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
  //
public:
  SDL_FrontEnd(class Machine *mach,int unit);
  virtual ~SDL_FrontEnd(void);
  //  
  // Coldstart and warmstart methods for the chip class.
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Argument parser frontends
  virtual void ParseArgs(class ArgParser *args);
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Return the active buffer we must render into
  virtual UBYTE *ActiveBuffer(void);
  //  
  // Return the next output buffer for the next scanline to generate.
  // This either returns a temporary buffer to place the data in, or
  // a row in the final output buffer, depending on how the display generation
  // process works.
  virtual UBYTE *NextScanLine(void)
  {
    return InputBuffer;
  }
  //
  // The same for the RGB input buffer for true color output GTIA requires
  // for the more demanding post-processed modes.
  virtual PackedRGB *NextRGBScanLine(void)
  {
    return RGBInputBuffer;
  }
  //
  // Push the output buffer back into the display process. This signals that the
  // currently generated display line is ready.
  virtual void PushLine(UBYTE *buffer,int size);
  //
  // Ditto for the RGB/true color input.
  virtual void PushRGBLine(PackedRGB *buffer,int size);
  //
  // Signal that we start the display generation again from top. Hence, this implements
  // some kind of "vertical sync" signal for the display generation.
  virtual void ResetVertical(void);
  //
  // If we want to emulate a flickering drive LED, do it here.
  virtual void SetLED(bool on);
  //
  // Enforce a full display refresh, i.e. after the monitor returnes
  virtual void EnforceFullRefresh(void);
  //
  // Switch the SDL screen to foreground or background
  virtual void SwitchScreen(bool foreground);
  //
  // Enable or disable the mouse pointer
  virtual void ShowPointer(bool showit);
  //
  // Get some display dimensions
  virtual void BufferDimensions(LONG &leftedge,LONG &topedge,
				LONG &width,LONG &height,LONG &modulo)
  {
    leftedge = LeftEdge;
    topedge  = TopEdge;
    width    = Width;
    height   = Height;
    modulo   = FrameModulo;
  }
  //  
  // For GUI frontends within this buffer: Get the position and the status
  // of the mouse within this buffer measured in internal coordinates.
  virtual void MousePosition(LONG &x,LONG &y,bool &button); 
  //
  // Set the mouse position to the indicated coordinate
  virtual void SetMousePosition(LONG x,LONG y);
  // 
  // Return the number of lines scrolled since we asked last.
  virtual int  ScrollDistance(void);
  //
  // For GUI frontends: Check whether the mouse is available as input
  // device. This returns false in case the mouse is an input device
  // and has been grabbed on its "GamePort".
  virtual bool MouseIsAvailable(void);
  //
  // Signal the requirement for refresh in the indicated region. This is only
  // used by the GUI engine as the emulator core uses PushLine for that.
  virtual void SignalRect(LONG leftedge,LONG topedge,LONG width,LONG height);  
  //
  // Disable or enable double buffering temporarely for the user
  // menu. Default is to have it enabled.
  virtual void EnableDoubleBuffer(bool enable);
};
#endif
///

///
#endif
