// Precompiled header
#include "pch.hpp"

// standard c/c++ libraries

// GA classes
#include "gaScsDataConstants.hpp"
#include "EventMap.hpp"
#include "AxisPositions.hpp"

// display argument usage
void static show_usage(std::string name) {  // 'static' limits scope to this translational unit
  std::cout << "Usage for " << name << ":" << std::endl << std::endl
            << "Command line arguments:" << std::endl
            << "\t-h, -H, -?,-help, or -Help will display this usage message" << std::endl
            << "\t-p or -P will create the SCS and CLS position tables" << std::endl
            << "\t-e or -E will create the Event table" << std::endl
            << "If no arguments are included, this help message is displayed." << std::endl
            << "At least one argument must be included, and only the specified tables will be processed." << std::endl
            << "The arguments can be used in any order." << std::endl
            << "Valid examples are:" << std::endl
            << "\t\"-p\" (SCS and CLS position tables only, no event table)" << std::endl
            << "\t\"-P -e\" (SCS and CLS position table and event table)" << std::endl
            << "\t\"-E\" (Event table only, no SCS or CLS position table)" << std::endl << std::endl;
  }

int main(int argc, char* argv[]) {
  // Look at command line arguments to see which tables need to be processed
    // -h, -H, -?, -help, or -Help will display a usage message
    // -p or -P will create the SCS and CLS position tables
    // -e or -E will create the Event table
  // At least one argument must be included, and only the specified tables will be processed.
  // If none are included, the help message is displayed.
  // The arguments can be used in any order.
  // Valid examples are:
    //  "-p" (SCS and CLS position tables only, no event table)
    // "-P -e" (SCS and CLS position table and event table)
    // "-E" (Event table only, no SCS or CLS position table)


  // keep track of what the arguments are asking us to do
  bool runPos= false;
  bool runEvents= false;

  // process the arguments
  if (1== argc) { // no arguments, program name only
    // show usage
    std::cout << std::endl << "No arguments found. Need at least 1 argument." << std::endl;
    show_usage(argv[0]);
    std::cout << "Press enter to exit." << std::endl;
    std::getchar();
    return 1; // exit with error
    }
  else {  // there is at least 1 argument 
    std::string arg; // holding place
    // loop thru the arguments
    for(size_t i= 1; i < argc; ++i) {
      // get the argument and see what it is
      arg= argv[i];
      if ("-h" == arg || "-H" == arg || "-?" == arg ||"-help" == arg || "-Help" == arg) {
        // help argument. Show usage and leave.
        show_usage(argv[0]);
        std::cout << "Press enter to exit." << std::endl;
        std::getchar();
        return 0; // exit okay
        }
      else if ("-p" == arg || "-P" == arg) {
        // create position tables argument
        runPos= true;
        }
      else if ("-e" == arg || "-E" == arg) {
        // create event table argument
        runEvents= true;
        }
      else {  // argument not recognized. Show usage and leave.
        std::cout << std::endl << "Unrecognized argument: \"" << arg << "\"" << std::endl;
        show_usage(argv[0]);
        std::cout << "Press enter to exit." << std::endl;
        std::getchar();
        return 1; // exit error
        }
      }
    }

/*
  // TROUBLESHOOTING: force to false so we can prevent anything from running while troubleshooting
  // Comment this section out for normal operation
  std::cout << "runEvents flag:" << runEvents << std::endl;
  runEvents= false;
  std::cout << "runPos flag:" << runPos << std::endl;
  runPos= false;
*/

  if (runEvents) {
    gaScsData::EventMap eventMap1;
    long status= eventMap1.GenerateEventMapTable();
    }

  if (runPos) {

    std::cout << "Creating Axis Position Object." << std::endl;
    gaScsData::AxisPositions axPos;
    std::cout << "Axis Position Object Created." << std::endl << std::endl ;

    std::cout << "Fetch coil map from db and populate the resident coil map." << std::endl;
    long status= axPos.GenerateCoilMap();
    std::cout << "Coil Map populated." << std::endl << std::endl;

    std::cout << "Generate position tables." << std::endl;
    axPos.GeneratePositionTables();
    std::cout << "Position tables generated." << std::endl << std::endl;
    }

  std::cout << "Press enter to exit." << std::endl;
  std::getchar();
  return 0;
}

