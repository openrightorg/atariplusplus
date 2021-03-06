/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartoss.hpp,v 1.9 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of an Oss supercart
 **********************************************************************************/

#ifndef CARTOSS_HPP
#define CARTOSS_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class CartOSS
// The CartOSS class implements an Oss supercartridge.
class CartOSS : public Cartridge {
  //
  // The contents of the cart
  class RomPage Rom[64];
  //
  // a completely empty page
  class RomPage Blank;
  //
  // Flag of the currently active bank.
  UBYTE         ActiveBank;
  // Flag whether this cart has been disabled.
  bool          Disabled;
  //
public:
  CartOSS(void);
  virtual ~CartOSS(void);
  //
  // This static array contains the possible cart sizes
  // for this cart type.
  static const UWORD CartSizes[];
  //
  // Return a string identifying the type of the cartridge.
  virtual const char *CartType(void);
  //
  // Read the contents of this cart from an open file. Headers and other
  // mess has been skipped already here. Throws on failure.
  virtual void ReadFromFile(FILE *fp);
  //
  // Remap this cart into the address spaces by using the MMU class.
  // It must know its settings itself, but returns false if it is not
  // mapped. Then the MMU has to decide what to do about it.
  virtual bool MapCart(class MMU *mmu);
  //  
  // Initialize this memory controller, built its contents.
  virtual void Initialize(void);
  //
  // Perform a write into the CartCtrl area, possibly modifying the mapping.
  // This never expects a WSYNC.
  virtual bool ComplexWrite(class MMU *mmu,ADR mem,UBYTE val);
  //
  // Test whether this cart is "available" in the sense that the CartCtl
  // line TRIG3 is pulled. Default result is "yes".
  virtual bool IsMapped(void)
  {
    return !Disabled;
  }
  //
  // Display the status over the monitor.
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Perform the snapshot operation for the CartCtrl unit.
  virtual void State(class SnapShot *snap);
};
///

///
#endif
