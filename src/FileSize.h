//==============================================================================
// FileSize.h
// created November 15, 2012
//==============================================================================

#include <iostream>


//------------------------------------------------------------------------------
// A filesize in the range [0B and 16EiB).
// Note on units: I use IEC units (kibibyte = 2^10B, mebibyte = 2^20B, ect.) throughout.
// These should not be confused with the SI units (kilobyte = 10^3B, megabyte = 10^6B, etc.).
struct FileSize {
   typedef long unsigned sizeType;
   static char const* prefix;
   static char const* digit;
   static unsigned sigdig;
   static unsigned streamWidth () { return sigdig + 4; }

   sizeType bytes; // need 64 bits for files larger than 4GiB

   // Constructors
   FileSize () {}
   FileSize (unsigned b): bytes(b) {}
   FileSize (long unsigned b): bytes(b) {}
   FileSize (int b): bytes(b) {}
   FileSize (long int b): bytes(b) {}

   // Arithmetic
   FileSize& operator=  (FileSize rhs) { bytes = rhs.bytes; return *this; }
   FileSize  operator+  (FileSize rhs) { return FileSize(bytes + rhs.bytes); }
   FileSize& operator+= (FileSize rhs) { bytes += rhs.bytes; return *this; }

   // Conversions
   operator long unsigned () const { return bytes; }
   operator float () const { return static_cast<float>(bytes); }
};


//------------------------------------------------------------------------------
// Prints a filesize in the correct IEC units.
std::ostream& operator<< (std::ostream& os, FileSize const& fs);

