/********************************************************************
 * COPYRIGHT -- General Atomics
 ********************************************************************
 * Library: 
 * File: CoilMap.cpp
 * Author: J. Sheeron (x2315)
 * Created: March 05, 2014
  *******************************************************************
 * Function:  The CoilMap class connects to the database, and queries the coil map table
 *            populating a boost library map with the coil map data.  The map uses the coil
 *            angle as the key, and a tuple as the value.  The tuple holds in order:
 *            the feature code, hex/quad pancake, layer, turn, azimuth, nominal radius
 *        BUG: LB logic is erronous in the case where the passed in angle matches the last angle (row) in the map.
 *             in this case, the LB functions behave as if there is no LB, but really this last row should count
 *
 * Libraries used:  string
 *                  SQLAPI.h
 *                  Boost Non-copyable
 *                  Boost Map Container
 *                  Boost tuple
 *******************************************************************/

 // Precompiled header
#include "pch.hpp"

// header file
#include "gaScsDataConstants.hpp"
#include "CoilMap.hpp"

namespace gaScsData {

// ctors and dtor
CoilMap::CoilMap() :
//    hqpStartPrev_(INITIAL_NO_POSITION), // first hqp will be start at 0, so set the previous so 0 will be seen as a new start
    serverText_(DB_SERVER_NAME + "@" + DB_DATABASE_NAME),
    errorText_("") {
  // initalize container of layer numbers indicating when coil measurement and compression occur
  // allocate space for best performance
  laMeCo_.reserve(NUM_OF_LA_ME_CO);
  // assign the array elements to the member container
  for (size_t i = 0; i < NUM_OF_LA_ME_CO; ++i) { // go thru list of layers, and insert into the set with a hint
    laMeCo_.insert(laMeCo_.end(), LA_ME_CO[i]);
    }

  // reserve space for the number of odd layer turn 14 transitions
  mapOl14T_.reserve(MAX_NUM_OF_CNOLT14FCT);

  // reserve space for number of joggle angles
  setJoggleAngles_.reserve(MAX_NUM_OF_JOGGLE_ANGLES);

  // initialize database connectivity parameters
  // use SQL server native client
  dbConnection_.setClient(SA_SQLServer_Client);
  // Set the connection to use the ODBC API
  // ODBC is default API, so this is only being specified for backward compatibility and documentation.
  dbConnection_.setOption( "UseAPI" ) = "ODBC";
  // Set the commnad object to use the connection
  dbCommand_.setConnection(&dbConnection_);
  }
  
CoilMap::~CoilMap() { }

long CoilMap::PopulateCoilMap() {
  // return value indicates success or error
  long connectStatus= 0;
  long queryStatus= 0;
  long opStatus= 0; // operation status
  long disconnectStatus= 0;
  
  // connect to db
  connectStatus= DbConnect();

  // if connect status is okay, query the db to get the coil map
  if (RTN_NO_ERROR == connectStatus) {
    queryStatus= QueryDb(SPNAME_SELECT_COIL_MAP);
    }
  // if connect and query status is okay, fetch the data from the query
  // and put it into the coil map
  if (RTN_NO_ERROR == connectStatus && 
      RTN_NO_ERROR == queryStatus) {
    opStatus= MapFeature();
    }
    
  // if connect status, previous query, and previous operation were all okay,
    // continue and query the db to get the turnAngle map (odd layer, 14th turns, Feature Code = T only)
  if (RTN_NO_ERROR == connectStatus &&
      RTN_NO_ERROR == queryStatus &&
      RTN_NO_ERROR == opStatus) {
    queryStatus= QueryDb(SPNAME_SELECT_CMOLT14FCT);
    }
    
  // if connect status, previous query, and previous operation were all okay,
    // continue and fetch the data from the query and put it into the turnAngle map
  if (RTN_NO_ERROR == connectStatus &&
      RTN_NO_ERROR == queryStatus &&
      RTN_NO_ERROR == opStatus) {
    opStatus= MapCmOlT14FcT();
    }

  // if connect status, previous query, and previous operation were all okay,
  // continue and query the db to get the joggle angles
  if (RTN_NO_ERROR == connectStatus &&
      RTN_NO_ERROR == queryStatus &&
      RTN_NO_ERROR == opStatus) {
    queryStatus= QueryDb(SPNAME_SELECT_JOGGLE_ANGLES);
    }
    
  // if connect status, previous query, and previous operation were all okay,
  // continue and put it into the Joggle Angles set
  if (RTN_NO_ERROR == connectStatus &&
      RTN_NO_ERROR == queryStatus &&
      RTN_NO_ERROR == opStatus) {
    opStatus= PopulateJoggleAngleSet();
    }
    
    
  // disconnect from the db if connection was successful
  if (RTN_NO_ERROR == connectStatus) {
    disconnectStatus= DbDisconnect();
    }
    
  // if all status is okay, return no error, otherwise return error
  if (RTN_NO_ERROR == connectStatus &&
    RTN_NO_ERROR == queryStatus &&
    RTN_NO_ERROR == opStatus &&
    RTN_NO_ERROR == disconnectStatus) {
    std::cout << "Done with no errors." << std::endl;
    return RTN_NO_ERROR;
    }
  else {
    std::cout << "An error has occurred!!." << std::endl;
    return RTN_ERROR;
    }
  } // PopulateCoilMap

// accessor functions
std::string CoilMap::GetErrorText() const { return errorText_; }

// these accessor functions get property for the row with the specified angle, or 
// the previous or equal to the passed in angle (i.e. "not after", or the Lower Bound, Hence the Lb ending), 
// or the row after the passed in angle (i.e. "not before" or the Upper Bound, hence the Ub ending).
// BUG: LB logic is erronous in the case where the passed in angle matches the last angle (row) in the map.
  // in this case, the LB functions behave as if there is no LB, but really this last row should count
// fhltar structure is the feature code, hex/quad number, layer, turn, azimuth, nominal radius
CoilMap::fhltar CoilMap::GetFhltar(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the iterator for the specified angle
  // pass the iterator to the overload
  return GetFhltar(cit);
  }

CoilMap::fhltar CoilMap::GetFhltarLb(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return GetFhltar(cit);
  } 

double CoilMap::GetAngle(double angle) const {  // acts as a way to verify if angle is in the map. If it isn't return value will be a sentinel
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the iterator for the specified angle
  // pass the iterator to the overload
  return GetAngle(cit);
  }

// Get the previous angle given an angle.
// Different than Lb -- if the passed in angle matches an angle, then the previous angle is returned (not the angle as the LB case does)
// if there is an angle in the map smaller than the passed in angle, the angle of the smaller entry is returned (like the LB case)
double CoilMap::GetPrevAngle(double angle) const {
  // return value uses a sentinel or angle to indicate success or error
  CoilMap::cm_cit cit= mapCoil_.lower_bound(angle);  // Get the position at or past the specified angle
  // If the test angle is smaller than anything in the map, the iterator will be at end(). There is no previous in this case. Return sentinel.
  // If the angle passed in was bigger than a row, then the iterator is at the row. Return the angle at that row.
  // If the angle passed in matches a row, backup up, and use the previous row.
  if (cit == mapCoil_.end()) // if at the end, the angle is smaller than anything in the list. No previous angle.
    return NO_FEATURE; // Return sentinel.
  else if (angle > cit->first)  // angle is bigger than an existing row, so the iterator points to the row.
    return cit->first; // return the pointed to row
  else if (cit != mapCoil_.begin()) // not at the beginning and we must have matche an angle to get here. Back up one and return that angle.
    return GetAngle(--cit); // decrement the iterator and get the previous position
  else  // catch all case. Angle matches the first entry, or something weird happened.
    return NO_FEATURE; // Return sentinel.
  }
 
double CoilMap::GetAngleLb(double angle) const { // previous angle
  // return value indicates success or error
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return GetAngle(cit);
} 

// Get Angle Upper Bound 
// Get angle after the passed in angle. (i.e. Upper bound, hence the Ub ending)
double CoilMap::GetAngleUb(double angle) const { // previous angle
  // return value indicates success or error
  return GetAngle(mapCoil_.upper_bound(angle));  // Get the position past the specified angle
} 

std::string CoilMap::GetFc(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the iterator for the specified angle
  // pass the iterator to the overload
  return GetFc(cit);
  }

// Get Feature code Lower Bound
std::string CoilMap::GetFcLb(double angle) const {
  // return value indicates success or error
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return GetFc(cit);
  }

long CoilMap::GetHexQuadNumber(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the iterator for the specified angle
  // pass the iterator to the overload
  return GetHexQuadNumber(cit);
  }
  
// Get HexQuad number Lower Bound
long CoilMap::GetHexQuadNumberLb(double angle) const {
  // return value indicates success or error
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return GetHexQuadNumber(cit);
}

long CoilMap::GetLayer(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the iterator for the specified angle
  // pass the iterator to the overload
  return GetLayer(cit);
  }

// Get Layer number Lower Bound
long CoilMap::GetLayerLb(double angle) const {
  // return value indicates success or error
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return GetLayer(cit);
  }

long CoilMap::GetTurn(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the iterator for the specified angle
  // pass the iterator to the overload
  return GetTurn(cit);
  }
  
// Get Turn Lower bound
long CoilMap::GetTurnLb(double angle) const {
  // return value indicates success or error
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return GetTurn(cit);
  }

double CoilMap::GetAzimuth(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the iterator for the specified angle
  // pass the iterator to the overload
  return GetAzimuth(cit);
  }
  
// Get Azimuth Lower Bound
double CoilMap::GetAzimuthLb(double angle) const {
  // return value indicates success or error
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return GetAzimuth(cit);
  }

double CoilMap::GetRadius(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the iterator for the specified angle
  // pass the iterator to the overload
  return GetRadius(cit);
  }
  
// Get Radius Lower bound
double CoilMap::GetRadiusLb(double angle) const {
  // return value indicates success or error
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return GetRadius(cit);
  }

// Get Joggle Lower bound
double CoilMap::GetJoggleLb(double angle) const {
  // return value indicates success or error
  // the set being looked in contains only joggle angles
  CoilMap::as_cit cit= setJoggleAngles_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != setJoggleAngles_.begin()) // if not at the beginning
    return *(--cit);  // decrement the iterator and retun the joggle angle
  else
    return RTN_NO_RESULTS;
  }

  // Given an angle, return the joggle length
// Jog angle length (the joggle window) is min at turn 1 and max at turn 14, otherwise it is zero
double CoilMap::GetJoggleLengthLb(double angle) const {
  // get the turn number, and set the joggle angle length accordingly
  long turn= GetTurnLb(angle);
  double jAngle= 0.0;
  if (1 == turn)  // turn 1
    jAngle= JOGGLE_LENGTH_MIN;
  else if (TURNS_PER_LAYER == turn) // turn 14
    jAngle= JOGGLE_LENGTH_MAX;
  return jAngle;
  }

// Get Joggle Angle of the joggle at or past the passed in angle (Upper bound, hence the Ub ending)
  // NOTE: Returns angle AT or past
double CoilMap::GetJoggleUb(double angle) const {
  // return value indicates success or error
  // the set being looked in contains only joggle angles
  CoilMap::as_cit cit= setJoggleAngles_.lower_bound(angle);  // iterator to matching or next angle from that specified
  if (cit != setJoggleAngles_.end())  // there was an angle (therefore a joggle) after the specified angle
    return *cit;  // return the looked up angle
  else  // no joggles after the specified angle
    return RTN_NO_RESULTS;
  }

bool CoilMap::isExisting(double angle) const { // does angle exist 
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the iterator for the specified angle
  // if iterator is not at the end, then the angle exists
  return (cit != mapCoil_.end());
  }


// Get data from the coil map using iterators
CoilMap::fhltar CoilMap::GetFhltar(CoilMap::cm_cit cit) const {
  CoilMap::fhltar features; // local version to be returned
  if (cit != mapCoil_.end()) {  // if not at the end
    // get the properties at the iterator
    features.get<0>()= cit->second.get<0>(); // feature code
    features.get<1>()= cit->second.get<1>(); // hex quand pancake number
    features.get<2>()= cit->second.get<2>(); // layer
    features.get<3>()= cit->second.get<3>(); // turn
    features.get<4>()= cit->second.get<4>(); // azimuth
    features.get<5>()= cit->second.get<5>(); // radius
    }
  else {                            // iterator is at the end
    // return sentinel values
    features.get<0>()= NO_FEATURE_STR; // feature code
    features.get<1>()= NO_FEATURE; // hex quand pancake number
    features.get<2>()= NO_FEATURE; // layer
    features.get<3>()= NO_FEATURE; // turn
    features.get<4>()= NO_FEATURE; // azimuth
    features.get<5>()= NO_FEATURE; // radius
    }
  return features;
  } 

double CoilMap::GetAngle(CoilMap::cm_cit cit) const { 
  // return value indicates success or error
  if (cit != mapCoil_.end()) {  // if not at the end
    // get the properties at the iterator
    return cit->first; // angle
    }
  else                          // iterator is at the end
    // return sentinel value
    return NO_FEATURE;
  } 

double CoilMap::GetAngle(CoilMap::cm_crit crit) const {
  // return value indicates success or error
  if (crit != mapCoil_.rend()) {  // if not at the end
                                // get the properties at the iterator
    return crit->first; // angle
  }
  else                          // iterator is at the end
                                // return sentinel value
    return NO_FEATURE;
}


std::string CoilMap::GetFc(CoilMap::cm_cit cit) const {
  // return value indicates success or error
  if (cit != mapCoil_.end()) {  // if not at the end
    // get the properties at the iterator
    return cit->second.get<0>(); // feature code
    }
  else                            // iterator is at the end
    return NO_FEATURE_STR;    // return sentinel value
  }

long CoilMap::GetHexQuadNumber(CoilMap::cm_cit cit) const {
  // return value indicates success or error
  if (cit != mapCoil_.end()) {  // if not at the end
    // get the properties at the iterator
    return cit->second.get<1>(); // hex/quad pancake number
    }
  else                            // iterator is at the end
    // return sentinel value
    return NO_FEATURE;
  }

long CoilMap::GetLayer(CoilMap::cm_cit cit) const {
  // return value indicates success or error
  if (cit != mapCoil_.end()) {  // if not at the end
    // get the properties at the iterator
    return cit->second.get<2>(); // layer
    }
  else                            // iterator is at the end
    // return sentinel value
    return NO_FEATURE;
  }

long CoilMap::GetTurn(CoilMap::cm_cit cit) const {
  // return value indicates success or error
  if (cit != mapCoil_.end()) {  // if not at the end
    // get the properties at the iterator
    return cit->second.get<3>(); // turn
    }
  else                            // iterator is at the end
    // return sentinel value
    return NO_FEATURE;
  }

double CoilMap::GetAzimuth(CoilMap::cm_cit cit) const {
  // return value indicates success or error
  if (cit != mapCoil_.end()) {  // if not at the end
    // get the properties at the iterator
    return cit->second.get<4>(); // azimuth
    }
  else                            // iterator is at the end
    // return sentinel value
    return NO_FEATURE;
  }

double CoilMap::GetRadius(CoilMap::cm_cit cit) const {
  // return value indicates success or error
  if (cit != mapCoil_.end()) {  // if not at the end
    // get the properties at the iterator
    return cit->second.get<5>(); // radius
    }
  else                            // iterator is at the end
    // return sentinel value
    return NO_FEATURE;
  }

// get angle given a turn from the layerAngle map of Odd Layer Turn 14 Transition Layers
double CoilMap::GetAngleOl14T(CoilMap::lam_cit cit) const {
  // return value indicates success or error
  if (cit != mapOl14T_.end()) {  // if not at the end
    // get the properties at the iterator
    return cit->second; // angle
    }
  else                            // iterator is at the end
    // return sentinel value
    return NO_FEATURE;
  }

bool CoilMap::isEvenLayer(CoilMap::cm_cit cit, bool &condition) const {
  // return value true or false to indicate ok or error
  // condition indicates odd (false) or even (true)
  if (cit != mapCoil_.end()) {  // if not at the end
    // layer MOD 2 = 0 then even
    condition = (0 == (cit->second.get<2>() % 2) ? true : false);
    return true;
    }
  else                            // iterator is at the end
    return false;
  }

bool CoilMap::isOddLayer(CoilMap::cm_cit cit, bool &condition) const {
  // return value true or false to indicate ok or error
  // condition indicates odd (true) or even (false)
  if (cit != mapCoil_.end()) {  // if not at the end
    // layer MOD 2 = 1 then odd
    condition = (1 == (cit->second.get<2>() % 2) ? true : false);
    return true;
    }
  else                            // iterator is at the end
    return false;
  }

bool CoilMap::isLastHqLayer(CoilMap::cm_cit cit, bool &condition) const {
  if (cit != mapCoil_.end()) {  // if not at the end
    condition = isLastHqLayer(GetLayer(cit));
    return true;
    }
  else  // iterator at the end
    return false;
  }

// Get the current and next angle.  Return them as a pair (current angle, next angle)
CoilMap::angle_pair CoilMap::GetCurrentNextAngle(CoilMap::cm_cit cit) const {
  CoilMap::angle_pair anglepr;
  anglepr.first= GetAngle(cit); // will get sentinel value if not found
  if (cit != mapCoil_.end())  // if not already at the end (end points past the last element)
    ++cit;  // increment the iterator to get to the next element
  if (cit != mapCoil_.end()) // if there is an element here
    anglepr.second= GetAngle(cit); // get the next element feature code
  else  // there are no elements after the one which was referenced
    anglepr.second= NO_FEATURE;    // use sentinel value
  return anglepr;
  }

// Get the current and next feature code. Return them as a pair (current FC, next FC)
CoilMap::fc_pair CoilMap::GetCurrentNextFc(CoilMap::cm_cit cit) const {
  CoilMap::fc_pair fcpr;
  fcpr.first= GetFc(cit); // will get sentinel value if not found
  if (cit != mapCoil_.end())  // if not already at the end (end points past the last element)
    ++cit;  // increment the iterator to get to the next element
  if (cit != mapCoil_.end()) // if there is an element here
    fcpr.second= GetFc(cit); // get the next element feature code
  else  // there are no elements after the one which was referenced
    fcpr.second= NO_FEATURE_STR;    // use sentinel value
  return fcpr;
  }

// Get the current layer and the next layer.  Return them as a pair (current layer, next layer)
CoilMap::layer_pair CoilMap::GetCurrentNextLayer(cm_cit cit) const {
  CoilMap::layer_pair layerpr;
  layerpr.first= GetLayer(cit); // will get sentinel value if not found
  if (cit != mapCoil_.end())  // if not already at the end (end points past the last element)
    ++cit;  // increment the iterator to get to the next element
  if (cit != mapCoil_.end()) // if there is an element here
    layerpr.second= GetLayer(cit); // get the next element feature code
  else  // there are no elements after the one which was referenced
    layerpr.second= NO_FEATURE;    // use sentinel value
  return layerpr;
  }
  

// Determine if the angle (column) is the last move (retreat) of the layer, 
// and return by reference the boolean condition, the joggle angle, and if the move is within the joggle window.
// This is the last move of the layer when:
  // A joggle starts and ends before the next column:
    // (Next Joggle Angle + Joggle Window) < (current angle + column increment)
    // isInJoggleWindow will be false
  // OR 
  // The current angle is within the joggle window of the previous joggle (the column is within a joggle):
    // (Previous Joggle Angle + Joggle window) >= current angle
    // isInJoggleWindow will be true
  // NOTE: The start joggle is the same as the start of the hex or layer (by dfn).
  // NOTE: The joggle window size is even/odd layer dependent, but this is handled by the GetJoggleLengthLb function.
    // Odd to even layer joggles are on turn 14, and even to odd layer joggles are on turn 1.
bool CoilMap::isLastMoveOfLayer(double angle, bool &condition, double &joggleAngle, bool &isInJoggleWindow) const {

  // find the next joggle angle and calculate how far away it is
  double nextJoggleAngle= GetJoggleUb(angle);
  double nextJoggleLength= GetJoggleLengthLb(nextJoggleAngle);
//  double degToNextJoggle= nextJoggleAngle - angle;
  // find the previous joggle angle, its length, and calculate how much past it we are
  double prevJoggleAngle= GetJoggleLb(angle);
  double prevJoggleLength= GetJoggleLengthLb(prevJoggleAngle);
//  double degToPrevJoggle= prevJoggleAngle - angle; // negative value when past a joggle

  if (RTN_NO_RESULTS != nextJoggleAngle && (nextJoggleAngle + nextJoggleLength) < (angle + COLUMN_INCREMENT)) {
    // A joggle starts and ends before the next column.
    // No need to check if the beginning is behind us, because we are using the *next* joggle 
    condition= true;
    joggleAngle= nextJoggleAngle;
    isInJoggleWindow= false;
    return true;
    }
  else if (RTN_NO_RESULTS != prevJoggleAngle && (prevJoggleAngle + prevJoggleLength) >= angle) {
    // the current angle is within the joggle window of the previous joggle.
    // The start of the joggle is before the angle, but the length of the joggle takes us up to or past the current angle.
    condition= true;
    joggleAngle= prevJoggleAngle;
    isInJoggleWindow = true;
    return true;
    }
  else {
    // not a new layer case
    condition= false;
    return true;
    }
  }  

// is Local Zero Lower bound
// Are we just after a local zero -- in other words, did we just start a new hqp?
bool CoilMap::isLocalZeroLb(double angle, bool &condition) const {
  // return value true or false to indicate ok or error
  // condition indicates local zero (true) or not (false)
  std::string fc= GetFcLb(angle); // get lower bound feature code
  if (FC_LOCAL == fc) { // fc is the Local Zero
    condition = true;
    return true;
    }
  else if (NO_FEATURE_STR != fc && FC_LOCAL != fc) { // fc is not an error and is not the local zero
    condition = false;
    return true;
    }
  else {  // error case
    condition = false;
    return false;
    }
  }

bool CoilMap::isEvenLayer(double angle, bool &condition) const {
  // return value true or false to indicate ok or error
  // condition indicates odd (false) or even (true)
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the position past the specified angle
  return isEvenLayer(cit, condition);
 }
  
// is Even Layer Lower bound
bool CoilMap::isEvenLayerLb(double angle, bool &condition) const {
  // return value true or false to indicate ok or error
  // condition indicates odd (false) or even (true)
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return isEvenLayer(cit, condition);
  }
  
bool CoilMap::isOddLayer(double angle, bool &condition) const {
  // return value true or false to indicate ok or error
  // condition indicates odd (true) or even (false)
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the position past the specified angle
  return isOddLayer(cit, condition);
  }

// is Odd Layer Lower bound
bool CoilMap::isOddLayerLb(double angle, bool &condition) const {
  // return value true or false to indicate ok or error
  // condition indicates odd (true) or even (false)
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return isOddLayer(cit, condition);
  }

// determine if a angle corresponds to a transition window Lower bound
  // if previous angle was a transition AND
  // foot angle - angle of previous transition <= the transition window (in the window)
  // then the passed in angle is one where a transition adjustment needs to happen
bool CoilMap::isInTransitionLb(double angle, bool &condition, double &degtoPrevTrans) const {
  long turn= GetTurnLb(angle);
  std::string fc= GetFcLb(angle);
  double anglePast = angle - GetAngleLb(angle);
  
  if (NO_FEATURE != turn && NO_FEATURE_STR != fc) {
    // looked up details are good so no errors. Proceed ...
    if  (FC_TRANSITION == fc &&  // previous row was a transition AND
        (TRANS_ARC_DEG >= anglePast)) { // within the transition window
      // in transition case
      condition= true;
      degtoPrevTrans= anglePast;
      return true;
      }
    else { // not in transition case
      condition= false;
      degtoPrevTrans= anglePast;
      return true;
      }
    }
  else {  // error case
    condition= false;
    degtoPrevTrans= 0;
    return false;
    }
  }

// determine if a angle corresponds to (is within) a joggle window Lower bound
bool CoilMap::isInJoggleLb(double angle, bool &condition) const {
  // Jog angle length (the joggle window) is min at turn 1 and max at turn 14, otherwise it is zero
  // get the turn number, and set the joggle angle length accordingly
  long turn= GetTurnLb(angle);
  std::string fc= GetFcLb(angle);
  double featureAngle= GetAngleLb(angle); // previous angle
  double jAngle= 0.0;
  
  if (NO_FEATURE != turn && NO_FEATURE_STR != fc && NO_FEATURE !=featureAngle) {
    // looked up details are good so no errors. Proceed ...
    if (1 == turn)
      jAngle= JOGGLE_LENGTH_MIN;
    else if (TURNS_PER_LAYER == turn)
      jAngle= JOGGLE_LENGTH_MAX;

    // if previous angle was a joggle, and foot angle - angle of previous joggle position <= adjusted joggle angle length (jAngle),
      //then the passed in angle is within the window
    if (FC_JOGGLE == fc && (angle - featureAngle <= jAngle)) {
      condition = true;
      return true;
      }
    else {
      condition = false;
      return true;
      }
    }
  else { // error case
      condition = false;
      return false;
    }
  }

// Determine if a turn corresponds to the last turn depending on the turn number and even/odd layer bool
// NOTE: Unlike other boolean tests, return value indicates test result (not status)
bool CoilMap::isLastTurnLb(long turn, bool isEvenLayer) const {
  // last turn if odd layer and turn is 14 OR
  // even layer and turn is 1
  if ((isEvenLayer && 1 == turn) ||               // even layer and turn 1 OR
     (!isEvenLayer && TURNS_PER_LAYER == turn)) { // odd layer and turn 14 OR
    // this is the last layer
    return true;
    }
  else  // not the last layer
    return false;
  }

// determine if a angle corresponds to the last turn of a layer
bool CoilMap::isLastTurnLb(double angle, bool &condition) const {
  // last turn if odd layer and angle is on the last turn (14) OR
  // even layer and the angle is on turn 1

  long turn= GetTurnLb(angle);
  bool isOdd;
  bool stat= isOddLayerLb(angle, isOdd);
  
  if (stat && ((isOdd && TURNS_PER_LAYER == turn) || // no lookup error AND ((odd layer, and turn 14) OR
               (!isOdd && 1 == turn))) {                                  // (even layer and turn 1))
    // no errors and this is the last layer
    condition = true;
    return true;
      }
  else if (stat && !((isOdd && TURNS_PER_LAYER == turn) || // no lookup error AND NOT ((odd layer, and turn 14) OR
                     (!isOdd && 1 == turn))) {                                        // (even layer and turn 1))
    // no erros and not the last layer
    condition = false;
    return true;
      }
  else { // lookup error
    condition = false;
    return false;
    }
  }
  
// determine if the angle or layer corresponds to the last hex/quad layer
  // Last layer of a hex/quad is when layer
  // is 6, 12, 18, 22, 28, 34, or 40
  // Mockup is 6, 10, and 16
  // return value true or false to indicate ok or error (except in long override)
  // condition indicates last layer (true) or not the last layer (false)
// NOTE: Unlike other boolean tests, return value indicates test result (not status)
// Mockup coil
/*
bool CoilMap::isLastHqLayer(long layer) const {
  if (6 == layer || 10 == layer || 16 == layer) {  // Last layer
    return true;
  }
  else  // not the last layer
    return false;
}
*/
// Normal coil
bool CoilMap::isLastHqLayer(long layer) const {
  if (6 == layer || 12 == layer || 18 == layer || 22 == layer ||
    28 == layer || 34 == layer || 40 <= layer) {  // Last layer
    return true;
  }
  else  // not the last layer
    return false;
}

bool CoilMap::isLastHqLayer(double angle, bool &condition) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.end()) {  // if not at the end, angle was found
    condition = isLastHqLayer(GetLayer(cit));
    return true;
    }
  else  // iterator at the end, angle not found
    return false;
  }

// determine if a angle corresponds to the last hex/quad layer Lower bound
bool CoilMap::isLastHqLayerLb(double angle, bool &condition) const {
  // return value true or false to indicate ok or error
  // condition indicates LastHqLayer (true) or Not the LastHqLayer (false)
  // Last layer of a hex/quad is when layer
  // is 6, 12, 18, 22, 28, 34, or 40
  // TODO: Deal with errors?  3-state bool?
  CoilMap::cm_cit cit= mapCoil_.upper_bound(angle);  // Get the position past the specified angle
  if (cit != mapCoil_.begin())  // if not at the beginning
    --cit;                            // decrement the iterator to get the previous position
  // pass the iterator to the overload
  return isLastHqLayer(cit, condition);
}
  
// looks for a value in the list of measurement and compression layers and returns true if the value was found
bool CoilMap::isInLaMeCo(long layer) const {
  lc_cit cit;  // make a iterator for the layer container
 cit = laMeCo_.find(layer); // look for the value
 if (cit != laMeCo_.end())  // found the value
   return true;
 else // value not found
   return false;
 }

// Given an angle, get the current Lb angle and the next angle.
// Return them as a pair (current Lb angle, next angle)
CoilMap::angle_pair CoilMap::GetCurrentNextAngle(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get element with the specified angle
  return GetCurrentNextAngle(cit);
  }
  
CoilMap::angle_pair CoilMap::GetCurrentNextAngleLb(double angle) const {
  return std::make_pair(GetAngleLb(angle), GetAngleUb(angle));
  }

// Given an angle, get the current Lb feature code and the next feature code. Return 
// them as a pair (current Lb fc, next fc)
// get the current and next feature code.  Return them as a pair (current FC, next FC)
CoilMap::fc_pair CoilMap::GetCurrentNextFc(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get element with the specified angle
  return GetCurrentNextFc(cit);
  }
CoilMap::fc_pair CoilMap::GetCurrentNextFcLb(double angle) const {
  double angLb = GetAngleLb(angle);
  if (NO_FEATURE != angLb) {
    // there is a lower bound angle. Use it to to call the member function
    return GetCurrentNextFc(angLb);
    }
  else // no Lb angle. Return sentinel values
    return std::make_pair(NO_FEATURE_STR, NO_FEATURE_STR);
  }

// Given an angle, get the current Lb layer and the next layer.
// Return them as a pair (current Lb layer, next layer)
CoilMap::layer_pair CoilMap::GetCurrentNextLayer(double angle) const {
  CoilMap::cm_cit cit= mapCoil_.find(angle);  // Get element with the specified angle
  return GetCurrentNextLayer(cit);
  }
CoilMap::layer_pair CoilMap::GetCurrentNextLayerLb(double angle) const {
  double angLb = GetAngleLb(angle);
  if (NO_FEATURE != angLb) {
    // there is a lower bound angle. Use it to to call the member function
    return GetCurrentNextLayer(angLb);
    }
  else // no Lb angle. Return sentinel values
    return std::make_pair(NO_FEATURE, NO_FEATURE);
  }
  
// get angle given a turn from the layerAngle map of Odd Layer Turn 14 Transition Layers
double CoilMap::GetAngleOl14T(long turn) const {
  CoilMap::lam_cit cit= mapOl14T_.find(turn);  // Get the iterator for the specified turn
  // pass the iterator to the overload
  return GetAngleOl14T(cit);
  }

// private helper functions

// database functions
long CoilMap::DbConnect() {
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
  }

long CoilMap::DbDisconnect() {
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
  }

// executes the specified stored procedure, which is expected to return results (SELECT...) 
long CoilMap::QueryDb(const std::string &sprocName) {
  // return value indicates success or error

  // query the db using teh specified stored procedure, 
  // presumably a SELECT command to fetch data

  // variable to hold return value
  long rtnValue= 0;

  // Set the command text of the command object
  dbCommand_.setCommandText(sprocName.c_str(), SA_CmdStoredProc);

  try {
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
  }

long CoilMap::MapFeature() {
  // return value indicates success or error

  // put the values fetched by the QueryDb() into the coil map.
  // QueryDb() should return one or more rows.  Map every row returned.

  // variable to hold return value
  long rtnValue= 0;
  try {
    if (dbCommand_.isResultSet() ) {
      // if there is a result set
      // create local tuple and angle variables to hold properties
      double angle;
      angle_properties tpAp; // tuple of angle properties
      while(dbCommand_.FetchNext() ) {
        // get angle, feature code, hqp number, layer, turn, azimuth, radius
        angle= dbCommand_.Field(CM_ANGLE_PARAM.c_str()).asDouble();                // angle
        tpAp.get<0>()= dbCommand_.Field(CM_FEATURECODE_PARAM.c_str()).asString();  // feature code
        tpAp.get<1>()= dbCommand_.Field(CM_HQP_PARAM.c_str()).asLong();            // hex/quad pancake number
        tpAp.get<2>()= dbCommand_.Field(CM_LAYER_PARAM.c_str()).asLong();          // layer
        tpAp.get<3>()= dbCommand_.Field(CM_TURN_PARAM.c_str()).asLong();           // turn
        tpAp.get<4>()= dbCommand_.Field(CM_AZIMUTH_PARAM.c_str()).asDouble();      // azimuth
        tpAp.get<5>()= dbCommand_.Field(CM_RADIUS_PARAM.c_str()).asDouble();       // radius
        // add the angle and associated properties to the map
        mapCoil_[angle]=tpAp;
        }
      // set return value for all okay
      rtnValue= RTN_NO_ERROR;
      }
    else {
      // set return value to error for no results present
      rtnValue= RTN_NO_RESULTS;
      }
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
  }

long CoilMap::MapCmOlT14FcT() {
  // maps Coil map rows of Turn 14 odd layers with feature codes of "T". Used for odd layer consolidation.
  // return value indicates success or error

  // QueryDb() is called with the correct stored procedure name prior to this function being called.
  // The query returns a set of layer numbers and corresponding angles for each odd layer turn 14 wiht a feature code of Transition(T)
  // put the values fetched by the QueryDb() into a layerAngle map
  // QueryDb() should return one or more rows.  Map every row returned.

  // variable to hold return value
  long rtnValue= 0;
  try {
    if (dbCommand_.isResultSet() ) {
      // if there is a result set
      // create local variables to hold layer and angle
      long layer;
      double angle;
      while(dbCommand_.FetchNext() ) {
        // get layer and angle
        layer= dbCommand_.Field(CM_LAYER_PARAM.c_str()).asLong();    // layer
        angle= dbCommand_.Field(CM_ANGLE_PARAM.c_str()).asDouble();  // angle
        // add the layer and associated angle to the map
        mapOl14T_[layer]= angle;
        }
      // set return value for all okay
      rtnValue= RTN_NO_ERROR;
      }
    else {
      // set return value to error for no results present
      rtnValue= RTN_NO_RESULTS;
      }
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
  }

long CoilMap::PopulateJoggleAngleSet() {
  // Populates a set of joggle angles sorted ascendingly
  // return value indicates success or error

  // QueryDb() is called with the correct stored procedure name prior to this function being called.
  // The query returns a sorted list of angles which correspond to joggles
  // Put the values fetched by the QueryDb() into a set
  // QueryDb() should return one or more rows.  Map every row returned.

  // variable to hold return value
  long rtnValue= 0;
  try {
    if (dbCommand_.isResultSet() ) {
      // if there is a result set
      // create local variables to the angle
      while(dbCommand_.FetchNext() ) {
        // get angle
        // add the layer and associated angle to the map
        setJoggleAngles_.insert(dbCommand_.Field(CM_ANGLE_PARAM.c_str()).asDouble());  // insert the retreived angle into the set
        }
      // set return value for all okay
      rtnValue= RTN_NO_ERROR;
      }
    else {
      // set return value to error for no results present
      rtnValue= RTN_NO_RESULTS;
      }
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
  }
 
} // namsepace gaScsData