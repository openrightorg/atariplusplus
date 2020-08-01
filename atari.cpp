/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atari.cpp,v 1.38 2014/01/08 13:09:53 thor Exp $
 **
 ** In this module: Main loop of the emulator
 **********************************************************************************/

/// Includes
#include "atari.hpp"
#include "machine.hpp"
#include "timer.hpp"
#include "display.hpp"
#include "monitor.hpp"
#include "argparser.hpp"
#include "antic.hpp"
#include "sighandler.hpp"
#include "errorrequester.hpp"
#include "choicerequester.hpp"
#include "new.hpp"
///

/// Atari::Atari
// The constructor of this main class
Atari::Atari(class Machine *mach)
  : Chip(mach,"Atari"), VBITimer(NULL), Display(NULL), YesNoRequester(NULL),
    NTSC(false), MaxMiss(1), CustomRate(false), RefreshRate(20)
{
}
///

/// Atari::~Atari
Atari::~Atari(void)
{
  delete VBITimer;
  delete YesNoRequester;
}
///

/// Atari::ColdStart
// The coldstart vector. This only fetches some important
// classes from the machine
void Atari::ColdStart(void)
{
  Display  = machine->Display();
  Antic    = machine->Antic();
  if (VBITimer       == NULL)
    VBITimer       = new class Timer;
  if (YesNoRequester == NULL)
    YesNoRequester = new class ChoiceRequester(machine);
}
///

/// Atari::WarmStart
// The warmstart vector. Nothing happens here.
void Atari::WarmStart(void)
{
}
///

/// Atari::RefreshDelay
// Compute the refresh rate in microseconds.
LONG Atari::RefreshDelay(void)
{ 
  // Check for custom vs. adjusted refresh rate.
  if (CustomRate) {
    return RefreshRate * 1000;
  } else {
    return Timer::UsecsPerSec/((NTSC)?(60):(50));
  }
}
///

/// Atari::ScaleFrequency
// Scale the given frequency to the current base frequency.
int Atari::ScaleFrequency(int freq)
{
  if (CustomRate) {
    LONG nominal  = Timer::UsecsPerSec/((NTSC)?(60):(50)); // nominal refresh rate
    LONG realrate = 1000 * RefreshRate;                    // the rate we really got.
    return freq * nominal / realrate;
  } else {
    return freq;
  }
}
///

/// Atari::EmulationLoop
// The main loop of the emulator: The main loop of the emulator
void Atari::EmulationLoop(void)
{
  class SigHandler sigh(machine); // install our signal handler. 
  // The signal handler gets removed on exit by destructor
  //
  LONG usecs;
  LONG missedframes;
  bool redo = false;
  //
  // Compute the refresh rate as the VBI delay in micro seconds
  usecs = RefreshDelay();
  VBITimer->StartTimer(0,usecs);
  missedframes = 0;
  machine->Display()->ShowPointer(false);
  //
  // Loop over captured async events we must handle
  //
  do {
    try {
      //
      // Run the emulation loop now.
      //
      while(!machine->Quit()) {
	bool events;
	// Check some machine conditions and act accordingly
	do {
	  // Continue execution of the major events until we
	  // come to a point where nothing else happens.
	  events = false;
	  if (machine->ColdReset()) {
	    machine->ColdStart();
	    events = true;
	  }
	  if (machine->Reset()) {
	    machine->WarmStart();
	    events = true;
	  }
	  if (machine->LaunchMonitor()) {	
	    // Sync the CPU state to an instruction boundary.
	    machine->CPU()->Sync();
	    //
	    machine->EnterMonitor();	  
	    VBITimer->StartTimer(0,usecs);
	    missedframes = 0;
	    events = true;
	  }
	  if (machine->LaunchMenu()) {	  
	    // Sync the CPU state to an instruction boundary.
	    machine->CPU()->Sync();
	    //
	    machine->EnterMenu();
	    // After a menu, the display might have changed. *Sigh*
	    Display = machine->Display();  
	    // And so did the refresh counter.
	    usecs   = RefreshDelay();
	    VBITimer->StartTimer(0,usecs);
	    missedframes = 0;
	    events  = true;
	  }
	  if (Display->MenuVerify()) {
	    // Sync the CPU state to an instruction boundary.
	    machine->CPU()->Sync();
	    machine->QuickMenu();
	    // The display might have changed now, reload some internals.	    
	    Display = machine->Display();  
	    // And so did the refresh counter.
	    usecs   = RefreshDelay();
	    VBITimer->StartTimer(0,usecs);
	    missedframes = 0;
	    events  = true;
	  }
	} while(events);
	//
	if (machine->Quit() == false) {
	  if (machine->Pause() == false) {
	    //
	    // Now use ANTIC to generate the display
	    //
	    Antic->RunDisplayList();
	  }
	  //
	  // Now check whether we run out of time for this refresh
	  //
	  if (!VBITimer->EventIsOver() || missedframes >= MaxMiss) {
	    // Ok, we either still have time to, or we need to 
	    // generate the display. This also drives all other
	    // frequent activity.
	    machine->VBI(VBITimer,false);
	    //
	    // If we run out of time because we missed too many frames,
	    // then we need to get a new time base.
	    if (missedframes >= MaxMiss) {
	      VBITimer->StartTimer(0,usecs);
	      missedframes = 0;
	    }
	  } else {
	    // Ooops, the time run out when generating the system. Note
	    // that we missed this frame and possibly try again. Drop
	    // this frame, do not run over X.
	    missedframes++;
	    //
	    // Better do quick here as we are late on schedule!
	    machine->VBI(VBITimer,true);
	  }
	  //
	  // Tell the timer that one cycle finished.
	  VBITimer->TriggerNextEvent();
	}
      }
      //
      // Collect asyncronous events that kill the machine immediately.
    } catch(const AsyncEvent &e) {
      switch(e.TypeOf()) {
      case AsyncEvent::Ev_Exit:
	// The user wants to leave the emulator as quick as possible.
	redo = false;
	break;
      case AsyncEvent::Ev_WarmStart:
	// Initiate a warmstart
	machine->WarmStart();
	redo = true;
	break;
      case AsyncEvent::Ev_ColdStart:
	// Initiate a coldstart
	machine->ColdStart();
	redo = true;
	break;
      case AsyncEvent::Ev_EnterMenu:
	machine->LaunchMenu() = true;
	redo = true;
	break;
      }
    } catch(const AtariException &e) {
      // Another kind of exception we need to handle: This is a true system exception
      if (e.TypeOf() == AtariException::Ex_NoMem) {
	// If we failed due to low memory, we cannot hope to recover as all the exception
	// handling could hope to archive would be another low-memory exception. Re-throw
	// this exception and leave it to main to catch it and print it to stdout.
	throw;
      }
      // Otherwise, leave it to the machine to build up an error requester.
      switch(machine->PutError(e)) {
      case ErrorRequester::ERQ_Nothing:
	// Building the requester failed. Re-throw the exception and leave it to
	// main to print it to stderr.
	throw;
      case ErrorRequester::ERQ_Cancel:
	// Abort the program flow. Same as above.
	throw;
      case ErrorRequester::ERQ_Retry:
	// Re-loop the main loop, better luck next time.
	redo    = true;
	machine->VBI(VBITimer,false);
	Display = machine->Display();  
	break;
      case ErrorRequester::ERQ_Monitor:
	// Run the monitor here.
	redo = true;
	// Actually, we should try to sync the CPU,
	// but in case something crashed, chances are bad
	// that we can...
	machine->LaunchMonitor() = true;
	Display = machine->Display();  
	break;
      case ErrorRequester::ERQ_Menu:
	// Enter the configuration menu.
	redo = true;
	machine->LaunchMenu() = true;
	Display = machine->Display();  
	break;
      }
    }
    if (machine->Quit()) {
      redo = false;
    }
  } while(redo);
}
///

/// Atari::ParseArgs
// Parse off machine specific arguments from the
// argument parser.
void Atari::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector videovector[] = 
    { {"PAL" ,false},
      {"NTSC",true },
      {NULL ,0}
    };
  LONG value;
  bool customref = CustomRate;

  value = NTSC;
  args->DefineTitle("Speed");
  args->DefineLong("MaxMiss","set maximum number of missed frames",1,100,MaxMiss);
  args->DefineSelection("VideoMode","set the video mode",videovector,value);
  NTSC = (value)?(true):(false);
  args->DefineBool("UnlockRate","don't lock the refresh rate to the video emulation mode",customref);
  if (customref != CustomRate) {
    CustomRate = customref;
    // Enforce a reparsing since another option pops up if the refresh rate is unlocked
    args->SignalBigChange(ArgParser::Reparse);
  }
  if (CustomRate) {
    args->DefineLong("FrameRate","set the screen refresh rate in milliseconds",1,100,RefreshRate);
  }
}
///

/// Atari::DisplayStatus
// Display the status of the atari emulator. 
// Currently, this is only the data read from the command line
// Future versions could offer here information about the
// frame rate and other statistics.
void Atari::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Speed Status:\n"
		   "\tRefreshRate is : %s\n"
		   "\tFrameRate      : " LD "\n"
		   "\tMaxMiss        : " LD "\n"
		   "\tVideoMode      : %s\n",
		   (CustomRate)?("unlocked"):("locked"),
		   RefreshRate,MaxMiss,
		   (NTSC)?("NTSC"):("PAL"));
}
///
