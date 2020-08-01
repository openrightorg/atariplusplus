/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: machine.cpp,v 1.107 2015/12/11 16:27:36 thor Exp $
 **
 ** In this module: Machine/Architecture specific settings
 **********************************************************************************/

/// Includes
#include "types.h"
#include "machine.hpp"
#include "argparser.hpp"
#include "stdio.hpp"
#include "chip.hpp"
#include "new.hpp"
#include "cpu.hpp"  
#include "gtia.hpp"
#include "antic.hpp"
#include "pokey.hpp"
#include "yconnector.hpp"
#include "pia.hpp"
#include "cartctrl.hpp"
#include "mmu.hpp"
#include "printer.hpp"
#include "interfacebox.hpp"
#include "tape.hpp"
#include "diskdrive.hpp"
#include "atarisio.hpp"
#include "sound.hpp"
#include "display.hpp"
#include "ram.hpp"
#include "osrom.hpp"
#include "cartrom.hpp"
#include "osssound.hpp"
#include "osshqsound.hpp"
#include "wavsound.hpp"
#include "alsasound.hpp"
#include "directxsound.hpp"
#include "basicrom.hpp"
#include "sio.hpp"
#include "keyboard.hpp"
#include "monitor.hpp"
#include "gamecontroller.hpp"
#include "atari.hpp"
#include "vbiaction.hpp"
#include "hbiaction.hpp"
#include "x11_frontend.hpp"
#include "sdl_frontend.hpp"
#include "curses_frontend.hpp"
#include "no_frontend.hpp"
#include "menu.hpp"
#include "titlemenu.hpp"
#include "analogjoystick.hpp"
#include "digitaljoystick.hpp"
#include "sdlanalog.hpp"
#include "sdldigital.hpp"
#include "list.hpp"
#include "snapshotreader.hpp"
#include "snapshotwriter.hpp"
#include "atarisioport.hpp"
#include "sdlport.hpp"
#include "sdlsound.hpp"
#include "warningrequester.hpp"
#include "errorrequester.hpp"
#include "choicerequester.hpp"
#include "licence.hpp"
#include "sighandler.hpp"
#include "keyboardstick.hpp"
#include <stdarg.h>
///

/// Defines
#if HAVE_SDL_SDL_H && HAVE_SDL_INIT && HAVE_SDL_INITSUBSYSTEM && HAVE_SDL_SETVIDEOMODE
# define USE_SDL_VIDEO 1
#else
# define USE_SDL_VIDEO 0
#endif
///

/// Machine::Machine
// The constructor of the machine
Machine::Machine(void)
  : machtype(Mach_AtariXL), quit(false)
{ 
  int i;

#ifndef X_DISPLAY_MISSING
  fronttype      = Front_X11;
#elif  USE_SDL_VIDEO
  fronttype      = Front_SDL;
#endif 
#ifdef USE_SOUND
#if HAVE_ALSA_ASOUNDLIB_H && HAVE_SND_PCM_OPEN && HAS_PROPER_ALSA
  soundtype      = Sound_Alsa;
#else
  soundtype      = Sound_HQ;
#endif
#elif HAVE_SDL_SDL_H && defined(HAVE_DXSOUND)
  soundtype      = Sound_DirectX;
#elif HAVE_SDL_SDL_H && HAVE_SDL_OPENAUDIO
  soundtype      = Sound_SDL;
#else
  soundtype      = Sound_Wav;
#endif
  cpu            = NULL;
  gtia           = NULL;
  pokey          = NULL;
  leftpokey      = NULL;
  pokeybridge    = NULL;
  pia            = NULL;
  antic          = NULL;
  cartctrl       = NULL;
  keyboard       = NULL;
  sio            = NULL;
  sound          = NULL;
  display        = NULL;
  xepdisplay     = NULL;
  mmu            = NULL;
  monitor        = NULL;
  menu           = NULL;
  quickmenu      = NULL;
  osrom          = NULL;
  basicrom       = NULL;
  cartrom        = NULL;
  atari          = NULL;
  sioport        = NULL;
  sdlport        = NULL;
  warninglog     = NULL;
  errorlog       = NULL;
  printer        = NULL;
  tape           = NULL;
  serial         = NULL;
  globalargs     = NULL;
  keypadstick    = NULL;

  escCode        = 0;
  quit           = false;
  reset          = false;
  coldstart      = false;
  pause          = false;
  launchmonitor  = false;
  launchmenu     = false;
  monitoroncrash = false;
  acceptlicence  = false;
  stereopokey    = false;
  enablexep      = false;
  nogfx          = true;
  noerrors       = false;
  nowarnings     = false;

  for(i=0;i<4;i++) {
    joysticks[i] = NULL;
  }
  for(i=0;i<8;i++) {
    paddles[i]          = NULL;
    analogjoysticks[i]  = NULL;
    digitaljoysticks[i] = NULL;
    sdlanalog[i]        = NULL;
    sdldigital[i]       = NULL;
  }
}
///

/// Machine::~Machine
Machine::~Machine(void)
{ 
  int i;
  // clean up all classes within here
  delete errorlog;
  errorlog = NULL;
  delete warninglog;
  warninglog = NULL;
  //
  delete cpu;
  delete gtia;
  delete antic;
  delete pokeybridge;
  delete pokey;
  delete leftpokey;
  delete pia;
  delete cartctrl;
  delete mmu;
  delete ram;
  delete osrom;
  delete cartrom;
  delete basicrom;
  delete sio;   // disposes also all serial devices
  delete keyboard;
  delete atari;
  delete monitor;
  delete menu;
  delete quickmenu;
  delete keypadstick;
  //
  for(i=0;i<4;i++) {
    delete joysticks[i];
  }
  for(i=0;i<8;i++) {
    delete paddles[i];
    delete analogjoysticks[i];
    delete digitaljoysticks[i];
#if HAVE_SDL_SDL_H && HAVE_SDL_JOYSTICKOPEN
    delete sdlanalog[i];
    delete sdldigital[i];
#endif
  }
  delete lightpen;
  delete sound;
  delete xepdisplay;
  delete display;
#if HAVE_SDL_SDL_H && HAVE_SDL_INIT
  delete sdlport;
#endif
  delete sioport;
}
///

/// Machine::BuildMachine
// Allocate all components of the machine
// This happens before we configure it.
void Machine::BuildMachine(class ArgParser *args)
{
  int i;
  char devname[40];
  //
  // Keep the global options now.
  globalargs = args;
  //
  // Build up the warning requester. It might not yet work, but at least
  // we have something to refer to.
  warninglog = new class WarningRequester(this);
  errorlog   = new class ErrorRequester(this);
  //
  // Build the SDL frontend in case we have SDL available.
#if HAVE_SDL_SDL_H && HAVE_SDL_INIT
  sdlport    = new class SDL_Port;
#endif
  // Build all chips: Note that the order matters for those
  // components that implement an HBIAction because it defines
  // the calling order. We currently require antic to be called
  // first, then pokey, then gtia, then the CPU.
  ram        = new class RAM(this);
  antic      = new class Antic(this);  
  pokey      = new class Pokey(this);
  gtia       = new class GTIA(this);
  cpu        = new class CPU(this);
  cartrom    = new class CartROM(this);
  mmu        = new class MMU(this);
  osrom      = new class OsROM(this);
  atari      = new class Atari(this);
  keyboard   = new class Keyboard(this);
  monitor    = new class Monitor(this);
  menu       = new class Menu(this);
  quickmenu  = new class TitleMenu(this);
  cartctrl   = new class CartCtrl(this);
  basicrom   = new class BasicROM(this);
  sio        = new class SIO(this);
  pia        = new class PIA(this);
  sioport    = new class AtariSIOPort(this);
  //
  // Attach serial devices to SIO. The ownership then
  // goes over to the SIO module.
  sio->RegisterDevice(printer = new class Printer(this)); // keep the printer for P: emulation
  sio->RegisterDevice(new class DiskDrive(this,"Drive.1",0));
  sio->RegisterDevice(new class DiskDrive(this,"Drive.2",1));
  sio->RegisterDevice(new class DiskDrive(this,"Drive.3",2));
  sio->RegisterDevice(new class DiskDrive(this,"Drive.4",3));
  sio->RegisterDevice(serial = new class InterfaceBox(this));
  sio->RegisterDevice(new class AtariSIO(this,"AtariSIO.1",0));
  sio->RegisterDevice(new class AtariSIO(this,"AtariSIO.2",1));
  sio->RegisterDevice(new class AtariSIO(this,"AtariSIO.3",2));
  sio->RegisterDevice(new class AtariSIO(this,"AtariSIO.4",3));
  sio->RegisterDevice(tape = new class Tape(this,"Tape"));

  for(i=0;i<4;i++) {
    snprintf(devname,40,"Joystick.%d",i);
    joysticks[i] = new class GameController(this,i,devname,false);
  }
  for(i=0;i<8;i++) {
    snprintf(devname,40,"Paddle.%d",i);
    paddles[i]   = new class GameController(this,i,devname,true);
  }
  lightpen    = new class GameController(this,0,"Lightpen",false);
  keypadstick = new class KeyboardStick(this);
  //
  for(i=0;i<8;i++) {
#ifdef HAVE_LINUX_JOYSTICK_H
    digitaljoysticks[i] = new class DigitalJoystick(this,i);
    if (!digitaljoysticks[i]->IsAvailable()) {
      delete digitaljoysticks[i];
      digitaljoysticks[i] = NULL;
    }
#endif
  }
  for(i=0;i<8;i++) {
#ifdef HAVE_LINUX_JOYSTICK_H
    analogjoysticks[i]  = new class AnalogJoystick(this,i);
    if (!analogjoysticks[i]->IsAvailable()) {
      // Delete the analog joysticks that are not available here.
      delete analogjoysticks[i];
      analogjoysticks[i] = NULL;
    }
#endif
  }
  for(i=0;i<8;i++) {
#if HAVE_SDL_SDL_H && HAVE_SDL_JOYSTICKOPEN
    sdlanalog[i] = new class SDLAnalog(this,i);
    if (!sdlanalog[i]->IsAvailable()) {
      delete sdlanalog[i];
      sdlanalog[i] = NULL;
    }
#endif
  }
  for(i=0;i<8;i++) {
#if HAVE_SDL_SDL_H && HAVE_SDL_JOYSTICKOPEN
    sdldigital[i] = new class SDLDigital(this,i);
    if (!sdldigital[i]->IsAvailable()) {
      delete sdldigital[i];
      sdldigital[i] = NULL;
    }
#endif
  }
}
///

/// Machine::PutWarning
void Machine::PutWarning(const char *fmt,...)
{
  va_list fmtlist;
  va_start(fmtlist,fmt);
  VPutWarning(fmt,fmtlist);
  va_end(fmtlist);
}
///

/// Machine::VPutWarning
// Print a warning with a varargs list
void Machine::VPutWarning(const char *fmt,va_list args)
{  
  if (nowarnings == false) {
    char msg[512];
    // Print into the string
    vsnprintf(msg,511,fmt,args);    
    // Check whether we have a warning requester, and if so,
    // whether we can open it savely.
    if (warninglog) {
      if (warninglog->Request(msg)) {
	return;
      }
    }
    //
#if MUST_OPEN_CONSOLE
    OpenConsole();
#endif
    // Requester either not available, or did not open. Too bad,
    // print on stderr instead.
    fprintf(stdout,"%s",msg);
  }
}
///

/// Machine::PutError
// Display a true failure
int Machine::PutError(const class AtariException &e)
{
  //
  // If error generation is disabled, enter the menu
  // if a menu is available, otherwise start
  // the monitor or enter the monitor.
  if (noerrors) {
    // No menu to display, abort. This mode is for
    // control-less operation, thus even the menu won't
    // help.
    return ErrorRequester::ERQ_Cancel;
  }
  //
  // This is the job of the error requester should we have one.
  if (errorlog && !nogfx) {
    return errorlog->Request(e);
  }
  //
#if MUST_OPEN_CONSOLE
  OpenConsole();
#endif
  //
  /*
  if (0) {
    class ErrorPrinter : public ExceptionPrinter {
      virtual void PrintException(const char *fmt,...) 
      {
	va_list args;
	va_start(args,fmt);
	vfprintf(stderr,fmt,args);
	va_end(args);
      }
    } exc_printer;
    //
    e.PrintException(exc_printer);
  }
  */
  //
  return 0;
}
///

/// Machine::Crash
// This is called as soon as the emulator crashes by
// an unknown or unstable opcode
void Machine::Crash(UBYTE opcode)
{  
  if (monitoroncrash) {
    launchmonitor = false;
    monitor->Crash(opcode);
  } else {
    PutWarning("6502 CPU crashed at $%04x due to the unreliable opcode $%02x.",
	       cpu->PC(),opcode);
    throw AsyncEvent(AsyncEvent::Ev_EnterMenu);
  }
}
///

/// Machine::Jam
// This is called as soon as the emulator executed
// a JAM (crash and never return) opcode
void Machine::Jam(UBYTE opcode)
{  
  if (monitoroncrash) {
    launchmonitor = false;
    monitor->Jam(opcode);
  } else {
    PutWarning("6502 CPU crashed at $%04x due to the illegal opcode $%02x.",
	       cpu->PC(),opcode);
    throw AsyncEvent(AsyncEvent::Ev_EnterMenu);
  }
}
///

/// Machine::Escape
// This one is privately for the CPU: Emulate an escape code by
// going thru all patches.  
// It is called by the CPU code to respond to an ESCape / JAM code 
// to execute emulator specific instructions
void Machine::Escape(UBYTE code)
{
  if (!patchProviderChain.IsEmpty()) {
    class AdrSpace *ram     = mmu->CPURAM();
    class PatchProvider *pp = patchProviderChain.First();
    while(pp) {
      if (pp->RunEmulatorTrap(ram,cpu,code))
	return;
      pp = pp->NextOf();
    }
  }
  //
  if (monitoroncrash) {
    // Oopsi! We found an ESC code we cannot handle!
    launchmonitor = false;
    monitor->UnknownESC(code);
  } else {
    PutWarning("6502 CPU crashed at $%04x due to an invalid Escape/HALT type $%02x.",
	       cpu->PC(),code);
    throw AsyncEvent(AsyncEvent::Ev_EnterMenu);
  }
}
///

/// Machine::AlocateEscape
// Allocate N escape codes, return the next available code.
UBYTE Machine::AllocateEscape(UBYTE count)
{
  UBYTE next;

  if (int(count) + int(escCode) >= 0xff)
    Throw(OutOfRange,"Machine::AllocateEscape",
	  "trying to install too many patches, out of machine ESCape codes");

  next     = escCode;
  escCode += count;

  return next;
}
///

/// Machine::SigBreak
// Handle a ^C event in whatever way it is appropriate
void Machine::SigBreak(void)
{
  if (monitoroncrash) {
    // Expert action: Enter the monitor
    launchmonitor = true;
  } else {
    // Default action: Signal program
    // shutdown.
    quit          = true;
  }
}
///

/// Machine::EnterMonitor
// Enter the monitor by the front gate
void Machine::EnterMonitor(void)
{
  launchmonitor = false;
#ifdef BUILD_MONITOR
  monitor->EnterMonitor();
#endif
}
///

/// Machine::QuickMenu
// Run the quick menu in the title bar
void Machine::QuickMenu(void)
{
  quickmenu->EnterMenu();
}
///

/// Machine::EnterMenu
// Enter the user front end
void Machine::EnterMenu(void)
{  
  launchmenu = false;
  if (!nogfx)
    menu->EnterMenu();
}
///

/// Machine::ParseConfig
// Parse the configuration options for the overall menu here.
void Machine::ParseConfig(class ArgParser *args)
{ 
  bool xep;
  static const struct ArgParser::SelectionVector machinevector[] = 
    { {"800"    ,Mach_Atari800 },
      {"1200"   ,Mach_Atari1200},
      {"XL"     ,Mach_AtariXL  },
      {"XE"     ,Mach_AtariXE  },
      {"5200"   ,Mach_5200     },
      {NULL ,0}
    };     
  static const struct ArgParser::SelectionVector frontvector[] =
    { 
#ifndef X_DISPLAY_MISSING
      {"X11",    Front_X11 },
#endif
#if USE_SDL_VIDEO
      {"SDL",    Front_SDL },
#endif
#ifdef USE_CURSES
      {"Curses", Front_Curses },
#endif
      {"None",   Front_None },
      {NULL , 0}
    };
  static const struct ArgParser::SelectionVector soundvector[] =
    {
#if HAVE_ALSA_ASOUNDLIB_H && HAVE_SND_PCM_OPEN && HAS_PROPER_ALSA
      {"Alsa"   , Sound_Alsa },
#endif
#ifdef USE_SOUND
      {"HQOss"  , Sound_HQ   },
      {"Oss"    , Sound_Oss  },
#endif
#if HAVE_SDL_SDL_H && defined(HAVE_DXSOUND)
      {"DirectX", Sound_DirectX},
#endif
      {"Wav"    , Sound_Wav  },
#if HAVE_SDL_SDL_H && HAVE_SDL_INIT && HAVE_SDL_INITSUBSYSTEM && HAVE_SDL_OPENAUDIO
      {"SDL"    , Sound_SDL  },
#endif
      {NULL     , 0},
    };
  LONG mach,front,snd;
  //
  //
  mach  = machtype;
  front = fronttype;
  snd   = soundtype;
  args->DefineTitle("Machine"); 
  args->OpenSubItem("Machine"); // also into the title menu
  args->DefineSelection("Machine",
			"set architecture to Atari800, 800XL, 130XE or 5200",
			machinevector,mach);
  args->DefineSelection("FrontEnd",
			"set graphical front end to "
#ifndef X_DISPLAY_MISSING
			"X Window System "
#endif
#if USE_SDL_VIDEO
# if !defined(X_DISPLAY_MISSING)	
			"or "
# endif
			"Simple Direct Media"
#endif
#ifdef USE_CURSES
# if !defined(X_DISPLAY_MISSING) || USE_SDL_VIDEO
			"or "
# endif
			"Curses Terminal Output"
#endif

			"",
			frontvector,front);

  args->DefineSelection("Sound",
			"set sound front end to "
#ifdef USE_SOUND
			"Oss sound driver or "
#endif
#if HAVE_SDL_SDL_H && HAVE_SDL_INIT && HAVE_SDL_INITSUBSYSTEM && HAVE_SDL_OPENAUDIO
			"SDL sound or "
#endif
			".wav sample output",
			soundvector,snd);

  xep = enablexep;
#if 0
  args->DefineBool("EnableXEP",
		   "enable 80 character display device",enablexep);
#endif

  if (machtype != (Machine_Type)mach || xep != enablexep) {
    args->SignalBigChange(ArgParser::ColdStart);
  }
  machtype  = (Machine_Type)mach;
  //
  //
  //
  // Check for the dual pokey configuration. If set, we need to build a pokey bridge and
  // a second pokey.
  args->DefineBool("StereoPokey","emulate dual pokey stereo extension",stereopokey);
  //
  // If we have a stereo pokey setup, build the second pokey now.
  // If the configuration changed noticably because we have to change the
  // mapping, we need to run thru a coldstart.
  if (stereopokey) {
    bool coldstart = false;
    //
    if (leftpokey == NULL) {
      // Build the left pokey channel as unit one.
      leftpokey    = new class Pokey(this,1);
      leftpokey->ColdStart();
      coldstart    = true;
    }
    // Build the pokey-pokey bridge that links the two into the page 0xd200.
    if (pokeybridge == NULL) {
      // The pokeys are distinguished by bit 4 
      pokeybridge  = new class YConnector(0x0010);
      coldstart    = true;
    } 
    //
    // Link the two pokeys. The right one goes into
    // bit 0, the left (first unit) into bit 1.
    pokeybridge->ConnectPage(pokey,0xd200);
    pokeybridge->ConnectPage(leftpokey,0xd210);
    //
    if (coldstart && mmu)
      mmu->Initialize();
  } else {
    bool coldstart = false;
    //
    // Cleanup, stereo patch is no longer required.
    if (pokeybridge) {
      delete pokeybridge;
      pokeybridge  = NULL;
      coldstart    = true;
    }
    if (leftpokey) {
      delete leftpokey;
      leftpokey    = NULL;
      coldstart    = true;
    }
    //
    // Run into a coldstart now.
    if (coldstart && mmu)
      mmu->Initialize();
  }
  //
  // Check whether the frontend changed. If so, release the old frontends and
  // update.
  if (fronttype != (FrontEnd_Type)front) {
    nogfx      = true;
    delete display;
    display    = NULL;
    delete xepdisplay;
    xepdisplay = NULL;
    fronttype  = (FrontEnd_Type)front;
    args->SignalBigChange(ArgParser::Reparse);
  }
  //
  // Now check the sound frontend
  if (soundtype != (Sound_Type)snd) {
    delete sound;
    sound     = NULL;
    soundtype = (Sound_Type)snd;
    args->SignalBigChange(ArgParser::Reparse);
  }
  //
  //
  //
  // Now build the missing front-ends: First for display.
  if (display == NULL) {
    switch(fronttype) {
    case Front_X11:
#ifndef X_DISPLAY_MISSING
      display = new class X11_FrontEnd(this,0);
      nogfx   = false;
#else
      display = NULL;
#endif
      break;
    case Front_SDL:
#if USE_SDL_VIDEO
      display = new class SDL_FrontEnd(this,0);
      nogfx   = false;
#else
      display    = NULL;
#endif
      break;
    case Front_Curses:
#if USE_CURSES
      display = new class Curses_FrontEnd(this);
      nogfx   = true;
#else
      display    = NULL;
#endif
      break;
    case Front_None:
      display = new class No_FrontEnd(this);
      nogfx   = true;
      break;
    }
  }
  if (enablexep && xepdisplay == NULL) {
    switch(fronttype) {
    case Front_X11:
#ifndef X_DISPLAY_MISSING
      xepdisplay = new class X11_FrontEnd(this,1);
#else
      xepdisplay = NULL;
#endif
      break;
    case Front_SDL:
#if USE_SDL_VIDEO
      xepdisplay = new class SDL_FrontEnd(this,1);
#else
      xepdisplay = NULL;
#endif
      break; 
    case Front_Curses:
    case Front_None:
      xepdisplay = NULL;
      break;
    }
  } else {
    delete xepdisplay;
    xepdisplay = NULL;
  }
  //
  //
  // Now build the sound generation frontend.
  if (sound   == NULL) {
    switch(soundtype) {
    case Sound_HQ:
#ifdef USE_SOUND
      sound = new class HQSound(this);
#else
      sound = NULL;
#endif
      break;
    case Sound_Oss:
#ifdef USE_SOUND
      sound = new class OssSound(this);
#else
      sound = NULL;
#endif
      break;
    case Sound_Wav:
      sound = new class WavSound(this);
      break;
    case Sound_SDL:
#if HAVE_SDL_SDL_H && HAVE_SDL_INIT && HAVE_SDL_INITSUBSYSTEM && HAVE_SDL_OPENAUDIO
      sound = new class SDLSound(this);
#else
      sound = NULL;
#endif
      break;
    case Sound_Alsa:
#if HAVE_ALSA_ASOUNDLIB_H && HAVE_SND_PCM_OPEN && HAS_PROPER_ALSA
      sound = new class AlsaSound(this);
#else
      sound = NULL;
#endif
      break;
    case Sound_DirectX:
#if HAVE_SDL_SDL_H && defined(HAVE_DXSOUND)
      sound = new class DirectXSound(this);
#else
      sound = NULL;
#endif
    }
  }
  //
  //
  if (display == NULL) {
    nogfx = true;
    Throw(ObjectDoesntExist,"Machine::ParseArgs",
	  "unable to build a suitable frontend. Either LibX11, SDL or Curses must be available");
  }
  if (sound   == NULL) {
    Throw(ObjectDoesntExist,"Machine::ParseArgs",
	  "unable to build a suitable sound generation core. Either Oss or .wav output must be available");
  }
  //
#ifdef BUILD_MONITOR
  // Check whether we should run the monitor on crash. This is a simple boolean option.
  args->DefineBool("MonitorOnCrash","enter the built-in system monitor on a crash",monitoroncrash);
#else
  monitoroncrash = false;
#endif
  //
  // Get the error and warning settings.
  args->DefineBool("IgnoreErrors","ignore error conditions and enter the menu on error",noerrors);
  args->DefineBool("IgnoreWarnings","ignore warnings and resume emulation on warnings",nowarnings);
  //
  // Check whether we should always accept the licence (and avoid showing it in first place)
  args->DefineBool("AcceptLicence","always accept the licence conditions and avoid showing them on startup",
		   acceptlicence);

  //
  args->CloseSubItem();
}
///

/// Machine::ParseArgs
// Parse arguments from the command line to 
// get the machine type we are emulating.
// If we call this with NULL, use the global arguments
ArgParser::ArgumentChange Machine::ParseArgs(class ArgParser *args)
{   
  class AtariException ex;
  bool errored = false;
  class Configurable *config;
  //
  // If we don't have any arguments, use the global ones here.
  // This allows easily to re-install these arguments as defaults
  // later on. Important for the menu.
  if (args == NULL)
    args = globalargs;  
  //
  ParseConfig(args);
  //
  // Now configure all remaining configurables
  for(config = configChain.First();config;config = config->NextOf()) {
    try {
      config->ParseArgs(args);
    } catch(AtariException &e) {
      // On error, reparse to the very end before throwing an exception.
      if (!errored) {
	ex      = e;
	errored = true;
      }
    }
  }
  //
  // Throw an error at the very end.
  if (errored)
    throw ex;
  //
  // Now return the reparse state
  return args->ReparseState();
}
///

/// Machine::WriteStates
// Write all available state definitions into a named file
// throw in case of an error
void Machine::WriteStates(const char *file)
{
  class SnapShotWriter *snw;
  class Saveable *sobj;

  // Iterate thru the list of snapshot objects and
  // let them write the contents into the target file.
  snw = new class SnapShotWriter;
  try {
    snw->OpenFile(file);
    sobj = snapshotChain.First();
    while(sobj) {
      sobj->State(snw);
      sobj = sobj->NextOf();
    }
  } catch(...) {
    // Forward the error, but dispose this temporary object
    // first.
    delete snw;
    throw;
  }
  snw->CloseFile();
  delete snw;
}
///

/// Machine::ReadStates
// Read the state machine from a file, throw in case of an error.
void Machine::ReadStates(const char *file)
{
  class SnapShotReader *snr;
  class Saveable *sobj;
  
  // Iterate thru the list of snapshot objects and
  // parse off the data that belongs to the snapshot file.
  snr = new class SnapShotReader;
  try {
    // First step: Collect the data items we need to read.
    snr->OpenFile(file);
    sobj = snapshotChain.First();
    while(sobj) {
      sobj->State(snr);
      sobj = sobj->NextOf();
    }
    // Second step: Collect the data
    snr->Parse();
    sobj = snapshotChain.First();
    while(sobj) {
      sobj->State(snr);
      sobj = sobj->NextOf();
    }
  } catch(...) {
    delete snr;
    throw;
  }
  snr->CloseFile();
  delete snr;
}
///

/// Machine::WarmReset
// Issue a keyboard click on the "warm reset" button. Depending on the model,
// various things could happen as the Atari800 and Atari400 handle these keys
// quite different.
void Machine::WarmReset(void)
{
  switch(machtype) {
  case Mach_Atari800:
    // Here, a keyboard reset enters antic by its NMI input.
    antic->ResetNMI();
    break;
  case Mach_AtariXL:
  case Mach_AtariXE:
  case Mach_Atari1200:
  case Mach_5200:
    // Here, a reset goes directly to the CPU.
    Reset() = true;
    break;
  default:
#if CHECK_LEVEL > 0
    Throw(InvalidParameter,"Machine::WarmReset",
	  "Machine type is invalid");
#endif
    break;
  }
}
///

/// Machine::WarmStart
// Run a system wide warmstart on all registered chips
void Machine::WarmStart(void)
{
  class Chip *chip;

  quit     = false;
  reset    = false;
  coldstart= false;
  pause    = false;

  chip = chipChain.First();
  while(chip) {
    chip->WarmStart();
    chip = chip->NextOf();
  }
}
///

/// Machine::ColdStart
// Run a system coldstart over all registered chips,
// ditto an initialization of the MemControllers.
void Machine::ColdStart(void)
{
  class Chip *chip;
  //
  quit     = false;
  reset    = false;
  coldstart= false;
  pause    = false;
  escCode  = 0;
  //   
  SigHandler::RestoreCoreDump();
  // Prep all mapping
  mmu->Initialize();
  // Prep and load Os ROM
  osrom->Initialize();
  // Prep and load Basic
  basicrom->Initialize();
  // Prep and load carts
  cartrom->Initialize();

  chip = chipChain.First();
  while(chip) {
    chip->ColdStart();
    chip = chip->NextOf();
  }
}
///

/// Machine::PokeyPage
// If we have two pokeys, they cannot get
// mapped into memory directly, but are rather
// connected by a "bridge" that distinguishes
// which access goes to which pokey. This call
// returns for the MMU the page that should go
// into the pokey area.
class Page *Machine::PokeyPage(void) const
{
  if (stereopokey) {
    return pokeybridge;
  } else {
    return pokey;
  }
}
///

/// Machine::ScaleFrequency
// Scale the given frequency for the selected refresh rate if
// a custom refresh rate has been selected. This is required for
// proper audio generation.
int Machine::ScaleFrequency(int freq)
{
  return atari->ScaleFrequency(freq);
}
///

/// Machine::RefreshDisplay
// Rebuild the display manually from the user interface
void Machine::RefreshDisplay(void)
{ 
  if (!nogfx) {
    class Timer dummy;
    
    dummy.StartTimer(0,0);  
    Display()->EnforceFullRefresh();
    VBI(&dummy,false,true);
  }
}
///

/// Machine::isNTSC
// Return true in case this is an NTSC machine.
bool Machine::isNTSC(void) const
{
  if (atari)
    return atari->isNTSC();
  return false;
}
///
