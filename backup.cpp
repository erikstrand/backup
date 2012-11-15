//==============================================================================
// backup.cpp
// created November 12, 2012
//==============================================================================

#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost::filesystem;


//==============================================================================
// Utilities
//==============================================================================

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
   // Note: if you're transferring exbibytes of data, don't use this utility.
   out << (b >> 60) << FileSize::prefix[5] << "iB";
   return os << out.str();
}


//------------------------------------------------------------------------------
struct CopyStatus {
   static const unsigned fsw = 9;

   FileSize bytes;
   FileSize totalBytes;
   FileSize fileBytes;
   FileSize fileTotal;
   //unsigned files;
   unsigned totalFiles;
   path srcPath;
   path dstPath;
   path dspPath;

   CopyStatus (): bytes(0), totalBytes(0), fileBytes(0), totalFiles(0) {}
};

//------------------------------------------------------------------------------
ostream& operator<< (ostream& os, CopyStatus const& s) {
   return os << setw(s.fsw) << s.bytes << '/' << setw(s.fsw) << s.totalBytes << " | ";
}
   
//------------------------------------------------------------------------------
struct FileCopier {
public:
   char buf[BUFSIZ];
   unsigned bufs_per_update;
   unsigned fsw;
   CopyStatus status;

public:
   FileCopier (): bufs_per_update(512000), fsw(9) {}
   void startBatch (unsigned nFiles, FileSize nBytes);
   void copy (path const& srcpath, path const& dstpath, path const& dsppath);
   void copy (path const& srcpath, path const& dstpath) { copy(srcpath, dstpath, srcpath); }

private:
   void printStart  (CopyStatus const& s) const;
   void printUpdate (CopyStatus const& s) const;
};

//------------------------------------------------------------------------------
void FileCopier::startBatch (unsigned nFiles, FileSize nBytes) {
   status.bytes = 0;
   status.totalBytes = nBytes;
   status.fileBytes = 0;
   status.totalFiles = nFiles;
}

//------------------------------------------------------------------------------
void FileCopier::copy (path const& srcpath, path const& dstpath, path const& dsppath) {
   // declare variables
   long unsigned buf_count = 0;
   long unsigned buf_trigger = bufs_per_update;
   FileSize initialBytes = status.bytes;

   // update status
   status.fileTotal = file_size(srcpath);
   status.srcPath = srcpath;
   status.dstPath = dstpath;
   status.dspPath = dsppath;

   // open files
   ifstream src(srcpath.c_str());
   //ofstream dst(dstpath.c_str(), ios_base::out | ios_base::binary);

   printStart(status);
   while (src) {
      if (buf_count++ == buf_trigger) {
         buf_trigger += bufs_per_update;
         FileSize newbytes = bufs_per_update * BUFSIZ;
         status.bytes += newbytes;
         status.fileBytes += newbytes;
         printUpdate(status);
      }

      src.read(buf, BUFSIZ);
      //dst.write(buf, src.gcount());
   }

   src.close();
   //dst.close();
   status.bytes = initialBytes + status.fileTotal;
}

//------------------------------------------------------------------------------
void FileCopier::printStart (CopyStatus const& s) const {
   cout << s << "Copying " << s.dspPath << " (" << s.fileTotal << ')' << '\n';
}

//------------------------------------------------------------------------------
void FileCopier::printUpdate (CopyStatus const& s) const {
   cout << s << "..." << s.dspPath << " (" << s.fileBytes << '/' << s.fileTotal << ')' << '\n';
}


//==============================================================================
// Modified Vectors (FileVector, DirVector, and FDPair)
//==============================================================================

//------------------------------------------------------------------------------
// A vector of files that keeps track of the combined size of its contents.
class FileVector : public vector<path> {
protected:
   FileSize _bytes;

public:
   FileVector (): _bytes(0) {}

   void push_back (path const& p, path const& full) {
      _bytes += file_size(full);
      vector<path>::push_back(p);
   }

   template <typename Func>
   void push_back (path const& p, Func grounder) {
      push_back(p, grounder(p));
   }

   void clear () {
      _bytes = 0;
      vector<path>::clear();
   }

   unsigned files () const { return vector<path>::size(); }
   FileSize bytes () const { return _bytes; }

// this method has no use in FileVector, so we're making it inaccessible
private:
   void push_back (path const& p) {}
};

//------------------------------------------------------------------------------
// A vector of directories that will tally the number and total size of all
// children of its contents.
class DirVector : public FileVector {
private:
   unsigned _files;

public:
   DirVector (): _files(0) {}

   void push_back (path const& p) { vector<path>::push_back(p); }
   template <typename Func> void annotate (Func grounder);
   unsigned files () const { return _files; }

// these methods have no use in DirVector, so we're making them inaccessible
private:
   void push_back (path const& p, path const& full) {}
   template <typename Func> void push_back (path const& p, Func grounder) {} 
};

//------------------------------------------------------------------------------
// Tallies up the number of files and bytes of children of the DirVector's contents.
template <typename Func>
void DirVector::annotate (Func grounder) {
   _files = 0;
   _bytes = 0;
   for (unsigned i=0; i<size(); ++i) {
      recursive_directory_iterator itr(grounder(FileVector::operator[](i)));
      recursive_directory_iterator end;
      while (itr != end) {
         if (is_regular_file(itr->path())) {
            ++_files;
            _bytes += file_size(itr->path());
         }
         ++itr;
      }
   }
}


//------------------------------------------------------------------------------
// A FileVector and DirVector working together.
struct FDPair {
   FileVector f;
   DirVector  d;

   void add (path const& p, path const& fullPath) {
      if (is_regular_file(fullPath)) {
         f.push_back(p, fullPath);
      } else if (is_directory(fullPath)) {
         d.push_back(p);
      }
   }
   template <typename Func> void add (path const& p, Func grounder) { add(p, grounder(p)); }

   template <typename Func> void annotate (Func grounder) { d.annotate(grounder); }

   unsigned ffiles () const { return f.files(); }
   FileSize fbytes () const { return f.bytes(); }
   unsigned dfiles () const { return d.files(); }
   FileSize dbytes () const { return d.bytes(); }
   unsigned files  () const { return ffiles() + dfiles(); }
   FileSize bytes  () const { return fbytes() + dbytes(); }

   void fprint () const;
   void dprint () const;
   void print () const;
};

//------------------------------------------------------------------------------
void FDPair::fprint () const {
   cout << f.size() << " files totaling " << fbytes() << '.' << '\n';
   for (unsigned i=0; i<f.size(); ++i) {
      cout << f[i] << '\n';
   }
   cout << '\n';
}

//------------------------------------------------------------------------------
void FDPair::dprint () const {
   cout << d.size() << " directories, containing " << dfiles() << " files (" << dbytes() << ")." << '\n';
   for (unsigned i=0; i<d.size(); ++i) {
      cout << d[i] << '\n';
   }
   cout << '\n';
}

//------------------------------------------------------------------------------
void FDPair::print () const {
   fprint();
   dprint();
}


//==============================================================================
// DirectoryComparer
//==============================================================================

//------------------------------------------------------------------------------
class DirectoryComparer {
private:
   path _p[2];
   path _extension;

   FDPair _uc[2];    // files and directories unique to dir1 and dir2
   FDPair _sc;       // shared files and directories

   vector<path> _sizeIssues; // shared files with different sizes
   vector<path> _fdIssues;   // shared paths with file / directory mismatch

   vector<path> _temp1;
   vector<path> _temp2;

   bool annotated = false;

   bool ignore_hidden_files = true;

public:
   DirectoryComparer (): _extension("") {}
   void setPaths (path const& p0, path const& p1) {
      _p[0] = p0;
      _p[1] = p1;
   }

   void summary ();
   void status (bool p0, bool p1, bool ps, bool pi);
   void backup ();

private: 
   path workingPath (unsigned n)                const { return _p[n] / _extension; }
   path relPath     (path const& p)             const { return _extension / p; }
   path fullPath    (path const& p, unsigned n) const { return _p[n] / _extension / p; }
   path groundPath  (path const& e, unsigned n) const { return _p[n] / e; }

   void compare ();
   void recursiveCompare ();
   void annotate0      () { _uc[0].d.annotate([this] (path const& p) { return groundPath(p, 0); }); }
   void annotate1      () { _uc[1].d.annotate([this] (path const& p) { return groundPath(p, 1); }); }
   void annotateShared () { _sc.d.annotate([this] (path const& p) { return groundPath(p, 0); }); }
   void copy ();

   void print0       () const;
   void print1       () const;
   void printShared  () const;
   void printIssues  () const;
   void printSummary () const;
};

//------------------------------------------------------------------------------
void DirectoryComparer::summary () {
   recursiveCompare();
   annotate0();
   annotate1();
   annotateShared();
   printSummary();
}

//------------------------------------------------------------------------------
void DirectoryComparer::status (bool p0, bool p1, bool ps, bool pi) {
   recursiveCompare();
   if (p0) {
      annotate0();
      print0();
   }
   if (p1) {
      annotate1();
      print1();
   }
   if (ps) {
      annotateShared();
      printShared();
   }
   if (pi) {
      printIssues();
   }
}

//------------------------------------------------------------------------------
void DirectoryComparer::backup () {
   recursiveCompare();
   copy();
}

//------------------------------------------------------------------------------
void DirectoryComparer::compare () {
   // clear temp vecs and reset annotation
   annotated = false;
   _temp1.clear();
   _temp2.clear();

   // fill temp vecs
   directory_iterator end;
   directory_iterator itr;
   for (itr = directory_iterator(workingPath(0)); itr != end; ++itr) {
      if ( ( is_regular_file(itr->path()) || is_directory(itr->path()) ) &&
           ( ignore_hidden_files && (itr->path().filename().native()[0] != '.') ) ) {
         _temp1.push_back(itr->path().filename());
      }
   }
   for (itr = directory_iterator(workingPath(1)); itr != end; ++itr) {
      if ( ( is_regular_file(itr->path()) || is_directory(itr->path()) ) &&
           ( ignore_hidden_files && (itr->path().filename().native()[0] != '.') ) ) {
         _temp2.push_back(itr->path().filename());
      }
   }

   // sort temp vecs
   sort(_temp1.begin(), _temp1.end());
   sort(_temp2.begin(), _temp2.end());

   // compare temp vecs
   vector<path>::iterator itr1 = _temp1.begin();
   vector<path>::iterator end1 = _temp1.end();
   vector<path>::iterator itr2 = _temp2.begin();
   vector<path>::iterator end2 = _temp2.end();
   path full0;
   path full1;
   path rel;
   auto ground0 = [this] (path const& p) -> path { return groundPath(p, 0); };
   auto ground1 = [this] (path const& p) -> path { return groundPath(p, 1); };
   if (itr1 != end1 && itr2 != end2) {
      full0 = fullPath(*itr1, 0);
      full1 = fullPath(*itr2, 1);
      while (true) {
         //cout << "full0: " << full0 << '\n';
         //cout << "full1: " << full1 << '\n';
         // if the names are the same
         if (*itr1 == *itr2) {
            // relative path is the same for both directories
            rel = relPath(*itr1);

            // *itr1 is a file
            if (is_regular_file(full0)) {
               // *itr1 is file, *itr2 is file
               if (is_regular_file(full1)) {
                  // test that filesizes match
                  if (file_size(ground0(rel)) == file_size(ground1(rel))) {
                     // Note that file content may still differ!
                     // If this is an issue we can check modification dates or
                     // store hashes (though file metadata is not currently duplicated).
                     _sc.f.push_back(rel, ground0);
                  } else {
                     _sizeIssues.push_back(rel);
                  }
               // *itr1 is file, *itr2 is not
               } else {
                  _fdIssues.push_back(rel);
               }
            // *itr1 is a dir
            } else {
               // *itr1 is dir, *itr2 is dir
               if (is_directory(full1)) {
                  // Note that directory contents may still differ;
                  // we will address this later.
                  _sc.d.push_back(rel);
               // *itr1 is dir, *itr2 is not
               } else {
                  _fdIssues.push_back(rel);
               }
            }
            // advance
            ++itr1;
            ++itr2;
            if (itr1 == end1 || itr2 == end2) { break; }
            full0 = fullPath(*itr1, 0);
            full1 = fullPath(*itr2, 1);

         // if *itr1 comes first, it is unique to dir1
         } else if (*itr1 < *itr2) {
            _uc[0].add(relPath(*itr1), full0);
            if (++itr1 == end1) { break; }
            full0 = fullPath(*itr1, 0);

         // if *itr2 comes first, it is unique to dir2
         } else {
            _uc[1].add(relPath(*itr2), full1);
            if (++itr2 == end2) { break; }
            full1 = fullPath(*itr2, 1);
         }
      }
   }

   // all remaining contents are unique
   // (only one of these while loop blocks ever executes)
   while (itr1 != end1) {
      _uc[0].add(relPath(*itr1), ground0);
      ++itr1;
   }
   while (itr2 != end2) {
      _uc[1].add(relPath(*itr2), ground1);
      ++itr2;
   }
}

//------------------------------------------------------------------------------
void DirectoryComparer::recursiveCompare () {
   compare();
   while (_sc.d.size()) {
      _extension = _sc.d.back();
      _sc.d.pop_back();
      compare();
   }
}

//------------------------------------------------------------------------------
void DirectoryComparer::copy () {
   // variables
   FileCopier copier;
   FileVector& f0 = _uc[0].f;    // for convenience
   vector<path>& d0 = _uc[0].d;  // for convenience
   path fullpath0;               // convenience (updated in loops)
   path fullpath1;               // convenience (updated in loops)
   FileVector errors;            // holds files that we fail to copy
   
   // precompute total number of files and bytes to be transferred
   annotate0();

   // prepare batch, print totals
   unsigned totalFiles = _uc[0].files();
   FileSize totalBytes = _uc[0].bytes();
   copier.startBatch(totalFiles, totalBytes);
   cout << "Copying " << totalFiles  << " files totaling " << totalBytes
        << " from " << workingPath(0) << " to " << workingPath(1) << ".\n";
   cout << "  Bytes Processed   |   Current File\n";

   // copy files from _uc[0].f
   for (unsigned i=0; i<f0.size(); ++i) {
      fullpath0 = groundPath(f0[i], 0);
      fullpath1 = groundPath(f0[i], 1);
      if (exists(fullpath1)) {
         // error!
         errors.push_back(f0[i], fullpath0);
         cout << "Warning: Cannot copy " << fullpath0 << " to " << fullpath1 << " because the latter already exists.\n";
      } else {
         copier.copy(fullpath0, fullpath1, f0[i]);
      }
   }

   // copy files from _uc[0].d
   for (unsigned i=0; i<d0.size(); ++i) {
      // iteration variables
      recursive_directory_iterator itr(groundPath(d0[i], 0));
      recursive_directory_iterator end;
      unsigned depth = 0;
      path connector("");
      path new_extension;

      while (itr != end) {
         // update connector
         if (depth < itr.level()) {
            connector /= new_extension;
            cout << copier.status << "Creating directory " << connector << '.' << '\n';
            //create_directory(_p[1] / connector);
            ++depth;  // this makes depth equal to itr.level()
         } else while (depth > itr.level()) {
            connector.remove_filename();
            --depth;
         }

         // copy file or save name of directory we may iterate into
         if (is_regular_file(itr->path())) {
            fullpath0 = itr->path();
            fullpath1 = _p[1] / connector / fullpath0.filename();
            if (exists(fullpath1)) {
               // error!
               errors.push_back(connector / fullpath0.filename(), fullpath0);
               cout << copier.status << "Warning: Cannot copy " << fullpath0 << " to " << fullpath1 << " because the latter already exists.\n";
            } else {
               copier.copy(fullpath0, fullpath1, connector / fullpath0.filename());
            }
         } else if (is_directory(itr->path())) {
            new_extension = itr->path().filename();
         }
         ++itr;
      }
   }

   // cleanup
   _uc[0].f.clear();
   _uc[0].d.clear();

   // print summary
   cout << setw(9) << totalBytes << '/' << setw(9) << totalBytes << " | ";
   cout << totalFiles - errors.size() << " of " << totalFiles << " files were copied.\n";
   if (errors.size()) {
      cout << "The following files were not copied:\n";
      for (unsigned i=0; i<errors.size(); ++i) {
         cout << errors[i] << '\n';
      }
   }
   cout << '\n';
}

//------------------------------------------------------------------------------
void DirectoryComparer::print0 () const {
   cout << "========== Unique to " << _p[0] << " ==========\n";
   _uc[0].print();
   cout << '\n';
}

//------------------------------------------------------------------------------
void DirectoryComparer::print1 () const {
   cout << "========== Unique to " << _p[1] << " ==========\n";
   _uc[1].print();
   cout << '\n';
}

//------------------------------------------------------------------------------
void DirectoryComparer::printShared () const {
   cout << "========== Common ==========\n";
   _sc.print();
   cout << '\n';
}

//------------------------------------------------------------------------------
void DirectoryComparer::printIssues () const {
   if (_sizeIssues.size() || _fdIssues.size()) {
      cout << "========== Issues ==========\n";
      for (path const& p : _sizeIssues) {
         cout << p << " is "  << file_size(groundPath(p, 0)) << " in " << _p[0]
                   << " but " << file_size(groundPath(p, 1)) << " in " << _p[1] << '.' << '\n';
      }
      for (path const& p : _fdIssues) {
         if (is_regular_file(groundPath(p, 0))) {
            cout << p << " is a file in " << _p[0] << " but a directory in " << _p[1] << '.' << '\n';
         } else {
            cout << p << " is a directory in " << _p[0] << " but a file in " << _p[1] << '.' << '\n';
         }
      }
   } else {
      cout << "No issues detected. Backup should run smoothly.\n";
   }
   cout << '\n';
}

//------------------------------------------------------------------------------
void DirectoryComparer::printSummary () const {
   cout << "Backup Summary:\n";
   cout << setw(5) << _uc[0].files() << " files (" << setw(9) << _uc[0].bytes() << ") are to be copied.\n";
   cout << setw(5) << _uc[1].files() << " files (" << setw(9) << _uc[1].bytes() << ") are to be deleted.\n";
   cout << setw(5) << _sc.files()    << " files (" << setw(9) << _sc.bytes() << ") are already backed up.\n";
   cout << setw(5) << _sizeIssues.size() + _fdIssues.size() << " files are in conflict.\n";
}

//==============================================================================
// main
//==============================================================================

//------------------------------------------------------------------------------
int main (int argc, char** argv) {
   
   /*
   path dir1a = path("/Volumes/Ryan Durkin/test1/dirdoc");
   path dir2a = path("/Users/erik/Documents/test1/dirdoc");
   path empty;
   cout << "empty: " << empty << '\n';
   cout << "dir1a / empty: " << dir1a / empty << '\n';
   cout << dir1a << '\n';
   cout << dir2a << '\n';
   if (dir1a.filename() == dir2a.filename()) {
      cout << "yup\n";
   }
   if (is_regular_file(dir1a)) {
      cout << dir1a << " is reg file\n";
   }
   if (is_regular_file(dir2a)) {
      cout << dir2a << " is reg file\n";
   }
   return 0;
   */

   try {
      path dir0 = path("/Volumes/Ryan Durkin/test1");
      path dir1 = path("/Volumes/Fabrizio/test2");
      //path dir0 = path("/Users/erik/Documents/test1");
      //path dir1 = path("/Users/erik/Documents/test2");
      DirectoryComparer dc;
      dc.setPaths(dir0, dir1);

      dc.summary();
      cout << '\n';
      dc.status(false, false, true, true);
      //dc.copy();
   }
   catch (...) {
      cout << "An error occurred!\n";
   }

   return 0;
}
