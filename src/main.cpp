//==============================================================================
// main.cpp
// created November 15, 2012
//==============================================================================

#include <iostream>
#include <boost/program_options.hpp>
#include "Backup.h"

using namespace std;
using boost::filesystem::path;
namespace po = boost::program_options;


//==============================================================================
// main
//==============================================================================

//------------------------------------------------------------------------------
int main (int argc, char** argv) {

   // Declare the supported options.
   po::options_description opts("Allowed options");
   opts.add_options()
       ("help",          "Print a help message.")
       ("outline,o",     "Print a four line outline of -abmi")
       ("show-a,a",      "Print files unique to directory A. These will be copied if invoked with -c.")
       ("show-b,b",      "Print files unique to directory B. These will be deleted if invoked with -d.")
       ("show-mutual,m", "Print files that are in both directories.")
       ("show-issues,i", "Print file conflicts that must be manually resolved.")
       ("copy,c",        "Copy directory A's unique files to directory B.")
       ("delete,d",      "Delete directory B's unique files.")
       ("safe,s",        "Run in Safe Mode: no files are created, modified, or removed.")
       ("dir_a",         "Directory A - the directory that should be backed up.")
       ("dir_b",         "Directory B - the directory where the backup copy is (or will be) located.")
   ;

   // Declare positional arguments.
   po::positional_options_description dirs;
   dirs.add("dir_a", 1);
   dirs.add("dir_b", 1);

   // Parse command line, detect errors.
   po::variables_map vm;
   try {
      po::store(po::command_line_parser(argc, argv).options(opts).positional(dirs).run(), vm);
   }
   catch (po::too_many_positional_options_error) {
      cout << "You may only specify two directories. For assistance, execute with the option --help.\n";
      return 0;
   }
   catch (po::error_with_option_name) {
      cout << "Invalid options. For assistance, execute with the option --help.\n";
      return 0;
   }
   po::notify(vm);    


   // Display help if requested.
   if (vm.count("help")) {
       cout << opts << "\n";
       return 0;
   }

   // Ensure we have two directories to work with.
   if (!vm.count("dir_b")) {
      cout << "You must specify two directories. For assistance, execute with the option --help.\n";
      return 0;
   }

   // Check that the directories exist.
   std::string dirA = vm["dir_a"].as<std::string>();
   std::string dirB = vm["dir_b"].as<std::string>();
   if (!boost::filesystem::is_directory(dirA)) {
      cout << "Error: " << dirA << " is not a reachable directory!\n";
      return 0;
   }
   if (!boost::filesystem::is_directory(dirB)) {
      cout << "Error: " << dirB << " is not a reachable directory!\n";
      return 0;
   }

   // Extract flags for which commands are requested.
   bool outline    = false;
   bool showA      = false;
   bool showB      = false;
   bool showMutual = false;
   bool showIssues = false;
   bool copy       = false;
   bool del        = false;
   bool safe       = false;
   if (vm.count("outline"))     { outline    = true; }
   if (vm.count("show-a"))      { showA      = true; }
   if (vm.count("show-b"))      { showB      = true; }
   if (vm.count("show-mutual")) { showMutual = true; }
   if (vm.count("show-issues")) { showIssues = true; }
   if (vm.count("copy"))        { copy       = true; }
   if (vm.count("delete"))      { del        = true; }
   if (vm.count("safe"))        { safe       = true; }


   // Execute the requested actions.
   try {
      // Create DirectoryComparer and set directories.
      DirectoryComparer dc;
      dc.setSafeMode(safe);
      dc.setPaths(dirA, dirB);

      if (outline) {
         dc.outline();
      }

      if (showA || showB || showMutual || showIssues) {
         dc.status(showA, showB, showMutual, showIssues);
      }

      if (copy || del) {
         dc.backup(copy, del);
      }
   }
   catch (...) {
      cout << "An unexpected error occurred! Were any files in either directory modified during execution?\n";
   }

   return 0;
}

