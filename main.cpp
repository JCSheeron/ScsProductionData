// Precompiled header
#include "pch.hpp"

// standard c/c++ libraries
#include <ctime> // get local start and end times, elapsed times. 
#pragma warning(disable : 4996) // _CRT_SECURE_NO_WARNINGS -- disable warnings casued byt ctime

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
      << "The arguments can be used in any order." << std::endl << std::endl
      << "NOTE the Event table generation uses the new layer start" << std::endl
      << "\tposition row angle info from the Scs Position table to" << std::endl
      << "\tgenerate Layer increment events. This means that the" << std::endl
      << "\tevent table can be generated sperately, but the ScsPositon" << std::endl
      << "\ttable must exist in order for there to be layer increment " << std::endl
      << "\tevents created in the event table" << std::endl << std::endl
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
    // If none are included, the help message is displayed.
    // The arguments can be used in any order.
    // At least one argument must be included, and only the specified tables will be processed.

    // NOTE the Event table generation uses the new layer start
      // position row angle info from the Scs Position table to
      // generate Layer increment events. This means that the
      // event table can be generated sperately, but the ScsPosition
      // table must exist in order for there to be layer increment 
      // events created in the event table

    // Valid examples are:
      //  "-p" (SCS and CLS position tables only, no event table)
      // "-P -e" (SCS and CLS position table and event table)
      // "-E" (Event table only, no SCS or CLS position table)


    // keep track of what the arguments are asking us to do
    bool runPos = false;
    bool runEvents = false;

    // process the arguments
    if (1 == argc) { // no arguments, program name only
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
      for (int i = 1; i < argc; ++i) {
        // get the argument and see what it is
        arg = argv[i];
        if ("-h" == arg || "-H" == arg || "-?" == arg || "-help" == arg || "-Help" == arg) {
          // help argument. Show usage and leave.
          show_usage(argv[0]);
          std::cout << "Press enter to exit." << std::endl;
          std::getchar();
          return 0; // exit okay
        }
        else if ("-p" == arg || "-P" == arg) {
          // create position tables argument
          runPos = true;
        }
        else if ("-e" == arg || "-E" == arg) {
          // create event table argument
          runEvents = true;
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

    // get and display start time
    time_t startRawTime= time(0);
    struct tm* sStartTime= localtime(&startRawTime);

    std::cout << std::endl << "Start time: " << asctime(sStartTime) << std::endl;

    if (runPos) {

      std::cout << "Creating Axis Position Object." << std::endl;
      gaScsData::AxisPositions axPos;
      std::cout << "Axis Position Object Created." << std::endl << std::endl;

      std::cout << "Fetch coil map from db and populate the resident coil map." << std::endl;
      long status = axPos.GenerateCoilMap();
      if (gaScsData::RTN_NO_ERROR == status)
        std::cout << "Coil Map populated." << std::endl;
      else
        std::cout << "Error when populating Coil Map." << std::endl;



      std::cout << "Generating position tables..." << std::endl;
      status = axPos.GeneratePositionTables();
      if (gaScsData::RTN_NO_ERROR == status)
        std::cout << "Position Tables Generated." << std::endl;
      else
        std::cout << "Error when generating position tables." << std::endl;
    }

    // if selected, run events second, so the scs position is available
    if (runEvents) {
      std::cout << std::endl << "Creating Event Map Object." << std::endl;
      gaScsData::EventMap eventMap1;
      std::cout << "Event Map Object Created." << std::endl;

      std::cout << "Generating Event Map ..." << std::endl;
      long status = eventMap1.GenerateEventMapTable();
      if (gaScsData::RTN_NO_ERROR == status)
        std::cout << "Event Map Generated." << std::endl;
      else
        std::cout << "Error when generating event map." << std::endl;
    }

    // get and display end time and elapsed time
    time_t endRawTime= time(0);
    struct tm* sEndTime= localtime(&endRawTime);
    std::cout << "End time: " << asctime(sEndTime) << std::endl;
    double elapsedSec= difftime(endRawTime, startRawTime);
    double minutes;
    // convert elapsed seconds to minutes, break into fract and int parts
    // then calc the seconds component
    long seconds= static_cast<long>((modf((elapsedSec / 60.0), &minutes)) * 60.0);
    
    std::cout << "Elapsed time (min:sec): " << minutes << ":" << seconds << std::endl << std::endl;

    std::cout << "Press enter to exit." << std::endl;
    std::getchar();
    return 0;
  }
