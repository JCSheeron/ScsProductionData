/********************************************************************
 * COPYRIGHT -- General Atomics
 ********************************************************************
 * Library: 
 * File: EventMap.cpp
 * Author: J. Sheeron (x2315)
 * Created: March 05, 2014
  *******************************************************************
 * Function:
 * Other libraries used:
 *******************************************************************/
// Precompiled header
#include "pch.hpp"

// header file
#include "gaScsDataConstants.hpp"
#include "EventMap.hpp"

namespace gaScsData {

// ctors and dtor
  EventMap::EventMap() : 
      coilMap_(),
      serverText_(DB_SERVER_NAME + "@" + DB_DATABASE_NAME),
      errorText_("") {

    // reserve space for the number of Hqp start angles
    hqpStartSet_.reserve(MAX_NUM_OF_HQP_START_ANGLES);

    // reserve space for number of layer start angles
    layerStartSet_.reserve(MAX_NUM_OF_LAYER_START_ANGLES);

    // initialize database connectivity parameters
    // use SQL server native client
    dbConnection_.setClient(SA_SQLServer_Client);
    // Set the connection to use the ODBC API
    // ODBC is default API, so this is only being specified for backward compatibility and documentation.
    dbConnection_.setOption( "UseAPI" ) = "ODBC";
    // Set the commnad object to use the connection
    dbCommand_.setConnection(&dbConnection_);

  }

  EventMap::~EventMap() { }

// public accessors

// public methods
  long EventMap::GenerateEventMapTable() {
    // return value indicates success or error
    long connectStatus = 0;
    long queryStatus = 0;
    long opStatus = 0;  // operation status
    long disconnectStatus = 0;

    // connect to db
    std::cout << "Connect to Db." << std::endl;
    connectStatus= DbConnect();
    if (RTN_NO_ERROR == connectStatus)
      std::cout << "Connection successful." << std::endl;
    else
      std::cout << "Connection error!!" << std::endl;

    // if connect status is okay
      // 1) populate the coil map
      // 2) Get the hqp start angles and make a set of them
      // 3) Get the layer start angles make a set of them
      // 4) Iterate thru the coil map and calculate the rest of the event instances -- populate the Event Map
      // 5) The event map is now complete for static events.
      //    Iterate thru the event map, and insert the events into the DB
    if (RTN_NO_ERROR == connectStatus) {
      // if no error (DB connect was sucessful)
      std::cout << "Populate coil map." << std::endl;
      opStatus= coilMap_.PopulateCoilMap();
      }

    if (RTN_NO_ERROR == opStatus)
      std::cout << "Populate Coil Map successful." << std::endl;
    else
      std::cout << "Populate Coil Map error!!" << std::endl;

    // if the connect status and the coil populate status is okay,
    // continue and get the hqp start angles
    if (RTN_NO_ERROR == connectStatus &&
        RTN_NO_ERROR == opStatus) {
      std::cout << "Get HQP Start Angles." << std::endl;
      queryStatus= QueryDb(SPNAME_SELECT_HQPSTART_ANGLES);
      }

    if (RTN_NO_ERROR == queryStatus)
      std::cout << "HQP Start Angles retrievd." << std::endl;
    else
      std::cout << "HQP Start Angles retrieval error!!" << std::endl;

    // if connect status, previous operation, and previous query were all okay,
    // continue and make a set of the result (hqp start angles).
    if (RTN_NO_ERROR == connectStatus &&
        RTN_NO_ERROR == opStatus &&
        RTN_NO_ERROR == queryStatus) {
      std::cout << "Save HQP Start Angles." << std::endl;
      opStatus= PopulateHqpStartSet();
      }

    if (RTN_NO_ERROR == opStatus)
      std::cout << "HQP Start Angles saved." << std::endl;
    else
      std::cout << "HQP Start Angles save error!!" << std::endl;

    // if connect status, previous query, and previous operation were all okay,
    // continue and get the the layer start angles
    if (RTN_NO_ERROR == connectStatus &&
        RTN_NO_ERROR == opStatus &&
        RTN_NO_ERROR == queryStatus) {
      std::cout << "Get Layer Start Angles." << std::endl;
      queryStatus= QueryDb(SPNAME_SELECT_LAYERSTART_ANGLES);
      }

    if (RTN_NO_ERROR == queryStatus)
      std::cout << "Layer Start Angles retrievd." << std::endl;
    else
      std::cout << "Layer Start Angles retrieval error!!" << std::endl;

    // if connect status, previous operation, and previous query were all okay,
    // continue and make a set of the result (hqp start angles).
    if (RTN_NO_ERROR == connectStatus &&
        RTN_NO_ERROR == opStatus &&
        RTN_NO_ERROR == queryStatus) {
      std::cout << "Save Layer Start Angles." << std::endl;
      opStatus= PopulateLayerStartSet();
    }

    if (RTN_NO_ERROR == opStatus)
      std::cout << "Layer Start Angles saved." << std::endl;
    else
      std::cout << "Layer Start Angles save error!!" << std::endl;

    // if connect status and previous operation were okay,
    // continue and create the event map, then put it into the database.
    if (RTN_NO_ERROR == connectStatus &&
        RTN_NO_ERROR == opStatus) {
      
      // create the event map
      std::cout << std::endl << "Create the event Map." << std::endl;
      MapEventInstances();
      std::cout << "Done Creating the event Map." << std::endl;

      // Delete undone events
      std::cout << "Deleting all undone events before the new events are inserted." << std::endl;
      DeleteAllUndoneEvents();

      // populate the database
      std::cout << "Write the event Map to the database." << std::endl;
      opStatus= InsertIntoDb(SPNAME_INSERT_EVENTLIST);  // iterate thru the event map and insert a db row for each event
      std::cout << "Done Writing the event Map to the database." << std::endl;
    }

    // is connect status is okay (connection was sucessful), disconnect from the db
    if (RTN_NO_ERROR == connectStatus) {
      std::cout << std::endl << "Disconnect from the database" << std::endl;
      disconnectStatus = DbDisconnect();
      }

    // if all status is okay, return no error, otherwise return error
    if (RTN_NO_ERROR == connectStatus &&
      RTN_NO_ERROR == opStatus &&
      RTN_NO_ERROR == queryStatus &&
      RTN_NO_ERROR == disconnectStatus) {
      std::cout << "Done with no errors." << std::endl;
      return RTN_NO_ERROR;
      }
    else {
      std::cout << "An error has occurred!!." << std::endl;
      return RTN_ERROR;
      }
    } //EventMap::GenerateEventMapTable()

  bool EventMap::isEventLayerIncrement(CoilMap::cm_cit cit) const {
    CoilMap::fc_pair fcPair= coilMap_.GetCurrentNextFc(cit); // get current and next feature code
    long layerCheck= coilMap_.GetLayer(cit); // get layer
    // Joggle, but not the last joggle of the hex, and not last layer (i.e. not end of the coil)
    // true if FC(current row) = J, FC(next row) <> L and Layer <> next to last (39)
    return FC_JOGGLE == fcPair.first && FC_LOCAL != fcPair.second && LAYERS_PER_COIL - 1 != layerCheck;
    } // EventMap::isEventLayerIncrement(CoilMap::cm_cit cit)

  // When the even layer is ending, the odd layer is at the 0U, so the
  // check for ending an even layer is if the 0U is at an odd layer joggle 
  bool EventMap::isEventEndEvenLayer(CoilMap::cm_cit cit) const {
    bool fcJ = (FC_JOGGLE == coilMap_.GetFc(cit) ? true : false); // is joggle
    bool oddLayer = (1 == coilMap_.GetLayer(cit) % 2 ? true : false); // is layer odd?
    // Joggle and an odd layer
    return fcJ && oddLayer;
    } // EventMap::isEventEndEvenLayer(CoilMap::cm_cit cit)

  bool EventMap::isEventConsolidateOdd(CoilMap::cm_cit cit) const {
    bool fcJ = (FC_JOGGLE == coilMap_.GetFc(cit) ? true : false); // is joggle
    bool oddLayer = (1 == coilMap_.GetLayer(cit) % 2 ? true : false); // is layer odd?
    // Joggle and an odd layer
    return fcJ && oddLayer;
    } // EventMap::isEventConsolidateOdd(CoilMap::cm_cit cit)

  bool EventMap::isEventHqpLoad(CoilMap::cm_cit cit) const {
    CoilMap::fc_pair fcPair= coilMap_.GetCurrentNextFc(cit); // get current and next feature code
    // joggle before a new hex (i.e. last joggle)
    // FC (current row) = J AND FC (next row) = L
    return FC_JOGGLE == fcPair.first && FC_LOCAL == fcPair.second;
    } // EventMap::isEventHqpLoad(CoilMap::cm_cit cit)

  bool EventMap::isEventTeachFiducial(CoilMap::cm_cit cit) const {
    std::string fc = coilMap_.GetFc(cit);
    long hqp = coilMap_.GetHexQuadNumber(cit);
    // At a local zero, but skip the first one (hqp = 1)
    return (FC_LOCAL == fc && 1 != hqp);
    } // EventMap::isEventTeachFiducial(CoilMap::cm_cit cit)

  bool EventMap::isEventRemovePlow(CoilMap::cm_cit cit) const {
    std::string fc= coilMap_.GetFc(cit);
    // He inlet or outlet
    return (FC_INLET == fc || FC_OUTLET == fc);
    } // EventMap::isEventRemovePlow(CoilMap::cm_cit cit)

  bool EventMap::isEventHePipeInsulation(CoilMap::cm_cit cit) const {
    std::string fc= coilMap_.GetFc(cit);
    // He inlet or outlet
    return (FC_INLET == fc || FC_OUTLET == fc);
    } // EventMap::isEventHePipeInsulation(CoilMap::cm_cit cit)

  bool EventMap::isEventHePipeMeasure(CoilMap::cm_cit cit) const {
    std::string fc = coilMap_.GetFc(cit);
    // He outlet
    return (FC_OUTLET == fc);
  } // EventMap::isEventHePipeMeasure(CoilMap::cm_cit cit)

  bool EventMap::isEventOpenLandingRoller(CoilMap::cm_cit cit) const{
    std::string fc= coilMap_.GetFc(cit);
    // He inlet or outlet
    return (FC_INLET == fc || FC_OUTLET == fc);
    } // EventMap::isEventOpenLandingRoller(CoilMap::cm_cit cit)

  // When the odd layer is ending, the even layer is at the 0U, so the 
  // check for ending an odd layer is if the 0U is at an even layer joggle, 
  bool EventMap::isEventEndOddLayer(CoilMap::cm_cit cit) const {
    bool fcJ = (FC_JOGGLE == coilMap_.GetFc(cit) ? true : false); // is joggle
    bool evenLayer = (0 == coilMap_.GetLayer(cit) % 2 ? true : false); // is layer even?
    // Joggle and even layer
    return fcJ && evenLayer;
    } // EventMap::isEventEndOddLayer(CoilMap::cm_cit cit)

  bool EventMap::isEventLayerCompression(CoilMap::cm_cit cit) const {
    bool fcJ = (FC_JOGGLE == coilMap_.GetFc(cit) ? true : false); // is joggle
    long layerCheck= coilMap_.GetLayer(cit); // get layer
    // is this layer in the list of layes where coil compression happens
    bool isCorrectLayer = coilMap_.isInLaMeCo(layerCheck);      // Joggle and correct layer
    return fcJ && isCorrectLayer;
    } // EventMap::isEventLayerCompression(CoilMap::cm_cit cit)

  bool EventMap::isEventTurnMeasurement(CoilMap::cm_cit cit) const { 
    bool fcJ = (FC_JOGGLE == coilMap_.GetFc(cit) ? true : false); // is joggle
    long layerCheck= coilMap_.GetLayer(cit); // get layer
    bool isCorrectLayer = coilMap_.isInLaMeCo(layerCheck);  // is this layer in the list of layes where coil compression happens
    // Joggle and correct layer
    return fcJ && isCorrectLayer;
    } // EventMap::isEventTurnMeasurement(CoilMap::cm_cit cit)

  bool EventMap::isEventMoveEChain(CoilMap::cm_cit cit) const {
    CoilMap::fc_pair fcPair= coilMap_.GetCurrentNextFc(cit); // get current and next feature code
    long layerCheck= coilMap_.GetLayer(cit); // get layer
    // Joggle, but not the last joggle of the hex, and the last layer (end of the coil)
    // true if FC(current row) = J, FC(next row) <> L and Layer <> next to last (39)
    return FC_JOGGLE == fcPair.first && FC_LOCAL != fcPair.second && LAYERS_PER_COIL - 1 == layerCheck;
    } // EventMap::isEventMoveEChain(CoilMap::cm_cit cit)

  bool EventMap::isEventLeadEndgame(CoilMap::cm_cit cit) const {
    bool fcW = (FC_WINDING_LOCK == coilMap_.GetFc(cit) ? true : false); // is winding lock
    long layerCheck= coilMap_.GetLayer(cit); // get layer
    // winding lock and last layer
    return fcW && LAYERS_PER_COIL == layerCheck;
    } // EventMap::isEventLeadEndgame(CoilMap::cm_cit cit)

  // Landing roller moves to 40 degree position for inner turns (on the "move to inner" turn).
  // This happens when wrapping from outer turns in, which happens on odd turns.
  bool EventMap::isEventMoveLrInnerTurnPos(CoilMap::cm_cit cit) const {
    bool oddLayer = (1 == coilMap_.GetLayer(cit) % 2 ? true : false); // is layer odd?
    long turnCheck= coilMap_.GetTurn(cit); // get turn
    // odd layer and correct turn
    return oddLayer && LR_MV_TO_INNER_TURN == turnCheck;
    } // EventMap::isEventMoveLrInnerTurnPos(CoilMap::cm_cit cit)

  // Landing roller moves to 200 degree position for outer turns (on the "move to outer" turn).
  // This happens when wrapping from inner turns out, which happens on even turns.
  bool EventMap::isEventMoveLrToOuterTurnPos(CoilMap::cm_cit cit) const {
    bool evenLayer = (0 == coilMap_.GetLayer(cit) % 2 ? true : false); // is layer even?
    long turnCheck= coilMap_.GetTurn(cit); // get turn
    // even layer and correct turn
    return evenLayer && LR_MV_TO_OUTER_TURN == turnCheck;
    } // EventMap::isEventMoveLrToOuterTurnPos(CoilMap::cm_cit cit)


  // add the specified event ID to the event map at the specified angle
  void EventMap::AddEventToMap(double angle, long eventId, std::string logicTrace) {
    // populate the event value type
    eventValue_.first= angle;                 // angle
    eventValue_.second.get<0>()= eventId;     // Event Id
    eventValue_.second.get<1>()= logicTrace;  // logic trace

    // add the event data to the map
    eventMap_.insert(eventValue_);
    }

  void EventMap::MapEventInstances() {
    // traverses the coil map and determies which events happen at which angles
    // events that are needed at an angle are added to the event map
 
    // coil map constant iterator -- used to traverse the coil map -- const interator -- no changes to coil map allowed
    CoilMap::cm_cit cicm;

    // holds intermediate results for calculated event angle, layer and trans angle
    double eventAngle= 0.0;
    long thisLayer= 0;
    double finishAngle= 0.0;
    double floatPart= 0.0; // margin for Consolidation Event calc
    double intPart= 0.0; // integer part used in Consolidation Event calc

    // string to hold logic trace info
    std::string logicTrace= "";

    // keep track of if a particular event is needed or not
    bool eNeeded = false;

    // Post Mockup Update: Hqp and layer increment events are now inserted at the same
    // angle as is in the Scs Position table, as opposed to calculating an angle for each event
    // in the below for loop as is done for the other events.  For each angle in the member sets
    // create an event
    as_cit ascit;  // angle set iterator
    // Hqp Load Event
    std::cout << "Create HQP Load Events." << std::endl;
    for (ascit = hqpStartSet_.begin(); ascit != hqpStartSet_.end(); ++ascit) {
      // for each angle in the set, add event to map
      logicTrace = "Angle is from Scs Pos Table where isNewHqp is set.";
      AddEventToMap(*ascit, EID_HQP_LOAD, logicTrace);
      }

    // Layer Increment Event
    std::cout << "Create Layer Increment Events." << std::endl;
    for (ascit = layerStartSet_.begin(); ascit != layerStartSet_.end(); ++ascit) {
      // for each angle in the set, add event to map
      logicTrace = "Angle is from Scs Pos Table where isNewLayer is set.";
      AddEventToMap(*ascit, EID_LAYER_INCREMENT, logicTrace);
      }

    // variables to deal with displaying progress
    double iterations= static_cast<double>(coilMap_.mapCoil_.size()); // number elements in the coil map -- use double so pctDone does not use interger math
    long count = 0;  // loop counter for display purposes
    long pctDone = 0;  // percent done

    // display progress 
    std::cout << "Traversing coil map to create events." << std::endl;
    std::cout << "There are " << static_cast<long>(iterations) << " angles to process between angles " << coilMap_.GetAngle(coilMap_.mapCoil_.begin()) <<
      " and " << coilMap_.GetAngle(coilMap_.mapCoil_.rbegin()) << std::endl;

    // traverse the coil map from beginning to end
    // see which, if any, events need to be mapped for each entry in the coil map
    // if an event needs to be added, then calculate the angle and add it to the map
    for ( cicm= coilMap_.mapCoil_.begin(); cicm != coilMap_.mapCoil_.end(); ++cicm ) {
      // display progress
      ++count;
      pctDone = static_cast<long>(round(100.0 * (count / iterations)));  // truncate fractional percentages
      std::cout << "On angle " << coilMap_.GetAngle(cicm) << " (" << pctDone << " %)\r" << std::flush; // no linefeed so this line will be overwritten next time thru

      // test for each type of event and add to map if needed
      // Post Mockup Update:  Layer Increment events are now added based on new layer angles
      // from the SCS positon map, and no longer incrementally determined.  The angles are stored
      // in the member set layerStartSet_. See above.  Code bypassed, but left here so old algorithm can be seen.
      // is Increment Layer Event needed
      // eNeeded= isEventLayerIncrement(cicm); // need to add to map if true
      eNeeded = false;
      if (eNeeded) {
        // calc angle = current angle + plow offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_PLOW;
        // add event to map
        logicTrace = "";
        AddEventToMap(eventAngle, EID_LAYER_INCREMENT, logicTrace);
        } // event needed
      
      // is consolodation event needed
	    // Post Mockup Update: Colsolidate Layer Events are no longer needed
	    // since consolidation is done "continuously" rather than at discrete steps.
      // eNeeded= isEventConsolidateOdd(cicm); // need to add to map if true
      // Force false, but leave here to show logic.
	    eNeeded = false;	// force false to eliminate the event.
      if (eNeeded) {
        // calc angle = ?
        eventAngle= coilMap_.GetAngle(cicm);
        floatPart= modf(((eventAngle-20.0) / 40.0), &intPart);  // get the remainder of (coil angle-20) / 40
        floatPart= modf((1.0 - floatPart), &intPart);  // get remainder of 1 - previous remainder
        floatPart= 40 * floatPart;  // HS Angle
        eventAngle= eventAngle + OFFSET_LANDED_TURN + floatPart + ANGLE_OFFSET_CONSOLIDATION;
        thisLayer= coilMap_.GetLayer(cicm);
        finishAngle= coilMap_.GetAngleOl14T(thisLayer);
        for ( ; eventAngle <= finishAngle; eventAngle += CONSOLIDATION_INTERVAL) { // event angle is initialized above. Loop thru range of angles and create a consolicaiton event every increment
          // add event to map
          logicTrace = "";
          AddEventToMap(eventAngle, EID_CONSOLIDATE_ODD, logicTrace);
          } // for loop
        } // event needed

      // is load hex/quad event needed
      // Post Mockup Update:  Load Hqp events are now added based on new hqp angles
      // from the SCS positon map, and no longer incrementally determined.  The angles are stored
      // in the member set hqpStartSet_. See above.  Code bypassed, but left here so old algorithnm can be seen.
      // eNeeded= isEventHqpLoad(cicm); // need to add to map if true
      eNeeded = false; // force to false to bypass this code
      if (eNeeded) {
        // calc angle = current angle + 0U offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_0U;
        // add event to map
        logicTrace = "";
        AddEventToMap(eventAngle, EID_HQP_LOAD, logicTrace);
        } // event needed

      // is teach fiducial laser position event needed
      eNeeded= isEventTeachFiducial(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + fiducial laser event offset + offset from local zero. Local
        // zero offset needed because the 0U roller is at the local zero when the check is made.
        eventAngle = coilMap_.GetAngle(cicm) + OFFSET_FIDUCIAL_LASER + FIDUCIAL_LASER_EVENT_LOCAL_OFFSET;
        // add event to map
        logicTrace = "";
        AddEventToMap(eventAngle, EID_TEACH_FIDUCIAL, logicTrace);
        } // event needed

      // is remove plow event needed
  	  // Post Mockup Update: Remove Plow Events are no longer needed
	    // since the plow was moved, and used differently.
      //eNeeded= isEventRemovePlow(cicm); // need to add to map if true
	    eNeeded = false;	// force false to eliminate the event.
      if (eNeeded) {
        // calc angle = current angle + plow offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_PLOW - ANGLE_OFFSET_SMALL;
        // add event to map
        logicTrace = "";
        AddEventToMap(eventAngle, EID_REMOVE_PLOW, logicTrace);
        } // event needed
      
      // Move landing roller events. 
      // See if move to the inner turn position (40 deg) is needed, 
      // and add it if necessary.
      eNeeded = isEventMoveLrInnerTurnPos(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + LR Odd layer offset
        eventAngle = coilMap_.GetAngle(cicm) + LR_MV_TO_INNER_TURN_OFFSET;
        logicTrace = "";
        AddEventToMap(eventAngle, EID_MOVE_LR_TO_INNER_TURN_POS, logicTrace);
        }

      // See if move to the outer turn position (200 deg) is needed, 
      // and add it if necessary.
      eNeeded = isEventMoveLrToOuterTurnPos(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + LR Even layer offset
        eventAngle = coilMap_.GetAngle(cicm) + LR_MV_TO_OUTER_TURN_OFFSET;
        logicTrace = "";
        AddEventToMap(eventAngle, EID_MOVE_LR_TO_OUTER_TURN_POS, logicTrace);
        }

      // Post-Mockup Update: Make compression, turn measure, and He Measure
      // Coincident with end of layer events.
      // Post-CSM1 Update: Make end of layer events use the now dynamic landing
      // roller offsets.  On odd layers, the landing roller will set for inner
      // turns (the 40 degree position), so use the LR inner turn offset.
      // On even layers, the landing roller will be set for outer turns (the
      // 200 degree location), so use the LR outer turn offset.
      // End the layer before the LR gets to the joggle by the end of layer LR
      // joggle offset.
      eNeeded = isEventEndOddLayer(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landing roller offset - joggle offset
        eventAngle = coilMap_.GetAngle(cicm) + LR_INNER_TURN_OFFSET - END_LAYER_LR_JOGGLE_NOM_OFFSET;
        // add end of layer event to map
        logicTrace = "Used LR inner turn offset";
        AddEventToMap(eventAngle, EID_END_ODD_LAYER, logicTrace);

        // see if a compression event is needed
        eNeeded = isEventLayerCompression(cicm); // need to add to map if true
        if (eNeeded) {
          // use eol angle. Add a bit so this event is shown after eol.
          // add event to map
          logicTrace = "coincident with end of odd layer";
          AddEventToMap(eventAngle + 0.001, EID_LAYER_COMPRESSION, logicTrace);
          } // compression event needed

        // see if a turn measurment event is needed
        eNeeded = isEventTurnMeasurement(cicm); // need to add to map if true
        if (eNeeded) {
          // use eol angle. Add a bit so this event is shown after eol.
          // add event to map
          logicTrace = "coincident with end of odd layer";
          AddEventToMap(eventAngle + 0.001, EID_TURN_MEASUREMENT, logicTrace);
          } // turn measurement event needed
  
        // see if a HeMeasurement is needed
        // eNeeded = isEventHePipeMeasure(cicm); // need to add to map if true
        // He measure is only needed at the end of even layers
        eNeeded= false; // force false to bypass
        if (eNeeded) {
          // use eol angle. Add a bit so this event is shown after eol.
          // add event to map
          logicTrace = "coincident with end of odd layer";;
          AddEventToMap(eventAngle + 0.001, EID_HE_PIPE_MEASURE, logicTrace);
          } // He measurement event needed

        } // end of odd layer event needed

		  // is end of even layer event
	    eNeeded = isEventEndEvenLayer(cicm); // need to add to map if true
	    if (eNeeded) {
        // calc angle = current angle + landing roller offset - joggle offset
		    eventAngle = coilMap_.GetAngle(cicm) + LR_OUTER_TURN_OFFSET - END_LAYER_LR_JOGGLE_NOM_OFFSET;
		    // add end of layer event to map
        logicTrace = "Used LR outer turn offset";
        AddEventToMap(eventAngle, EID_END_EVEN_LAYER, logicTrace);

        // see if a compression event is needed
        eNeeded = isEventLayerCompression(cicm); // need to add to map if true
        if (eNeeded) {
          // use eol angle. Add a bit so this event is shown after eol.
          // add event to map
          logicTrace = "coincident with end of even layer";
          AddEventToMap(eventAngle + 0.001, EID_LAYER_COMPRESSION, logicTrace);
        } // compuression event needed

        // see if a turn measurment event is needed
        eNeeded = isEventTurnMeasurement(cicm); // need to add to map if true
        if (eNeeded) {
          // use eol angle. Add a bit so this event is shown after eol.
          // add event to map
          logicTrace = "coincident with end of even layer";
          AddEventToMap(eventAngle + 0.001, EID_TURN_MEASUREMENT, logicTrace);
        } // turn measurement event needed

        // see if a HeMeasurement is needed
        // He is always needed a the end of event layers
        // eNeeded = isEventHePipeMeasure(cicm); // need to add to map if true
        eNeeded= true; // force addition of event.
        if (eNeeded) {
          // use eol angle. Add a bit so this event is shown after eol.
          // add event to map
          logicTrace = "coincident with end of even layer";
          AddEventToMap(eventAngle + 0.001, EID_HE_PIPE_MEASURE, logicTrace);
        } // He measurement event needed

      } // end of even layer event needed


	  	// is He pipe insulation needed
      eNeeded= isEventHePipeInsulation(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + 2U offset - large offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_2U + ANGLE_OFFSET_HE_PIPE;
        // add event to map
        logicTrace = "";
        AddEventToMap(eventAngle, EID_HE_PIPE_INSULATION, logicTrace);
        } // event needed

      // is Landing Roller open needed.
      // Post CSM1 Update: LR is now has two different locations based on even
      // or odd layers. The LR gets opened because there is a He Inlet or Outlet. 
      // These He pipes are only on inner most or outer most turns, but in the
      // coil map, inlets and outlets are right before or after joggles,
      // throwing off the even/odd layer determination. Instead use the turn number
      // of the inlet/outlet. This won't increment on an inlet/outlet, because they
      // have a row in the coil map distinct from a joggle. If the turn number is 
      // small, use the LR offset for inner turns, if the turn number is big, 
      // use the LR offset for outer turns.
      eNeeded= isEventOpenLandingRoller(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landing roller offset - small offset
        // Determine which LR offset to use by looking  at the turn number
        long turn= coilMap_.GetTurn(cicm);
        if (turn <= LR_MV_TO_OUTER_TURN) { // outer turn 
          eventAngle= coilMap_.GetAngle(cicm) + LR_OUTER_TURN_OFFSET - ANGLE_OFFSET_SMALL;
          logicTrace = "Used LR outer turn offset";
          }
        else {// inner turn
          eventAngle= coilMap_.GetAngle(cicm) + LR_INNER_TURN_OFFSET - ANGLE_OFFSET_SMALL;
          logicTrace = "Used LR inner turn offset";
          }
        // add event to map
        AddEventToMap(eventAngle, EID_OPEN_LANDING_ROLLER, logicTrace);
        } // event needed

      // is Move Energy Chain needed
      eNeeded= isEventMoveEChain(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landed turn offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_0U;
        // add event to map
        logicTrace = "";
        AddEventToMap(eventAngle, EID_MOVE_ECHAIN, logicTrace);
        } // event needed

      // is Long lead end game needed
      eNeeded= isEventLeadEndgame(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landed turn offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_0U + ANGLE_OFFSET_COIL_END;
        // add event to map
        logicTrace = "";
        AddEventToMap(eventAngle, EID_LONG_LEAD_ENDGAME, logicTrace);
        } // event needed

      } // the for loop, traversing the map

    // end the line and add an extra spece after the output in the for loop.
    std::cout << std::endl << std::endl;

    } // EventMap::CalculateEventInstances()

  // DB related functions
  // Return values indicate success or error.
  long EventMap::DbConnect() {
    // return value indicates success or error

    // connects to the database

    // variable to hold return value
    long rtnValue= 0;
    try {
      // connect to database
      // Client type was specified in constructor
      dbConnection_.Connect(serverText_.c_str(),     // server_name@database_name
                            DB_USER_NAME.c_str(),   // user name
                            DB_PASSWORD.c_str());   // password
      // set return value for all okay
      rtnValue= RTN_NO_ERROR;
      }
    catch(SAException &ex) {
      // SAConnection::Rollback()
      // can also throw an exception
      // (if a network error for example),
      // we will be ready
      try {
        // on error rollback changes
        dbConnection_.Rollback();
        }
      catch(SAException &) {
        }
      // get error message
      errorText_= (const char*)ex.ErrText();
      // output the error text
      std::cout << errorText_ << std::endl;
      // set return value to indicate an error
      rtnValue= RTN_ERROR;
      }
    return rtnValue;
    } // AxesPositions::DbConnect()

  long EventMap::DbDisconnect() {
    // return value indicates success or error

    // disconnects from the data base

    // variable to hold return value
    long rtnValue = 0;
    try {
      // disconnect
      dbConnection_.Disconnect();
      // set return value for all okay
      rtnValue = RTN_NO_ERROR;
    }
    catch (SAException &ex) {
      // get error message
      errorText_ = (const char*)ex.ErrText();
      // output the error text
      std::cout << errorText_ << std::endl;
      // set return value to indicate an error
      rtnValue = RTN_ERROR;
    }
    return rtnValue;
  } // AxesPositions::DbDisconnect()

  // executes the specified stored procedure, which is expected to return results (SELECT...) 
  long EventMap::QueryDb(const std::string &sprocName) {
    // return value indicates success or error

    // query the db using teh specified stored procedure, 
    // presumably a SELECT command to fetch data

    // variable to hold return value
    long rtnValue = 0;

    // Set the command text of the command object
    dbCommand_.setCommandText(sprocName.c_str(), SA_CmdStoredProc);

    try {
      // execute the command
      dbCommand_.Execute();
      // set return value for all okay
      rtnValue = RTN_NO_ERROR;
    }
    catch (SAException &ex) {
      // get error message
      errorText_ = (const char*)ex.ErrText();
      // output the error text
      std::cout << errorText_ << std::endl;
      // set return value to indicate an error
      rtnValue = RTN_ERROR;
    }
    return rtnValue;
  } // QueryDb(const std::string &sprocName)

  // Get hqp start angles. 
  // This functions relys on the result set being in the command member object
  // as a result of  the QueryDb call (do this call before calling this function.)
  long EventMap::PopulateHqpStartSet() {
    // Populates a set of hqp start angles from the Scs Position table sorted ascendingly
    // return value indicates success or error

    // QueryDb() is called with the correct stored procedure name prior to this function being called.
    // The query returns a sorted list of angles which correspond to the start of HQPs
    // Put the values fetched by the QueryDb() into a set
    // QueryDb() should return one or more rows.  Map every row returned.

    // variable to hold return value
    long rtnValue = 0;
    try {
      if (dbCommand_.isResultSet()) {
        // if there is a result set
        // create local variables for the angle
        while (dbCommand_.FetchNext()) {
          // get angle
          // add the angle to the set
          // use the end of the set as the insertion hint for optimum performance
          hqpStartSet_.insert(hqpStartSet_.end(), dbCommand_.Field(SAP_RIAANGLE_PARAM.c_str()).asDouble());  // insert the retreived angle into the set
        }
        // set return value for all okay
        rtnValue = RTN_NO_ERROR;
      }
      else {
        // set return value to error for no results present
        rtnValue = RTN_NO_RESULTS;
      }
    }
    catch (SAException &ex) {
      // get error message
      errorText_ = (const char*)ex.ErrText();
      // output the error text
      std::cout << errorText_ << std::endl;
      // set return value to indicate an error
      rtnValue = RTN_ERROR;
    }
    return rtnValue;
  }

  // Get layer start angles
  // This functions relys on the result set being in the command member object
  // as a result of  the QueryDb call (do this call before calling this function.)
  long EventMap::PopulateLayerStartSet() {
    // Populates a set of layer start angles from the Scs Position table sorted ascendingly
    // return value indicates success or error

    // QueryDb() is called with the correct stored procedure name prior to this function being called.
    // The query returns a sorted list of angles which correspond to the start of HQPs
    // Put the values fetched by the QueryDb() into a set
    // QueryDb() should return one or more rows.  Map every row returned.

    // variable to hold return value
    long rtnValue = 0;
    try {
      if (dbCommand_.isResultSet()) {
        // if there is a result set
        // create local variables for the angle
        while (dbCommand_.FetchNext()) {
          // get angle
          // add the layer and associated angle to the map
          // use the end of the set as the insertion hint for optimum performance
          layerStartSet_.insert(layerStartSet_.end(), dbCommand_.Field(SAP_RIAANGLE_PARAM.c_str()).asDouble());  // insert the retreived angle into the set
        }
        // set return value for all okay
        rtnValue = RTN_NO_ERROR;
      }
      else {
        // set return value to error for no results present
        rtnValue = RTN_NO_RESULTS;
      }
    }
    catch (SAException &ex) {
      // get error message
      errorText_ = (const char*)ex.ErrText();
      // output the error text
      std::cout << errorText_ << std::endl;
      // set return value to indicate an error
      rtnValue = RTN_ERROR;
    }
    return rtnValue;
  }
  // Insert a row at the angle. Use the passed in event id.
  // Execute the specified stored procedure to do the insert
  // It is assumed the connection is already done, and that sometime after the call a disconnect is performed.
  long EventMap::InsertIntoDb(double angle, long eventId, const std::string &sprocName, const std::string& trace) {
    // execute the specified stored procedure to do the insert
    // return value indicates success or error

    // variable to hold return value
    long rtnValue= 0;

    // Set the command text of the command object
    dbCommand_.setCommandText(sprocName.c_str(), SA_CmdStoredProc);

    try {
      // set the input parameters
      dbCommand_.Param("eventId").setAsLong() = eventId;
      dbCommand_.Param("angle").setAsDouble() = angle;
      dbCommand_.Param("logicTrace").setAsString() = trace.c_str();

      // execute the command
      dbCommand_.Execute();
      // set return value for all okay
      rtnValue= RTN_NO_ERROR;
      }
    catch(SAException &ex) {
      // get error message
      errorText_= (const char*)ex.ErrText();
      // output the error text
      std::cout << errorText_ << std::endl;
      // set return value to indicate an error
      rtnValue= RTN_ERROR;
      }
    return rtnValue;
    } // EventMap::InsertIntoDb(double angle, const std::string& trace, long EventId)

  // Iterate thru the event map and insert a row for each map entry.
  // Execute the specified stored procedure to do the insert
  // It is assumed the connection is already done, and that sometime after the call a disconnect is performed.
  long EventMap::InsertIntoDb(const std::string &sprocName) {
    // flag to keep track of an error occuring
    bool errorFlag = false;
    // variable to hold return value
    long rtnValue= 0;

    // variables to deal with displaying progress
    double records = static_cast<double>(eventMap_.size()); // number of records to insert -- use double so pctDone does not use interger math
    long count = 0;  // loop counter for display purposes
    double pctDone = 0;  // percent done

    std::cout << "There are " << static_cast<long>(records) << " records to insert." << std::endl;

    for(EventMap::em_const_iter emci = eventMap_.begin(); emci != eventMap_.end(); ++emci) {
      // display progress
      ++count;
      pctDone = static_cast<long>(100.0 * (count / records));
      std::cout << "On record " << count << " of " << records << " (" << pctDone << " %)\r" << std::flush;

      // do the insert
      rtnValue = InsertIntoDb(emci->first, emci->second.get<0>(), sprocName, emci->second.get<1>().c_str());
      if (rtnValue != RTN_NO_ERROR)
        errorFlag = true;
      } // for loop

    // end the line and add an extra spece after the output in the for loop.
    std::cout << std::endl << std::endl;

    if (!errorFlag) // no error flag, return ok
      return RTN_NO_ERROR;
    else  // error
      return RTN_ERROR;
    } // EventMap::InsertIntoDb() 

  // delete all rows from event list table
  // that are not referenced in the event history or
  // action history tables (i.e. are not done or partially done)
  // assumes valid connection has been made
  // return value indicates success or error
  long EventMap::DeleteAllUndoneEvents() {

    // variable to hold return value
    long rtnValue= RTN_ERROR;

    try {
      // Set the command text of the command object
      dbCommand_.setCommandText(SPNAME_DELETE_ALL_INC_EVENTS.c_str(), SA_CmdStoredProc);

      // there are no input parameters

      // execute the command
      dbCommand_.Execute();
      // set return value for all okay
      rtnValue= RTN_NO_ERROR;
    }
    catch (SAException &ex) {
      // get error message
      errorText_= (const char*)ex.ErrText();
      // output the error text
      std::cout << errorText_ << std::endl;
      // set return value to indicate an error
      rtnValue= RTN_ERROR;
    }
    return rtnValue;
  } // EventMap::DeleteAllUndoneEvents()

} // namespace gaScsData
