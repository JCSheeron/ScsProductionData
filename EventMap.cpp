/********************************************************************
 * COPYRIGHT -- General Atomics
 ********************************************************************
 * Library: 
 * File: AxesPositions.cpp
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
    long status= 0;
    // connect to db
    status= DbConnect();

    // if status is okay
      // 1) populate the coil map
      // 2) iterate thru the coil map and calculate the event instances -- populate the Event Map
      // 3) Iterate thru the event map, and insert the events into the DB
    if (RTN_NO_ERROR == status) {
      // if no error (DB connect was sucessful)
      coilMap_.PopulateCoilMap();
      MapEventInstances();
      InsertIntoDb(SPNAME_INSERT_EVENTLIST);  // iterate thru the event map and insert a db row for each event
      }

    // is status is okay (connection was sucessful), disconnect from the db
    if (RTN_NO_ERROR == status)
      status= DbDisconnect();

    // TODO: determine proper return values
    if (RTN_NO_ERROR == status)
      return RTN_NO_ERROR;
    else
      return RTN_ERROR;
    } //EventMap::GenerateEventMapTable()

  bool EventMap::isEventLayerIncrement(CoilMap::cm_cit cit) const {
    CoilMap::fc_pair fcPair= coilMap_.GetCurrentNextFc(cit); // get current and next feature code
    long layerCheck= coilMap_.GetLayer(cit); // get layer
    // Joggle, but not the last joggle of the hex, and not last layer (i.e. not end of the coil)
    // true if FC(current row) = J, FC(next row) <> L and Layer <> next to last (39)
    return FC_JOGGLE == fcPair.first && FC_LOCAL != fcPair.second && LAYERS_PER_COIL - 1 != layerCheck;
    } // EventMap::isEventLayerIncrement(CoilMap::cm_cit cit)

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
    CoilMap::fc_pair fcPair= coilMap_.GetCurrentNextFc(cit); // get current and next feature code
    // joggle before a new hex (i.e. last joggle)
    // FC (current row) = J AND FC (next row) = L
    return FC_JOGGLE == fcPair.first && FC_LOCAL == fcPair.second;
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

  bool EventMap::isEventOpenLandingRoller(CoilMap::cm_cit cit) const{ 
    std::string fc= coilMap_.GetFc(cit);
    // He inlet or outlet
    return (FC_INLET == fc || FC_OUTLET == fc);
    } // EventMap::isEventOpenLandingRoller(CoilMap::cm_cit cit)

  bool EventMap::isEventEndOddLayer(CoilMap::cm_cit cit) const {
    bool fcJ = (FC_JOGGLE == coilMap_.GetFc(cit) ? true : false); // is joggle
    bool evenLayer = (0 == coilMap_.GetLayer(cit) % 2 ? true : false); // is layer even?
    // Joggle and even layer
    return fcJ && evenLayer;
    } // EventMap::isEventEndOddLayer(CoilMap::cm_cit cit)

  bool EventMap::isEventLayerCompression(CoilMap::cm_cit cit) const {
    bool fcJ = (FC_JOGGLE == coilMap_.GetFc(cit) ? true : false); // is joggle
    long layerCheck= coilMap_.GetLayer(cit); // get layer
    bool isCorrectLayer = coilMap_.isInLaMeCo(layerCheck);  // is this layer in the list of layes where coil compression happens
    // Joggle and correct layer
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

    // holds intermediat results for calculated event angle, layer and trans angle
    double eventAngle= 0.0;
    long thisLayer= 0;
    double finishAngle= 0.0;
    double floatPart= 0.0; // margin for Consolidation Event calc
    double intPart= 0.0; // integer part used in Consolidation Event calc

    // string to hold logic trace info
    std::string logicTrace= "";

    // keep track of if a particular event is needed or not
    bool eNeeded = false;

    // traverse the coil map from beginning to end
    // see which, if any, events need to be mapped for each entry in the coil map
    // if an event needs to be added, then calculate the angle and add it to the map
    for ( cicm= coilMap_.mapCoil_.begin(); cicm != coilMap_.mapCoil_.end(); ++cicm ) {
      // test for each type of event and add to map if needed
      // is Increment Layer Event needed
      eNeeded= isEventLayerIncrement(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + plow offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_PLOW;
        // add event to map
        AddEventToMap(eventAngle, EID_LAYER_INCREMENT, logicTrace);
        } // event needed
      
      // is even layer end event
      eNeeded= isEventEndEvenLayer(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landing roller offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_LANDING_ROLLER - ANGLE_OFFSET_SMALL;
        // add event to map
        AddEventToMap(eventAngle, EID_END_EVEN_LAYER, logicTrace);
        } // event needed

      // is consolodation event needed
      eNeeded= isEventConsolidateOdd(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = ?
        // TODO: Correct event angle Al Gore Rhythm?
        eventAngle= coilMap_.GetAngle(cicm);
        floatPart= modf(((eventAngle-20.0) / 40.0), &intPart);  // get the remainder of (coil angle-20) / 40
        floatPart= modf((1.0 - floatPart), &intPart);  // get remainder of 1 - previous remainder
        floatPart= 40 * floatPart;  // HS Angle
        eventAngle= eventAngle + OFFSET_LANDED_TURN + floatPart - ANGLE_OFFSET_CE;
        thisLayer= coilMap_.GetLayer(cicm);
        finishAngle= coilMap_.GetAngleOl14T(thisLayer);
        for ( ; eventAngle <= finishAngle; eventAngle += CONSOLIDATION_INTERVAL) { // event angle is initialized above. Loop thru range of angles and create a consolicaiton event every increment
          // add event to map
          AddEventToMap(eventAngle, EID_CONSOLIDATE_ODD, logicTrace);
          } // for loop
        } // event needed

      // is load hex/quad event needed
      eNeeded= isEventHqpLoad(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + 0U offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_0U;
        // add event to map
        AddEventToMap(eventAngle, EID_HQP_LOAD, logicTrace);
        } // event needed

      // is teach fiducial laser position event needed
      eNeeded= isEventTeachFiducial(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + fiducial laser offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_FIDUCIAL_LASER;
        // add event to map
        AddEventToMap(eventAngle, EID_TEACH_FIDUCIAL, logicTrace);
        } // event needed

      // is remove plow event needed
      eNeeded= isEventRemovePlow(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + plow offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_PLOW - ANGLE_OFFSET_SMALL;
        // add event to map
        AddEventToMap(eventAngle, EID_REMOVE_PLOW, logicTrace);
        } // event needed

      // is He pipe insulation needed
      eNeeded= isEventHePipeInsulation(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + 2U offset - large offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_2U - ANGLE_OFFSET_LARGE;
        // add event to map
        AddEventToMap(eventAngle, EID_HE_PIPE_INSULATION, logicTrace);
        } // event needed

      // is Landing Roller open needed
      eNeeded= isEventOpenLandingRoller(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landing roller offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_LANDING_ROLLER - ANGLE_OFFSET_SMALL;
        // add event to map
        AddEventToMap(eventAngle, EID_OPEN_LANDING_ROLLER, logicTrace);
        } // event needed

      // is end of odd layer needed
      eNeeded= isEventEndOddLayer(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landing roller offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_LANDING_ROLLER - ANGLE_OFFSET_SMALL;
        // add event to map
        AddEventToMap(eventAngle, EID_END_ODD_LAYER, logicTrace);
        } // event needed

      // is compression measurement layer
      eNeeded= isEventLayerCompression(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landed turn offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_LANDED_TURN - ANGLE_OFFSET_SMALL;
        // add event to map
        AddEventToMap(eventAngle, EID_LAYER_COMPRESSION, logicTrace);
        } // event needed

      // is measurement layer
      eNeeded= isEventTurnMeasurement(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landed turn offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_LANDED_TURN - ANGLE_OFFSET_SMALL;
        // add event to map
        AddEventToMap(eventAngle, EID_TURN_MEASUREMENT, logicTrace);
        } // event needed

      // is Move Energy Chain needed
      eNeeded= isEventMoveEChain(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landed turn offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_0U;
        // add event to map
        AddEventToMap(eventAngle, EID_MOVE_ECHAIN, logicTrace);
        } // event needed

      // is Long lead end game needed
      eNeeded= isEventLeadEndgame(cicm); // need to add to map if true
      if (eNeeded) {
        // calc angle = current angle + landed turn offset - small offset
        eventAngle= coilMap_.GetAngle(cicm) + OFFSET_0U - ANGLE_OFFSET_SMALL;
        // add event to map
        AddEventToMap(eventAngle, EID_LONG_LEAD_ENDGAME, logicTrace);
        } // event needed

      } // traversing the map
    } // EventMap::CalculateEventInstances()

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

  // insert an event Id at the angle
  long EventMap::InsertIntoDb(double angle, long eventId, const std::string &sprocName, const std::string& trace) {
    // execute the specified stored procedure to do the insert
    // return value indicates success or error

    // variable to hold return value
    long rtnValue= 0;

    // Set the command text of the command object
    dbCommand_.setCommandText(SPNAME_INSERT_EVENTLIST.c_str(), SA_CmdStoredProc);

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

  // insert a row at the angle. Use values of the foot and columm position member vectors
  // execute the specified stored procedure to do the insert
//  long EventMap::InsertIntoDb(long angle, long eventId, const std::string &sprocName, const std::string& trace) {
//    return InsertIntoDb(static_cast<double>(angle), eventId, sprocName, trace);
//    } // EventMap::InsertIntoDb(long angle, const std::string& trace, long EventId)

  // Iterate thru the event map and insert a row for each map entry
  // execute the specified stored procedure to do the insert
  long EventMap::InsertIntoDb(const std::string &sprocName) {
    // flag to keep track of an error occuring
    bool errorFlag = false;
    // variable to hold return value
    long rtnValue= 0;
    for(EventMap::em_const_iter emci = eventMap_.begin(); emci != eventMap_.end(); ++emci) {
      rtnValue = InsertIntoDb(emci->first, emci->second.get<0>(), sprocName, emci->second.get<1>().c_str());
      if (rtnValue != RTN_NO_ERROR)
        errorFlag = true;
      }
    if (!errorFlag) // no error flag, return ok
      return RTN_NO_ERROR;
    else  // error
      return RTN_ERROR;
    } // EventMap::InsertIntoDb() 


  long EventMap::DbDisconnect() {
    // return value indicates success or error

    // disconnects from the data base

    // variable to hold return value
    long rtnValue= 0;
    try {
      // disconnect
      dbConnection_.Disconnect();
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
    } // AxesPositions::DbDisconnect()


} // namespace gaScsData