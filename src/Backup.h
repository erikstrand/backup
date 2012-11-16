//==============================================================================
// Backup.cpp
// created November 15, 2012
//==============================================================================

#include <vector>
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include "FileSize.h"

namespace bfs = boost::filesystem;


//==============================================================================
// FileCopier (and CopyStatus)
//==============================================================================

//------------------------------------------------------------------------------
struct CopyStatus {
   static const unsigned fsw = 9;

   FileSize bytes;
   FileSize totalBytes;
   FileSize fileBytes;
   FileSize fileTotal;
   //unsigned files;
   unsigned totalFiles;
   bfs::path srcPath;
   bfs::path dstPath;
   bfs::path dspPath;

   CopyStatus (): bytes(0), totalBytes(0), fileBytes(0), totalFiles(0) {}
};
std::ostream& operator<< (std::ostream& os, CopyStatus const& s);

//------------------------------------------------------------------------------
struct FileCopier {
public:
   char buf[BUFSIZ];
   unsigned bufs_per_update;
   unsigned fsw;
   CopyStatus status;
   // when in safe mode no files are created, altered, or deleted
   bool safe_mode;

public:
   FileCopier (): bufs_per_update(512000), fsw(9), safe_mode(false) {}
   FileCopier (bool safe): bufs_per_update(512000), fsw(9), safe_mode(safe) {}
   void startBatch (unsigned nFiles, FileSize nBytes);
   void copy (bfs::path const& srcpath, bfs::path const& dstpath, bfs::path const& dsppath);
   void copy (bfs::path const& srcpath, bfs::path const& dstpath) { copy(srcpath, dstpath, srcpath); }

private:
   void printStart  (CopyStatus const& s) const;
   void printUpdate (CopyStatus const& s) const;
};


//==============================================================================
// Modified Vectors (FileVector, DirVector, and FDPair)
//==============================================================================

//------------------------------------------------------------------------------
// A vector of files that keeps track of the combined size of its contents.
class FileVector : public std::vector<bfs::path> {
protected:
   FileSize _bytes;

public:
   FileVector (): _bytes(0) {}

   void push_back (bfs::path const& p, bfs::path const& full) {
      _bytes += file_size(full);
      std::vector<bfs::path>::push_back(p);
   }

   template <typename Func>
   void push_back (bfs::path const& p, Func grounder) {
      push_back(p, grounder(p));
   }

   void clear () {
      _bytes = 0;
      std::vector<bfs::path>::clear();
   }

   unsigned files () const { return std::vector<bfs::path>::size(); }
   FileSize bytes () const { return _bytes; }

// this method has no use in FileVector, so we're making it inaccessible
private:
   void push_back (bfs::path const& p) {}
};

//------------------------------------------------------------------------------
// A vector of directories that will tally the number and total size of all
// children of its contents.
class DirVector : public FileVector {
private:
   unsigned _files;

public:
   DirVector (): _files(0) {}

   void push_back (bfs::path const& p) { std::vector<bfs::path>::push_back(p); }
   template <typename Func> void annotate (Func grounder);
   unsigned files () const { return _files; }

// these methods have no use in DirVector, so we're making them inaccessible
private:
   void push_back (bfs::path const& p, bfs::path const& full) {}
   template <typename Func> void push_back (bfs::path const& p, Func grounder) {} 
};

//------------------------------------------------------------------------------
// A FileVector and DirVector working together.
struct FDPair {
   FileVector f;
   DirVector  d;

   void add (bfs::path const& p, bfs::path const& fullPath) {
      if (is_regular_file(fullPath)) {
         f.push_back(p, fullPath);
      } else if (is_directory(fullPath)) {
         d.push_back(p);
      }
   }
   template <typename Func> void add (bfs::path const& p, Func grounder) { add(p, grounder(p)); }

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


//==============================================================================
// DirectoryComparer
//==============================================================================

//------------------------------------------------------------------------------
class DirectoryComparer {
private:
   static const unsigned A0 = 0x1;
   static const unsigned A1 = 0x2;
   static const unsigned AM = 0x4;
   static const unsigned RC = 0x8;

private:
   bfs::path _p[2];
   bfs::path _extension;

   FDPair _uc[2];    // files and directories unique to dir1 and dir2
   FDPair _sc;       // shared files and directories

   std::vector<bfs::path> _sizeIssues; // shared files with different sizes
   std::vector<bfs::path> _fdIssues;   // shared paths with file / directory mismatch

   std::vector<bfs::path> _temp1;
   std::vector<bfs::path> _temp2;

   unsigned _annotations;

   bool ignore_hidden_files;
   // when in safe mode no files are created, altered, or deleted
   bool safe_mode;

public:
   DirectoryComparer (): _extension(""), _annotations(0), ignore_hidden_files(true) {}
   void setSafeMode (bool safe) { safe_mode = safe; }
   void setPaths (bfs::path const& p0, bfs::path const& p1) {
      _p[0] = p0;
      _p[1] = p1;
   }

   void summary ();
   void status (bool p0, bool p1, bool ps, bool pi);
   void backup ();

private: 
   bfs::path workingPath (unsigned n)                     const { return _p[n] / _extension; }
   bfs::path relPath     (bfs::path const& p)             const { return _extension / p; }
   bfs::path fullPath    (bfs::path const& p, unsigned n) const { return _p[n] / _extension / p; }
   bfs::path groundPath  (bfs::path const& e, unsigned n) const { return _p[n] / e; }

   void compare ();
   void recursiveCompare ();
   inline void annotate0 ();
   inline void annotate1 ();
   inline void annotateMutual ();
   void copy ();

   void print0       () const;
   void print1       () const;
   void printShared  () const;
   void printIssues  () const;
   void printSummary () const;
};

//------------------------------------------------------------------------------
void DirectoryComparer::annotate0 () {
   if (!(_annotations & A0)) {
      _uc[0].d.annotate([this] (bfs::path const& p) { return groundPath(p, 0); });
      _annotations |= A0;
   }
}

//------------------------------------------------------------------------------
void DirectoryComparer::annotate1 () {
   if (!(_annotations & A1)) {
      _uc[1].d.annotate([this] (bfs::path const& p) { return groundPath(p, 1); });
      _annotations |= A1;
   }
}

//------------------------------------------------------------------------------
void DirectoryComparer::annotateMutual () {
   if (!(_annotations & AM)) {
      _sc.d.annotate([this] (bfs::path const& p) { return groundPath(p, 0); });
      _annotations |= AM;
   }
}

