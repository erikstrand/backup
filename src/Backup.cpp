//==============================================================================
// Backup.cpp
// created November 15, 2012
//==============================================================================

#include "Backup.h"
#include <iomanip>
#include <algorithm>

using namespace std;
using namespace boost::filesystem;


//==============================================================================
// FileCopier (and CopyStatus)
//==============================================================================

//------------------------------------------------------------------------------
ostream& operator<< (ostream& os, CopyStatus const& s) {
   return os << setw(s.fsw) << s.bytes << '/' << setw(s.fsw) << s.totalBytes << " | ";
}
   
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
   ofstream dst;
   if (!safe_mode) dst.open(dstpath.c_str(), ios_base::out | ios_base::binary);

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
      if (!safe_mode) dst.write(buf, src.gcount());
   }

   src.close();
   if (!safe_mode) dst.close();
   status.bytes = initialBytes + status.fileTotal;
}

//------------------------------------------------------------------------------
void FileCopier::printStart (CopyStatus const& s) const {
   cout << s << "Copying " << s.dspPath << " (" << s.fileTotal << ')' << '\n';
}

//------------------------------------------------------------------------------
void FileCopier::printUpdate (CopyStatus const& s) const {
   cout << s << "... " << s.fileBytes << '/' << s.fileTotal << '\n';
}


//==============================================================================
// Modified Vectors (FileVector, DirVector, and FDPair)
//==============================================================================

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
void FDPair::fprint () const {
   cout << f.size() << " files totaling " << fbytes() << '.' << '\n';
   for (unsigned i=0; i<f.size(); ++i) {
      cout << "  * " << f[i] << '\n';
   }
   cout << '\n';
}

//------------------------------------------------------------------------------
void FDPair::dprint () const {
   cout << d.size() << " directories, containing " << dfiles() << " files (" << dbytes() << ")." << '\n';
   for (unsigned i=0; i<d.size(); ++i) {
      cout << "  * " << d[i] << '\n';
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
void DirectoryComparer::outline () {
   recursiveCompare();
   annotate0();
   annotate1();
   annotateMutual();
   printOutline();
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
      annotateMutual();
      printShared();
   }
   if (pi) {
      printIssues();
   }
}

//------------------------------------------------------------------------------
void DirectoryComparer::backup (bool c, bool d) {
   recursiveCompare();
   if (c) copy();
   if (d) del();
}

//------------------------------------------------------------------------------
void DirectoryComparer::compare () {
   // clear annotations and temp vecs
   _annotations = 0;
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
   if (!(_annotations & RC)) {
      compare();
      while (_sc.d.size()) {
         _extension = _sc.d.back();
         _sc.d.pop_back();
         compare();
      }
      _extension = "";
      _annotations |= RC;
   }
}

//------------------------------------------------------------------------------
void DirectoryComparer::copy () {
   // variables
   FileCopier copier(safe_mode);
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
   cout << "========== Copying Files from A to B ==========\n";
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
   recursive_directory_iterator end;
   unsigned depth = 0;
   path connector;
   path new_extension;
   for (unsigned i=0; i<d0.size(); ++i) {
      // initialize for recursive crawl of this directory
      recursive_directory_iterator itr(groundPath(d0[i], 0));
      depth = 0;
      connector = d0[i];
      cout << copier.status << "Creating directory " << connector << '.' << '\n';
      if (!safe_mode) create_directory(_p[1] / connector);

      while (itr != end) {
         // update connector
         if (depth < itr.level()) {
            connector /= new_extension;
            cout << copier.status << "Creating directory " << connector << '.' << '\n';
            if (!safe_mode) create_directory(_p[1] / connector);
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
   _annotations &= ~A0;

   // print outline
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
void DirectoryComparer::del () {
   // precompute total number of files and bytes to be transferred
   annotate1();

   // prepare batch, print totals
   unsigned totalFiles = _uc[1].files();
   FileSize totalBytes = _uc[1].bytes();
   cout << "========== Deleting Files from B ==========\n";
   cout << "Removing " << totalFiles  << " files totaling " << totalBytes
        << " from " << workingPath(1) << ".\n";

   // delete files in _uc[1].f
   path grounded;
   for (path const& p : _uc[1].f) {
      grounded = groundPath(p, 1);
      cout << "Removing " << p << " (" << FileSize(file_size(grounded)) << ").\n";
      if (!safe_mode) remove(grounded);
   }

   // delete files in _uc[1].d
   for (path const& p : _uc[1].d) {
      cout << "Removing " << p << ".\n";
      if (!safe_mode) remove_all(groundPath(p, 1));
   }
}

//------------------------------------------------------------------------------
void DirectoryComparer::print0 () const {
   cout << "========== Unique to " << _p[0] << " ==========\n";
   _uc[0].print();
}

//------------------------------------------------------------------------------
void DirectoryComparer::print1 () const {
   cout << "========== Unique to " << _p[1] << " ==========\n";
   _uc[1].print();
}

//------------------------------------------------------------------------------
void DirectoryComparer::printShared () const {
   cout << "========== Common to " << _p[0] << " and " << _p[1] << " ==========\n";
   _sc.fprint();
   if (!(_annotations & RC)) {
      _sc.dprint();
   }
}

//------------------------------------------------------------------------------
void DirectoryComparer::printIssues () const {
   cout << "========== Issues ==========\n";
   if (_sizeIssues.size() || _fdIssues.size()) {
      for (path const& p : _sizeIssues) {
         cout << "  * " << p << " is "  << FileSize(file_size(groundPath(p, 0))) << " in " << _p[0]
              << " but " << FileSize(file_size(groundPath(p, 1))) << " in " << _p[1] << '.' << '\n';
      }
      for (path const& p : _fdIssues) {
         cout << "  * ";
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
void DirectoryComparer::printOutline () const {
   cout << "========== Outline ==========\n";
   cout << "Directory A: " << _p[0] << '\n';
   cout << "Directory B: " << _p[1] << '\n';
   cout << setw(5) << _uc[0].files() << " files (" << setw(9) << _uc[0].bytes() << ") are to be copied.\n";
   cout << setw(5) << _uc[1].files() << " files (" << setw(9) << _uc[1].bytes() << ") are to be deleted.\n";
   cout << setw(5) << _sc.files()    << " files (" << setw(9) << _sc.bytes() << ") are already backed up.\n";
   cout << setw(5) << _sizeIssues.size() + _fdIssues.size() << " files are in conflict and must be manually resolved.\n";
   cout << '\n';
}

