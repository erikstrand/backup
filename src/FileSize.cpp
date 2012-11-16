//==============================================================================
// FileSize.cpp
// created November 15, 2012
//==============================================================================

#include "FileSize.h"
#include <sstream>

using namespace std;


//------------------------------------------------------------------------------
/*
 * Notes:
 * Should add a way to specify the width. This would allow for easy vertical
 * alignment of multiple FileSizes, as well as compact individual FileSizes.
 * This would also allow the ostringstream to be replaced with a char array.
 */

//------------------------------------------------------------------------------
// Set FileSize static members.
char const* FileSize::prefix = "kMGTPE";
char const* FileSize::digit = "0123456789";
unsigned FileSize::sigdig = 5;

//------------------------------------------------------------------------------
// Prints a filesize in the correct IEC units.
ostream& operator<< (ostream& os, FileSize const& fs) {
   FileSize::sizeType b = fs;
   ostringstream out;

   // if bytes, there is no prefix
   if (b < FileSize::sizeType(1024)) {
      // should print this number like the ones below
      out << b << "  B";  // spaces ensure that units always take 3 chars
      return os << out.str();
   }

   // scale and prefix correctly for kilo, mega, giga, and peta bytes
   long unsigned shift = 20;
   for (unsigned i=0; i<5; ++i) {
      if ((b >> shift) == 0) {
         shift -= 10;
         double scaled = static_cast<double>(b) / ((1ul << shift) * 1000ul);
         int places = 0;
         int digits = 0;
         while (static_cast<int>(scaled) == 0) {
            scaled *= 10;
            ++places;
         }
         while (places <= 3) {
            int d = static_cast<int>(scaled);
            out << FileSize::digit[d];
            scaled -= d;
            scaled *= 10;
            ++places;
            ++digits;
         }
         out << '.';
         while (digits < 5) {
            int d = static_cast<int>(scaled);
            out << FileSize::digit[d];
            scaled -= d;
            scaled *= 10;
            ++digits;
         }
         out << FileSize::prefix[i] << "iB";
         return os << out.str();
      }
      shift += 10;
   }

   // If we've made it this far then we're dealing with exbibytes.
   out << (b >> 60) << FileSize::prefix[5] << "iB";
   return os << out.str();
}

