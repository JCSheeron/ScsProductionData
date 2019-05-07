/* *******************************************************************
 * COPYRIGHT -- General Atomics
 ********************************************************************
 * Library: 
 * File: AxesPositions.cpp
 * Author: J. Sheeron (x2315)
 * Created: March 05, 2014
 *******************************************************************
 * Function: The AxisPositions class is responsible for calculating and storing
 * the Scs and Cls position tables.  Each row in these tables corresponds to a
 * foot move.  The sequence of operations is as follows:
 * 1) Connect to the DB
 * 2) Retrieve the coil map using a sql stored procedure select command.
 * 3) Disconect from the DB
 * 4) Populate a member CoilMap object (coilMap_)
 * 5) Iterate through the coil map, start at 30 degrees (Col A),
 *    go every 60 degrees (next column) until the end of the coil (max angle ~200,000 degrees).
 *     * At every angle, calculate the position for each foot.
 *     * Put the data into a member map scsAxisPositionMap_
 * 6) Connect to the DB
 * 7) Delete all rows from the Scs and Cls position tables, using a sql stored procedure
 * 8) For each row in the Scs position map, insert a row in the corresponding
 *    db table using a sql stored procedure
 * 9) Once the scs position table is populated in the db, use a sql stored
 *    procedure to build the Cls position table from the Scs position table.
 * 10) Disconnect from the db
 *
 * NOTE 1: There is a sql view defined to show the position tables.  The view is created for use in
 * all downstream logic, being in the PLC or as part of the event table generation. This view
 * ensures that there are no NULL values displayed, provides a way to condition data, and brings
 * position table and coil map data in a seamless way. The hqp number, layer number, turn number,
 * and overall turn values, for example are not stored in the position table, but are shown in the
 * position table view using a table join mechanism, joining the position and coil map tables.
 * Once the scs and cls position tables are generated, it is intended that all other logic will use
 * the views for any related logic or data generation.
 *
 * NOTE 2:  The coil map based values for hqp, layer, and overall turn shown in the position table views
 * need to be adjusted as show in the following table.  This adjustment value is stored in the position
 * table, and is used by the view to display the adjusted (corrected) hqp, layer, and overall turn value.
 * The adjustements are necessary because of the non-regular way the joggles in the coil map (where these
 * values are incremented) end up being arranged in relation to the columns.  In cases, for example,
 * of a new layer is starting in the position table right before a joggle, the layer number reported by
 * the coil map has not incremented yet, and needs to be increased by 1 in the view.
 * |---------------------------------------------------------------------------------------------------------------------------------------|
 * | isNewHqp | isNewLayer | isInJoggle | hqp adj | layer adj | overallTurn adj | notes                                                    |
 * |----------|------------|------------|---------|-----------|-----------------|----------------------------------------------------------|
 * |    no    |     no     |     no     |    0    |     0     |       0         | nominal row                                              |
 * |    no    |     no     |     yes    |    -1   |     -1    |       -1        | hqp adj only ast turn & layer, layer adj only last turn  |
 * |    yes   |     no     |     no     |    +1   |     +1    |       +1        | new hqp row right before a joggle in coil map            |
 * |    yes   |     no     |     yes    |    0    |     0     |       0         | new hqp row right after a joggle in coil map             |
 * |    no    |     yes    |     no     |    0    |     +1    |       +1        | new layer row right before a joggle in the coil map      |
 * |    no    |     yes    |     yes    |    0    |     0     |       0         | new layer row right after a joggle in the coil map       |
 * |    yes   |     yes    |      x     |    x    |     x     |       x         | don't care case -- by dfn can't be new hqp and new layer |
 * |----------|------------|------------|---------|-----------|-----------------|----------------------------------------------------------|
 *
 * Libraries used:
 *******************************************************************/
 
 // Precompiled header
#include "pch.hpp"

// header file
#include "gaScsDataConstants.hpp"
#include "AxisPositions.hpp"

namespace gaScsData {

// ctors and dtor
  AxisPositions::AxisPositions() : 
      coilMap_(),
      serverText_(DB_SERVER_NAME + "@" + DB_DATABASE_NAME),
      errorText_("") {

    // initialize member variables
      // vector of axis indexes
      // NOTE: enum value should match vector position (index)
      vAxisIndexes_.reserve((COLUMN_COUNT * 2) + 1);  // make space for each axis plus unknown
      vAxisIndexes_.push_back(AXIDX_UNKNOWN);
      vAxisIndexes_.push_back(AXIDX_A_FT_IN);
      vAxisIndexes_.push_back(AXIDX_A_FT_OUT);
      vAxisIndexes_.push_back(AXIDX_B_FT_IN);
      vAxisIndexes_.push_back(AXIDX_B_FT_OUT);
      vAxisIndexes_.push_back(AXIDX_C_FT_IN);
      vAxisIndexes_.push_back(AXIDX_C_FT_OUT);
      vAxisIndexes_.push_back(AXIDX_D_FT_IN);
      vAxisIndexes_.push_back(AXIDX_D_FT_OUT);
      vAxisIndexes_.push_back(AXIDX_E_FT_IN);
      vAxisIndexes_.push_back(AXIDX_E_FT_OUT);
      vAxisIndexes_.push_back(AXIDX_F_FT_IN);
      vAxisIndexes_.push_back(AXIDX_F_FT_OUT);
      vAxisIndexes_.push_back(AXIDX_A_COL_IN);
      vAxisIndexes_.push_back(AXIDX_A_COL_OUT);
      vAxisIndexes_.push_back(AXIDX_B_COL_IN);
      vAxisIndexes_.push_back(AXIDX_B_COL_OUT);
      vAxisIndexes_.push_back(AXIDX_C_COL_IN);
      vAxisIndexes_.push_back(AXIDX_C_COL_OUT);
      vAxisIndexes_.push_back(AXIDX_D_COL_IN);
      vAxisIndexes_.push_back(AXIDX_D_COL_OUT);
      vAxisIndexes_.push_back(AXIDX_E_COL_IN);
      vAxisIndexes_.push_back(AXIDX_E_COL_OUT);
      vAxisIndexes_.push_back(AXIDX_F_COL_IN);
      vAxisIndexes_.push_back(AXIDX_F_COL_OUT);
      
      // initialize Scs position detail vectors
      scsPosDetail_.get<0>().assign(COLUMN_COUNT, INITIAL_NO_POSITION); // foot position vector. size to the number of columns and init
      scsPosDetail_.get<1>().assign(COLUMN_COUNT, INITIAL_NO_POSITION); // column position vector. size to the number of columns and init
      scsPosDetail_.get<2>().assign((COLUMN_COUNT * 2) + 1, false); // selected axis vector. make space for each axis plus 1 for the flag, and init to false.

    // initialize transition adjustment mark vector
    // one element for each column, init to false
    arrAdjMark_.assign(6, false);
      
    // initialize database connectivity parameters
    // use SQL server native client
    dbConnection_.setClient(SA_SQLServer_Client);
    // Set the connection to use the ODBC API
    // ODBC is default API, so this is only being specified for backward compatibility and documentation.
    dbConnection_.setOption( "UseAPI" ) = "ODBC";
    // Set the commnad object to use the connection
    dbCommand_.setConnection(&dbConnection_);

  }

  AxisPositions::~AxisPositions() { }

// public accessors

// public methods

  // Connects to the Db, retrieves the coil map and populates the member data structure
    // This is done via the coil map interface
  // return value indicates success or error
  long AxisPositions::GenerateCoilMap() {
    return coilMap_.PopulateCoilMap();
  }
    
  // 1) Iterate thru azimuth positions, and using the coil map, calculate the CLS move distances and SCS positions.
  // 2) Add them to the SCS position map. 
  // 3) Delete all the existing (old) rows from the CLS and SCS position tables.
    // This also resets the identity index so rows will start at id = 1
  // 4) Iterate thru the SCS map, and insert the positions in the SCS position map into the SCS position table in the DB
  // 5) Build the db CLS positon table from the SCS position table. 
  // Return value indicates success or error
  long AxisPositions::GeneratePositionTables() {
    long connectStatus= 0;
    long insertStatus = 0;
    
    // Make an entry in the Cls and Scs position maps for each foot/column pair (in/out) azimuth

    std::cout << "Calculating Axis Moves for SCS and CLS." << std::endl;
    CalculateAxisMoves();
    std::cout << "Done Calculating Axis Moves." << std::endl << std::endl;
    
    // connect to db
    connectStatus= DbConnect();

    // if connect status is okay
      // 3) Delete all the existing (old) rows from the CLS and SCS position tables.
      // 4) Iterate thru the CLS map, and insert the positions longo a CLS position table in the DB
      // 5) Iterate thru the SCS map, and insert the positions longo a SCS position table in the DB
    if (RTN_NO_ERROR == connectStatus) {
      // if no error (DB connect was sucessful)
      
      // delete previous records from CLS and SCS position tables
      DeleteAllPositions();

      std::cout << "Insert records into SCS position table." << std::endl;
      insertStatus = InsertIntoScsDb(); 
      std::cout << "Done inserting records into SCS position table." << std::endl << std::endl;

      // Cls table is built from data in the scs table. It must go second.
      std::cout << "Insert records into CLS position table." << std::endl;
      insertStatus = InsertIntoClsDb(); 
      std::cout << "Done inserting records into CLS position table." << std::endl << std::endl;
      }
      
    // is status is okay (connection was sucessful), disconnect from the db
    if (RTN_NO_ERROR == connectStatus)
      connectStatus= DbDisconnect();

    if (RTN_NO_ERROR == connectStatus && RTN_NO_ERROR == insertStatus)
      return RTN_NO_ERROR;
    else
      return RTN_ERROR;
    } // AxisPositions::GeneratePositionTables()

  // convert an axis index to a string
  std::string AxisPositions::IndexToString(long index) const {
    // convert the index to the enumeration and call the overload
    return IndexToString(static_cast<gaScsData::AxisIndexes>(index));
    } // std::string AxisPositions::IndexToString(long index)
    
  std::string AxisPositions::IndexToString(gaScsData::AxisIndexes index) const {
    if(AXIDX_A_FT_IN == index)
      return "A Foot Inner";
    else if (AXIDX_A_FT_OUT == index)
      return "A Foot Outer";
    else if (AXIDX_B_FT_IN == index)
      return "B Foot Inner";
    else if (AXIDX_B_FT_OUT == index)
      return "B Foot Outer";
    else if (AXIDX_C_FT_IN == index)
      return "C Foot Inner";
    else if (AXIDX_C_FT_OUT == index)
      return "C Foot Outer";
    else if (AXIDX_D_FT_IN == index)
      return "D Foot Inner";
    else if (AXIDX_D_FT_OUT == index)
      return "D Foot Outer";
    else if (AXIDX_E_FT_IN == index)
      return "E Foot Inner";
    else if (AXIDX_E_FT_OUT == index)
      return "E Foot Outer";
    else if (AXIDX_F_FT_IN == index)
      return "F Foot Inner";
    else if (AXIDX_F_FT_OUT == index)
      return "F Foot Outer";
    else if(AXIDX_A_COL_IN == index)
      return "A Column Inner";
    else if (AXIDX_A_COL_OUT == index)
      return "A Column Outer";
    else if (AXIDX_B_COL_IN == index)
      return "B Column Inner";
    else if (AXIDX_B_COL_OUT == index)
      return "B Column Outer";
    else if (AXIDX_C_COL_IN == index)
      return "C Column Inner";
    else if (AXIDX_C_COL_OUT == index)
      return "C Column Outer";
    else if (AXIDX_D_COL_IN == index)
      return "D Column Inner";
    else if (AXIDX_D_COL_OUT == index)
      return "D Column Outer";
    else if (AXIDX_E_COL_IN == index)
      return "E Column Inner";
    else if (AXIDX_E_COL_OUT == index)
      return "E Column Outer";
    else if (AXIDX_F_COL_IN == index)
      return "F Column Inner";
    else if (AXIDX_F_COL_OUT == index)
      return "F Column Outer";
    else 
      return "Unknown Index!";
    } // std::string AxisPositions::IndexToString(gaScsData::AxisIndexes index)


// private helper functions
  // Calculate the transition adjustment (return value)
  // This is dependent on where in the transition window the passed in angle falls,
  // and on odd or even layer. The transition past the start (transAngleChange_) could have been passed
  // in as a parameter, but it was not to make this function more stand along.
  double AxisPositions::CalculateTransitionAdjustment(double angle) {
      
    // look up even or odd layer, and use appropriate algorithm
    transStat_= coilMap_.isOddLayerLb(angle, transResult_); // odd layer
    if (transStat_ && transResult_) {  // sucessful lookup and odd layer
      // Odd layers transition from an arc region to a straight region and
      // from larger radius (r2) to smaller radius (r1).
      
      // Calculate large (r2) and small (r1) radius and RArc
      transR2_= coilMap_.GetRadiusLb(angle); // nominal radius at start of transition
      transR1_= transR2_ - TURN_INDEX_NOMINAL; // nominal radius after the transition (next turn)
      transRArc_= transR2_ - TRANS_Ro; 
      // Calculate the angle beyond the start of the transition (RIA - beginning of transition angle).
        // The angles in the coil map for transitions are by definition the angles of the
        // start of the transitions, therefore, when in a transition region, the angle of the start
        // of the transition can be looked up from the coil map table
      transAngleFromStart_= angle - coilMap_.GetAngleLb(angle); 
      
      // Calculate angle where regions changes from arc to straight
      transAngleChange_= TRANS_ARC_DEG - (atan(TRANS_STRAIGHT_LENGTH / transR1_) * RADIANS_TO_DEG); 
      
      // Calculate the radius within the transition region. The formula used depends on if the 
        // angle falls in the straight or arc region of the transition.
      if (0 <= transAngleFromStart_ && transAngleFromStart_ <= transAngleChange_) { // arc region
        // angle is between 0 and the change angle (inclusive)
        transR_= (TRANS_Ro * cos(transAngleFromStart_ * DEG_TO_RADIANS)) +
                 sqrt(pow(transRArc_, 2) - (pow(TRANS_Ro, 2) * pow(sin(transAngleFromStart_ * DEG_TO_RADIANS), 2)));

        // return the size of the adjustment
        return transR2_ - transR_; // radius is getting smaller, adjustment should be positive
        }
      else if (transAngleChange_ < transAngleFromStart_ && transAngleFromStart_ <= TRANS_ARC_DEG ){ // straight region
        // angle greater than change angle and less than or equal to the total transition angle
        transR_= transR1_ / (cos((TRANS_ARC_DEG - transAngleFromStart_) * DEG_TO_RADIANS));

        // return the size of the adjustment
        return transR2_ - transR_; // radius is getting smaller, adjustment should be positive
        }
      else { // Error! Neither region
        // TODO: confirm the error behavior
        return 0; // return no adjustment on error
        }
      }
    else if (transStat_ && !transResult_) { // sucessful lookeup and even layer
      // Even layers transition from an straight region to an arc region and
      // from smaller radius (r1) to larger radius (r1).
      
      // Calculate small (r1) and large (r2) radius and RArc
      transR1_= coilMap_.GetRadiusLb(angle); // nominal radius at start of transition
      transR2_= transR1_ + TURN_INDEX_NOMINAL; // nominal radius after the transition (next turn)
      transRArc_= transR2_ - TRANS_Ro; 
      // Calculate the angle beyond the start of the transition (RIA - beginning of transition angle).
        // The angles in the coil map for transitions are by definition the angles of the
        // start of the transitions, therefore, when in a transition region, the angle of the start
        // of the transition can be looked up from the coil map table
      transAngleFromStart_= angle - coilMap_.GetAngleLb(angle); 
      
      // Calculate angle where regions changes from straight to arc
      transAngleChange_= atan(TRANS_STRAIGHT_LENGTH / transR1_) * RADIANS_TO_DEG; 
      
      // Calculate the radius within the transition region. The formula used depends on if the 
        // angle falls in the straight or arc region of the transition.
      if (0 <= transAngleFromStart_ && transAngleFromStart_ <= transAngleChange_) { // straight region
        // angle is between 0 and the change angle (inclusive)
        transR_=  transR1_ / cos(transAngleFromStart_ * DEG_TO_RADIANS);
 
        // return the sizs of the adjustment
        return transR_ - transR1_; // radius is getting larger, adjustment should be positive
        }
      else if (transAngleChange_ < transAngleFromStart_ && transAngleFromStart_ <= TRANS_ARC_DEG ){ // arc region
        // angle greater than change angle and less than or equal to the total transition angle
        transR_= (TRANS_Ro * cos((transAngleFromStart_ - TRANS_ARC_DEG) * DEG_TO_RADIANS)) +
                 sqrt(pow(transRArc_, 2) - (pow(TRANS_Ro, 2) * pow(sin((transAngleFromStart_ - TRANS_ARC_DEG) * DEG_TO_RADIANS), 2)));

        // return the size of the adjustment
        return transR_ - transR1_; // radius is getting larger, adjustment should be positive
        }
      else { // Error! Neither region
        // TODO: confirm the error behavior
        return 0; // return no adjustment on error
        }
      }
    else { // something wrong with the lookup
        // TODO: confirm the error behavior
        return 0; // return no adjustment on error
      } // odd or even layer
    
    } // AxisPositions::CalculateTransitionAdjustment(double angle, double &degtoPrevTrans)

  // Given an angle, the corresponding foot pair is marked as having been adjusted 
  // for a transition (via the member boolean array)
  // return value indicates true on success or false on bad angle
  bool AxisPositions::SetTransAdjust(double angle) {
    // The passed in angle is used to calculate a corresponding column (MOD 360).  
    // get azimuth and then use the azimuth to determine which column
    long azi = static_cast<long>(fmod(angle, 360.0)); // the resulting value should be an integer, but truncate any decimal part just in case
    long column = -1;  // sentinel value for error checking. Should end up between 0 and 5.  
    if (A_COLUMN_AZIMUTH == azi)
        column = 0;
    else if (B_COLUMN_AZIMUTH == azi)
        column = 1;
    else if (C_COLUMN_AZIMUTH == azi)
        column = 2;
    else if (D_COLUMN_AZIMUTH == azi)
        column = 3;
    else if (E_COLUMN_AZIMUTH == azi)
        column = 4;
    else if (F_COLUMN_AZIMUTH == azi)
        column = 5;
    else 
      return false;  // return error 

    // mark the column and return if valid
    if (0 <= column && 5 >= column) {
      arrAdjMark_[column]= true;
      return true;
      }
    else // should never get here
      return false;
    } // AxisPositions::SetTransAdjust(double angle)
    
  // Clear all trans adjust marks
  void AxisPositions::ClearAllTransAdjust() {
    for (unsigned long column = 0; column < arrAdjMark_.size(); ++ column)
      arrAdjMark_[column]= false;
    } // AxisPositions::ClearAllTransAdjust()

  // Is the trans adjust mark set
  bool AxisPositions::isTransAdjustSet(double angle) const {
    // The passed in angle is used to calculate a corresponding column (MOD 360).  
    // get azimuth and then use the azimuth to determine which column
    long azi = static_cast<long>(fmod(angle, 360.0)); // the resulting value should be an integer, but truncate any decimal part just in case
    long column = -1;  // sentinel value for error checking. Should end up between 0 and 5.  
    if (A_COLUMN_AZIMUTH == azi)
        column = 0;
    else if (B_COLUMN_AZIMUTH == azi)
        column = 1;
    else if (C_COLUMN_AZIMUTH == azi)
        column = 2;
    else if (D_COLUMN_AZIMUTH == azi)
        column = 3;
    else if (E_COLUMN_AZIMUTH == azi)
        column = 4;
    else if (F_COLUMN_AZIMUTH == azi)
        column = 5;
    else 
      return false;  // error case

    // mark the column and return if valid
    if (0 <= column && 5 >= column) {
      return arrAdjMark_[column];
      }
    else  // should never get here
      return false;
    } // AxisPositions::isTransAdjustSet(double angle) 
    
    
  // Look at the degrees before and after a joggle the passed in angle is and
  // return what type of adjustment is needed due to the joggle
  // Pass back by reference the distance to the joggle (in degrees) and the joggle adjustment
  // Joggles affect moves are roughtly at (taking the joggle length into account):
    // Region 1: (360 to 360 - Joggle Length) degrees before a joggle,
    // Region 2: (0 to 0 - Joggle Length) at or degrees past a joggle,
    // or Region 3: (360 to (360 - Joggle Length) degrees past a joggle.
    // the amounts are set as defined constants.
  // Each of the 3 above regions needs a different adjustment.
  // Look ahead at the next joggle. If it is within region 1, then the retracting feed should retract a nominal + joggle adjustment distance
    // and the advancing foot should advanced as normal.  If the previous joggle is more than 360 degrees away, 
    // then look at the previous joggle to check for region 2 or 3.
  // If the previous joggle is in region 2 then the retreating foot should retreat all the way and the advancing foot should NOP.
  // **** Change:  If the previous joggle is in region 3 then the retreating foot should be nominal and the advancing feet should be nominal (a normal case)

  // **** Region 3 Used to be this:
  // If the previous joggle is in region 3 then the retreating foot should NOP, and the advancing foot should advance to the last turn
    // Note that the foot roles have swapped between region 2 and region 3.
  AxisPositions::joggleAdjustmentType AxisPositions::CalculateJoggleAdjustmentType(double angle, double &degToNextJoggle, double &degToPrevJoggle, double &jAdj) const {
    // find the next joggle angle, its length, and calculate how far away it is
    double nextJoggleAngle= coilMap_.GetJoggleUb(angle);
    degToNextJoggle= nextJoggleAngle - angle;
    double nextJoggleLength= coilMap_.GetJoggleLengthLb(nextJoggleAngle);
    // find the previous joggle angle, its length, and calculate how much past it we are
    double prevJoggleAngle= coilMap_.GetJoggleLb(angle);
    degToPrevJoggle= prevJoggleAngle - angle; // negative value when past a joggle
    double prevJoggleLength= coilMap_.GetJoggleLengthLb(prevJoggleAngle);
    
    if ((JOGGLE_RETRACT_ADJ_THRESHOLD < degToNextJoggle) && // next is too far ahead for region 1
        (JOGGLE_ADV_TO_FIRST_THRESHOLD - prevJoggleLength) > degToPrevJoggle) { // previous is too far past for retion 2 or 3
      // not close enouth to the next joggle for region 1, and too far past last joggle for region 2 or 3
      // no joggle adjustment and do nominal moves for both advancing and retracting feet
      jAdj= 0;
      return JA_RET_NOM_ADV_NOM;
      }
    else if (JOGGLE_RETRACT_ADJ_THRESHOLD >= degToNextJoggle && 
             (JOGGLE_RETRACT_ADJ_THRESHOLD - nextJoggleLength) <= degToNextJoggle) { // within region 1
      // cannot also be in region 2 or 3 because joggles are not very close together
      // joggle adjustment should be half a normal index
      jAdj= TURN_INDEX_NOMINAL / 2.0;
      // adjustment type is to adjust the retreating foot by the adjustment value and to do a nominal index of the advancing foot
      return JA_RET_ADJ_ADV_NOM;
      }
    else if (JOGGLE_FULL_RETRACT_THRESHOLD >= degToPrevJoggle && 
             (JOGGLE_FULL_RETRACT_THRESHOLD - prevJoggleLength) <= degToPrevJoggle) { // within region 2
      // cannot also be in region 1 or 3 because joggles are not very close together
      // joggle adjustment not used for this type of joggle adjustment
      jAdj= 0;
      // adjustment type is to fully retract the retreating foot, and do nothing (no move) on the advancing foot
      return JA_RET_FULL_ADV_NOP;
      }
    else if (JOGGLE_ADV_TO_FIRST_THRESHOLD >= degToPrevJoggle && 
             (JOGGLE_ADV_TO_FIRST_THRESHOLD - prevJoggleLength) <= degToPrevJoggle) { // within region 3
      // cannot also be in region 1 or 2 because joggles are not very close together
      // joggle adjustment not used for this type of joggle adjustment
      jAdj= 0;
      // adjustment type is now a nomimal move for both feet
      // return JA_RET_NOP_ADV_NOM; // was this
      return JA_RET_NOM_ADV_NOM; // now this
    }
    else {  // not in a region -- not a joggle case
      // no joggle adjustment and do nominal moves for both advancing and retracting feet
      jAdj= 0;
      return JA_RET_NOM_ADV_NOM;
      }
    
    } // AxisPositions::joggleAdjustmentType AxisPositions::CalculateJoggleAdjustmentType(double angle, double &degToNextJoggle, double @degToPrevJoggle, double &jAdj)

 
    // Calculate the Ria Angle where the new layer positions row should go
    // and determine if the angle marks the beginning of an odd or even layer.
    // The joggle angle, which is assumed to be one near the current angle,
    // is used to determine if the new layer is even or odd.
    // Return an (angle,isEven) pair.
  AxisPositions::angleIsEven_pair AxisPositions::CalculateNewLayerRiaAngle(double coilAngle, double joggleAngle) const {
    // Calculate the ria angle where the new layer positions should go.
    double riaAngle= coilAngle - ADV_FOOT_RIA_OFFSET_ANGLE + NEW_LAYER_OFFSET;
    // see if the new layer is even or odd
    bool stat, isEven;
    stat = coilMap_.isEvenLayerLb(joggleAngle, isEven); 
    // return the angle where the row goes and the isEven value
    return std::make_pair(riaAngle, isEven);
    }
    

      // Populates the SCS axis move position detail structure passed in as a reference.
      // Only deals with foot positions.
      // Look at the passed in mode parameter to determine which SQL procedure needs to be called. Populate the position detail accordingly.
      // If insert mode is IM_ABS_ALL or IM_REL_ALL, then move the feet inside axes to pos 1 and outside axes to pos 2.
        // For Insert All mode, the columns are loaded with a sential value, as their desired position is not known until run-time.
        // The foot role parameter is not used (has no effect) in the IM_ABS_ALL or IM_REL_ALL insert modes.
        // If in IM_ABS_ALL, and the  isInJoggle flag is set, then the column corresponding to the coil angle is adjusted by an index to account for the joggle.
        // If the layer is even, then the outer foot is retracted from the desired position by a nominal index.
        // If the layer is odd, then the inner foot is retracted from the desired position by a nominal index.
      // If insert mode is IM_REL_SEL, IM_ABS_SEL, or IM_ABS_UPDATE_SEL, then move the selected axes to pos 1, and pos 2 is unused.
        // Used the passed in angle, the odd/even layer, and the role to determine the prober axis to apply the distance/position value to
        // It is expected that the passed in angle will fall on a column azimuth, and not in between columns.
      //
      // return value indicates success or error
  long AxisPositions::PopScsPosDetail(double coilAngle, bool isEven, footRole role, insertMode mode,
                                      double fDistPos1, double fDistPos2, const std::string &trace, 
                                      bool isInTransition, bool isInJoggle, bool isNewHqp, bool isNewLayer,
                                      bool isLastTurn, bool isLastLayer, long hqpAdj, long layerAdj,
                                      SPosDetail &posDetail) {
    // The passed in coil angle is used to calculate a corresponding column (MOD 360).  
    // get azimuth and then use the azimuth to determine which column.
    long azi = static_cast<long>(fmod(coilAngle, 360.0)); // the resulting value should be an integer, but truncate any decimal part just in case
    // abs of azi is < 360, but if the azimuth is negative, add 360 to make it positive
    azi = (azi < 0) ? (azi + 360) : azi;
    long column = -1;  // sentinel value for error checking. Should end up between 0 and 5.  
    if (A_COLUMN_AZIMUTH == azi)
      column = 0;
    else if (B_COLUMN_AZIMUTH == azi)
      column = 1;
    else if (C_COLUMN_AZIMUTH == azi)
      column = 2;
    else if (D_COLUMN_AZIMUTH == azi)
      column = 3;
    else if (E_COLUMN_AZIMUTH == azi)
      column = 4;
    else if (F_COLUMN_AZIMUTH == azi)
      column = 5;
    else
      return RTN_ERROR;  // return error sentinel value
          
    // look at the insert mode to know absolute vs relative and whether to insert a value for every axis or just a selected axis
    if (IM_ABS_ALL == mode || IM_REL_ALL == mode) {  // either of the insert all modes
      // in the non joggle case, absolute and relative moves are similar, except fot the isAbsolute flag
      // Absolute moves are used at the start of layers and hqps
      // Nominally set all the inner feet to pos 1, and set all outer feet to pos 2
      // If the layer is odd, nominally outer feed should get retracted all the way, and inner feet should be extended all the way
      // If the layer is even, nominally inner feed should get retracted all the way, and outer feet should be extended all the way
      // The joggle case is handled as a special case:
        // If this is a joggle and not a last layer, then the advancing foot corresponding to the coil angle (azimuth) should be extended one turn (end position - an index),
          // and the retreating foot shold be retracted one turn (end position + an inedx)
      if (!isEven) {
        // new layer is odd
        if (!isInJoggle) {
          // not a joggle case. Use nominal positions.
          // on an odd layer here, so the inner foot are retreating
          // and the outer foot are advancing 
          // Set all inner feet to pos 1
          posDetail.get<0>()[0] = fDistPos1;   // Foot A In
          posDetail.get<0>()[2] = fDistPos1;   // Foot B In
          posDetail.get<0>()[4] = fDistPos1;   // Foot C In
          posDetail.get<0>()[6] = fDistPos1;   // Foot D In
          posDetail.get<0>()[8] = fDistPos1;   // Foot E In
          posDetail.get<0>()[10] = fDistPos1;   // Foot F In

          // Set all the outer feet to pos 2
          posDetail.get<0>()[1] = fDistPos2;   // Foot A Out
          posDetail.get<0>()[3] = fDistPos2;   // Foot B Out
          posDetail.get<0>()[5] = fDistPos2;   // Foot C Out
          posDetail.get<0>()[7] = fDistPos2;   // Foot D Out
          posDetail.get<0>()[9] = fDistPos2;   // Foot E Out
          posDetail.get<0>()[11] = fDistPos2;   // Foot F Out
         }
       else if (isInJoggle && !isLastLayer) {
        // joggle but not last layer case
        // on an odd layer here, so the inner foot are retreating, and the one corresponding to the angle should be retreated one turn from nominal (bigger value),
        // and the outer feet are advancing and the one corresponding to the angle should be advanced one turn from nominal (smaller value)
        // Set all inner feet to pos + an index
        posDetail.get<0>()[0] = fDistPos1 + ((IM_ABS_ALL == mode && 0 == column) * TURN_INDEX_NOMINAL);   // Foot A In
        posDetail.get<0>()[2] = fDistPos1 + ((IM_ABS_ALL == mode && 1 == column) * TURN_INDEX_NOMINAL);   // Foot B In
        posDetail.get<0>()[4] = fDistPos1 + ((IM_ABS_ALL == mode && 2 == column) * TURN_INDEX_NOMINAL);   // Foot C In
        posDetail.get<0>()[6] = fDistPos1 + ((IM_ABS_ALL == mode && 3 == column) * TURN_INDEX_NOMINAL);   // Foot D In
        posDetail.get<0>()[8] = fDistPos1 + ((IM_ABS_ALL == mode && 4 == column) * TURN_INDEX_NOMINAL);   // Foot E In
        posDetail.get<0>()[10] = fDistPos1 + ((IM_ABS_ALL == mode && 5 == column) * TURN_INDEX_NOMINAL);   // Foot F In

        // Set all the outer feet to pos less an index
        posDetail.get<0>()[1] = fDistPos2 - ((IM_ABS_ALL == mode && 0 == column) * TURN_INDEX_NOMINAL);   // Foot A Out
        posDetail.get<0>()[3] = fDistPos2 - ((IM_ABS_ALL == mode && 1 == column) * TURN_INDEX_NOMINAL);   // Foot B Out
        posDetail.get<0>()[5] = fDistPos2 - ((IM_ABS_ALL == mode && 2 == column) * TURN_INDEX_NOMINAL);   // Foot C Out
        posDetail.get<0>()[7] = fDistPos2 - ((IM_ABS_ALL == mode && 3 == column) * TURN_INDEX_NOMINAL);   // Foot D Out
        posDetail.get<0>()[9] = fDistPos2 - ((IM_ABS_ALL == mode && 4 == column) * TURN_INDEX_NOMINAL);   // Foot E Out
        posDetail.get<0>()[11] = fDistPos2 - ((IM_ABS_ALL == mode && 5 == column) * TURN_INDEX_NOMINAL);   // Foot F Out
        }        
        else if (isInJoggle && isLastLayer) {
        // joggle and last layer case
        // on an odd layer here, so the inner foot are retreating, and the one corresponding to the angle should be retreated one turn from nominal (bigger value),
        // and the outer foot corresponding to the angle are advancing and should be put to the nominal position
        // Set all inner feet to pos + an index
        posDetail.get<0>()[0] = fDistPos1 + ((IM_ABS_ALL == mode && 0 == column) * TURN_INDEX_NOMINAL);   // Foot A In
        posDetail.get<0>()[2] = fDistPos1 + ((IM_ABS_ALL == mode && 1 == column) * TURN_INDEX_NOMINAL);   // Foot B In
        posDetail.get<0>()[4] = fDistPos1 + ((IM_ABS_ALL == mode && 2 == column) * TURN_INDEX_NOMINAL);   // Foot C In
        posDetail.get<0>()[6] = fDistPos1 + ((IM_ABS_ALL == mode && 3 == column) * TURN_INDEX_NOMINAL);   // Foot D In
        posDetail.get<0>()[8] = fDistPos1 + ((IM_ABS_ALL == mode && 4 == column) * TURN_INDEX_NOMINAL);   // Foot E In
        posDetail.get<0>()[10] = fDistPos1 + ((IM_ABS_ALL == mode && 5 == column) * TURN_INDEX_NOMINAL);   // Foot F In

        // Set all the outer feet to pos 2
        posDetail.get<0>()[1] = fDistPos2;   // Foot A Out
        posDetail.get<0>()[3] = fDistPos2;   // Foot B Out
        posDetail.get<0>()[5] = fDistPos2;   // Foot C Out
        posDetail.get<0>()[7] = fDistPos2;   // Foot D Out
        posDetail.get<0>()[9] = fDistPos2;   // Foot E Out
        posDetail.get<0>()[11] = fDistPos2;   // Foot F Out
        }
      }
      else {
        // new layer is even
        if (!isInJoggle) {
          // not a joggle case. Use nominal positions.
          // on an even layer here, so the inner foot are advancing
          // and the outer foot are retreating 
          // Set all inner feet to pos 1
          posDetail.get<0>()[0] = fDistPos1;   // Foot A In
          posDetail.get<0>()[2] = fDistPos1;   // Foot B In
          posDetail.get<0>()[4] = fDistPos1;   // Foot C In
          posDetail.get<0>()[6] = fDistPos1;   // Foot D In
          posDetail.get<0>()[8] = fDistPos1;   // Foot E In
          posDetail.get<0>()[10] = fDistPos1;   // Foot F In

          // Set all the outer feet to pos 2
          posDetail.get<0>()[1] = fDistPos2;   // Foot A Out
          posDetail.get<0>()[3] = fDistPos2;   // Foot B Out
          posDetail.get<0>()[5] = fDistPos2;   // Foot C Out
          posDetail.get<0>()[7] = fDistPos2;   // Foot D Out
          posDetail.get<0>()[9] = fDistPos2;   // Foot E Out
          posDetail.get<0>()[11] = fDistPos2;   // Foot F Out
        }
        else if (isInJoggle && !isLastLayer) {
          // joggle but not last layer case
          // on an even layer here, so the outer feet are retreating, and the one corresponding to the angle should be retreated one turn from nominal (bigger value),
          // and the inner are advancing and the one corresponding to the angle should be advanced one turn from nominal (smaller value)
          // Set all inner feet to pos - an index
          posDetail.get<0>()[0] = fDistPos1 - ((IM_ABS_ALL == mode && 0 == column) * TURN_INDEX_NOMINAL);   // Foot A In
          posDetail.get<0>()[2] = fDistPos1 - ((IM_ABS_ALL == mode && 1 == column) * TURN_INDEX_NOMINAL);   // Foot B In
          posDetail.get<0>()[4] = fDistPos1 - ((IM_ABS_ALL == mode && 2 == column) * TURN_INDEX_NOMINAL);   // Foot C In
          posDetail.get<0>()[6] = fDistPos1 - ((IM_ABS_ALL == mode && 3 == column) * TURN_INDEX_NOMINAL);   // Foot D In
          posDetail.get<0>()[8] = fDistPos1 - ((IM_ABS_ALL == mode && 4 == column) * TURN_INDEX_NOMINAL);   // Foot E In
          posDetail.get<0>()[10] = fDistPos1 - ((IM_ABS_ALL == mode && 5 == column) * TURN_INDEX_NOMINAL);   // Foot F In

          // Set all the outer feet to pos + an index
          posDetail.get<0>()[1] = fDistPos2 + ((IM_ABS_ALL == mode && 0 == column) * TURN_INDEX_NOMINAL);   // Foot A Out
          posDetail.get<0>()[3] = fDistPos2 + ((IM_ABS_ALL == mode && 1 == column) * TURN_INDEX_NOMINAL);   // Foot B Out
          posDetail.get<0>()[5] = fDistPos2 + ((IM_ABS_ALL == mode && 2 == column) * TURN_INDEX_NOMINAL);   // Foot C Out
          posDetail.get<0>()[7] = fDistPos2 + ((IM_ABS_ALL == mode && 3 == column) * TURN_INDEX_NOMINAL);   // Foot D Out
          posDetail.get<0>()[9] = fDistPos2 + ((IM_ABS_ALL == mode && 4 == column) * TURN_INDEX_NOMINAL);   // Foot E Out
          posDetail.get<0>()[11] = fDistPos2 + ((IM_ABS_ALL == mode && 5 == column) * TURN_INDEX_NOMINAL);   // Foot F Out
        }
        else if (isInJoggle && isLastLayer) {
          // joggle and last layer case
          // on an odd layer here, so the inner foot are retreating, and the one corresponding to the angle should be retreated one turn from nominal (bigger value),
          // and the outer feet are advancing and the one corresponding to the angle should be put to the nominal position
          // Set all inner feet to pos + an index
          posDetail.get<0>()[0] = fDistPos1;   // Foot A In
          posDetail.get<0>()[2] = fDistPos1;   // Foot B In
          posDetail.get<0>()[4] = fDistPos1;   // Foot C In
          posDetail.get<0>()[6] = fDistPos1;   // Foot D In
          posDetail.get<0>()[8] = fDistPos1;   // Foot E In
          posDetail.get<0>()[10] = fDistPos1;   // Foot F In

          // Set all the outer feet to pos 2
          posDetail.get<0>()[1] = fDistPos2 + ((IM_ABS_ALL == mode && 0 == column) * TURN_INDEX_NOMINAL);    // Foot A Out
          posDetail.get<0>()[3] = fDistPos2 + ((IM_ABS_ALL == mode && 1 == column) * TURN_INDEX_NOMINAL);   // Foot B Out
          posDetail.get<0>()[5] = fDistPos2 + ((IM_ABS_ALL == mode && 2 == column) * TURN_INDEX_NOMINAL);   // Foot C Out
          posDetail.get<0>()[7] = fDistPos2 + ((IM_ABS_ALL == mode && 3 == column) * TURN_INDEX_NOMINAL);   // Foot D Out
          posDetail.get<0>()[9] = fDistPos2 + ((IM_ABS_ALL == mode && 4 == column) * TURN_INDEX_NOMINAL);   // Foot E Out
          posDetail.get<0>()[11] = fDistPos2 + ((IM_ABS_ALL == mode && 5 == column) * TURN_INDEX_NOMINAL);   // Foot F Out
        }
      }
      // set all the columns to the not calculated sentinel value
      posDetail.get<1>()[0]= POSITION_NOT_CALCULATED;   // Column A In
      posDetail.get<1>()[2]= POSITION_NOT_CALCULATED;   // Column B In
      posDetail.get<1>()[4]= POSITION_NOT_CALCULATED;   // Column C In
      posDetail.get<1>()[6]= POSITION_NOT_CALCULATED;   // Column D In
      posDetail.get<1>()[8]= POSITION_NOT_CALCULATED;   // Column E In
      posDetail.get<1>()[10]= POSITION_NOT_CALCULATED;   // Column F In
      
      posDetail.get<1>()[1]= POSITION_NOT_CALCULATED;   // Column A Out
      posDetail.get<1>()[3]= POSITION_NOT_CALCULATED;   // Column B Out
      posDetail.get<1>()[5]= POSITION_NOT_CALCULATED;   // Column C Out
      posDetail.get<1>()[7]= POSITION_NOT_CALCULATED;   // Column D Out
      posDetail.get<1>()[9]= POSITION_NOT_CALCULATED;   // Column E Out
      posDetail.get<1>()[11]= POSITION_NOT_CALCULATED;   // Column F Out
      
      // clear the SelectedAxes info (not used in this mode) : Selected Axis flag, distance, and index
      posDetail.get<2>()[0]= false; // selected axis flag
      posDetail.get<3>().get<0>()= 0; // distance
      posDetail.get<3>().get<1>()= AXIDX_UNKNOWN; // index
      posDetail.get<3>().get<2>()= false; // absolute adjust flag
      
      // Set the PosAttributes: trace, isAbsolute, isInTransition, isInJoggle, isNewHqp, isNewLayser
        // flags, coil angel and then return. 
      posDetail.get<4>().get<0>()= trace;  // logic trace string
      // set the absolute flag depending on the mode
      posDetail.get<4>().get<1>()= IM_ABS_ALL == mode;  // isAbsolute flag (mode is absolute)
      posDetail.get<4>().get<2>()= isInTransition;      // isInTransition flag
      posDetail.get<4>().get<3>()= isInJoggle;  // isInJoggle flag
      posDetail.get<4>().get<4>()= isNewHqp;    // isNewHqp flag
      posDetail.get<4>().get<5>()= isNewLayer;  // isNewLayer flag
      posDetail.get<4>().get<6>()= isLastTurn;   // isLastTurn flag
      posDetail.get<4>().get<7>()= isLastLayer;   // isLastLayer flag
      posDetail.get<4>().get<8>()= coilAngle;   // coil angle
      // set the hqp and layer adj
      posDetail.get<5>().get<0>() = hqpAdj; // hqp adjust
      posDetail.get<5>().get<1>() = layerAdj; // layer adjust

      return RTN_NO_ERROR;  // return ok

      }
    else if (IM_REL_SEL == mode || IM_ABS_SEL == mode || IM_ABS_UPDATE_SEL == mode ) {  // one of the insert selected modes
      // These modes populate the position detail the same, except for 
        // the position detail.absolute flag and the position detail.selected detail.absolute adjust flag
        // A different procedure is called (elsewhere) depending on the position detail.selected detail.absolute adjust flag

      // look at even vs odd layer and advancing/retreating role to determine where the position goes
      if ((isEven && FOOT_ROLE_ADVANCING == role) || (!isEven && FOOT_ROLE_RETREATING == role)) {
        // Even layer and advancing foot or odd layer and retreating foot
        // inner columns move
        // Set inner column selection bit
          // (column variable * 2) + 1 is the index to the inner column, +1 more is the index to the outer column
        if (0 <= column && 5 >= column) { // range check column
          // column value okay
          // set column inner foot position/distance and mark as selected. Clear the other axes.
          SetSelectedScsAxis((column * 2) + 1, fDistPos1, posDetail, mode);
          }
        else {  // bad column value
          UnselectAllScsAxes(posDetail);
          posDetail.get<4>().get<0>()= trace + "column value out of range. col: " + std::to_string(static_cast<long long>(column)); // append error message to trace
          posDetail.get<4>().get<1>()= false;  // isAbsolute flag. force to false so that a move to abs 0 cannot happen
          posDetail.get<4>().get<2>()= false;  // isInTransition flag
          posDetail.get<4>().get<3>()= false;  // isInJoggle flag
          posDetail.get<4>().get<4>()= false;  // isNewHqp flag
          posDetail.get<4>().get<5>()= false;  // isNewLayer flag
          posDetail.get<4>().get<6>()= false;   // isLastTurn flag
          posDetail.get<4>().get<7>()= false;   // isLastLayer flag
          posDetail.get<4>().get<8>()= coilAngle;  // coil angle
          // set the hqp and layer adj
          posDetail.get<5>().get<0>() = 0; // hqp adjust
          posDetail.get<5>().get<1>() = 0; // layer adjust
          return RTN_ERROR;  // return error sentinel value
          }
        }
      else if ((isEven && FOOT_ROLE_RETREATING == role) || (!isEven && FOOT_ROLE_ADVANCING == role)) {
        // Even layer and a retreating foot or odd layer and and an advancing foot
        // outer columns move
        // Set outer column selection bit
          // (column variable * 2) + 1 is the index to the inner column, +1 more is the index to the outer column
        if (0 <= column && 5 >= column) { // range check column
          // column value okay
          // set column outer foot position and mark as selected. Clear the other axes.
          SetSelectedScsAxis((column * 2) + 2, fDistPos1, posDetail, mode);
          }
        else {  // bad column value
          UnselectAllScsAxes(posDetail);
          posDetail.get<4>().get<0>()= trace + "column value out of range. col: " + std::to_string(static_cast<long long>(column)); // append error message to trace
          posDetail.get<4>().get<1>()= false;  // isAbsolute flag. force to false so that a move to abs 0 cannot happen
          posDetail.get<4>().get<2>()= false;  // isInTransition flag
          posDetail.get<4>().get<3>()= false;  // isInJoggle flag
          posDetail.get<4>().get<4>()= false;  // isNewHqp flag
          posDetail.get<4>().get<5>()= false;  // isNewLayer flag
          posDetail.get<4>().get<6>()= false;   // isLastTurn flag
          posDetail.get<4>().get<7>()= false;   // isLastLayer flag
          posDetail.get<4>().get<8>()= coilAngle;  // coil angle
          // set the hqp and layer adj
          posDetail.get<5>().get<0>() = 0; // hqp adjust
          posDetail.get<5>().get<1>() = 0; // layer adjust
          return RTN_ERROR;  // return error sentinel value
          }
        }
      else {
        // error case -- shouldn't get here
        UnselectAllScsAxes(posDetail);
        posDetail.get<4>()= trace + "Column role error."; // append error message to trace
          posDetail.get<4>().get<1>()= false;  // isAbsolute flag. force to false so that a move to abs 0 cannot happen
          posDetail.get<4>().get<2>()= false;  // isInTransition flag
          posDetail.get<4>().get<3>()= false;  // isInJoggle flag
          posDetail.get<4>().get<4>()= false;  // isNewHqp flag
          posDetail.get<4>().get<5>()= false;  // isNewLayer flag
          posDetail.get<4>().get<6>()= false;   // isLastTurn flag
          posDetail.get<4>().get<7>()= false;   // isLastLayer flag
          posDetail.get<4>().get<8>()= coilAngle;  // coil angle
          // set the hqp and layer adj
          posDetail.get<5>().get<0>() = 0; // hqp adjust
          posDetail.get<5>().get<1>() = 0; // layer adjust
          return RTN_ERROR;  // return error sentinel value
        }

      // set the isSelectedAxes Flag
      posDetail.get<2>()[0]= true;

      // set the, trace, isAbsolute, isTransition and isJoggleAdj, and the rest of the posDetail values and return
      posDetail.get<4>().get<0>()= trace;  // logic trace string
      // set the absolute flag depending on the mode
      posDetail.get<4>().get<1>()= (IM_ABS_SEL == mode || IM_ABS_UPDATE_SEL == mode);  // isAbsolute flag (mode is absolute)
      posDetail.get<4>().get<2>()= isInTransition;  // isInTransition flag
      posDetail.get<4>().get<3>()= isInJoggle;  // isInJoggle flag
      posDetail.get<4>().get<4>()= isNewHqp;  // isNewHqp flag
      posDetail.get<4>().get<5>()= isNewLayer;  // isNewHqp flag
      posDetail.get<4>().get<6>()= isLastTurn;   // isLastTurn flag
      posDetail.get<4>().get<7>()= isLastLayer;   // isLastLayer flag
      posDetail.get<4>().get<8>()= coilAngle;  // coil angle

      // set the hqp and layer adj
      posDetail.get<5>().get<0>() = hqpAdj; // hqp adjust
      posDetail.get<5>().get<1>() = layerAdj; // layer adjust

      return RTN_NO_ERROR;  // return ok
      }
    else // no mode selected
      return RTN_ERROR;  // return ok
    } //AxisPositions::PopScsPosDetail()

  // Set the selection bit for the specfied axis and set the distance in element 0 of 
    // the foot axis positions vector.
    // Iterate over the axis index enumeration vector and find the one matching the index.
    // Set the selection bit for the one that maches, and clear the others
    // passed in index 1-12 maps to foot A inner (1) - F outer (12). 13-24 maps to column A inner (13) - F outer (24)
  void AxisPositions::SetSelectedScsAxis(AxisIndexes index, double distPos, SPosDetail &posDetail, insertMode mode) {
    // set the distance/position
    posDetail.get<3>().get<0>()= distPos; 
    // set the index value -- this is for diagnostics and troubleshooting and display
    posDetail.get<3>().get<1>()= index; 
    // set the absolute adjust flag depending on mode - this identifies the 
    // distPos as an adjustment to an absolute value resulting in a new absolute value when set
    // or simply an absolute value when clear
    posDetail.get<3>().get<2>()= (IM_ABS_UPDATE_SEL == mode); 

    for (std::vector<AxisIndexes>::size_type axi = 1; axi != vAxisIndexes_.size() - 1; ++axi) {
      // axi is the axis index, and iterates from 1-24
      // AxisIndexes size is 1 more than the number of columns because element 0 holds the enum value "Unknown axis ID"
      // see if the index matches or not

      if (vAxisIndexes_[axi] == index) { // index matches -- set the distance and selection bit
        posDetail.get<2>()[axi]= true;  // set the selected axis flag
        }
      else if (vAxisIndexes_[axi] != index) {  // index does not match, - clear the selection
        // clear the non-matching distances and selections
        posDetail.get<2>()[axi]= false;  // clear the selected axis flag
        }
      } // iterate over axis indexes
    } //  AxisPositions::SetSelectedScsAxis(AxisIndexes index, double distPos, SPosDetail &posDetail)

  // Set the selection bit for the specfied axis and set the distance in element 0 of 
    // the foot axis positions vector.
    // Iterate over the axis index enumeration vector and fine the one matching the index.
    // Set the selection bit for the one that maches, and clear the others
    // passed in index 1-12 maps to foot A inner (1) - F outer (12). 13-24 maps to column A inner (13) - F outer (24)
  void AxisPositions::SetSelectedScsAxis(long index, double distPos, SPosDetail &posDetail, insertMode mode) {
    // range check the index
    if (1 <= index && (COLUMN_COUNT * 2) >= index) {
      // index is okay
      // lookup the index and call the overload
      AxisIndexes idx = vAxisIndexes_[index];
      SetSelectedScsAxis(idx, distPos, posDetail, mode);
      }
    } // AxisPositions::SetSelectedScsAxis(long index, double distPos, SPosDetail &posDetail)

  // Clear the selection bit for the specified axis.
    // Iterate over the axis index enumeration vector and fine the one matching the index. Clear the selection bit
    // for the one that maches.
    // passed in index 1-12 maps to foot A inner (1) - F outer (12). 13-24 maps to column A inner (13) - F outer (24)
  void AxisPositions::UnselectSelectedScsAxis(AxisIndexes index, SPosDetail &posDetail) {
    // convert the index enumeration to a long and call the overload
    long idx = static_cast<long>(index);
    UnselectSelectedScsAxis(idx, posDetail);
    } // AxisPositions::UnselectSelectedScsAxis(AxisIndexes index, SPosDetail &posDetail)
    
  // Clear the selection bit for the specified axis.
    // Iterate over the axis index enumeration vector and fine the one matching the index. Clear the selection bit
    // for the one that maches.
    // passed in index 1-12 maps to foot A inner (1) - F outer (12). 13-24 maps to column A inner (13) - F outer (24)
  void AxisPositions::UnselectSelectedScsAxis(long index, SPosDetail &posDetail) {
    if (1 <= index && (COLUMN_COUNT * 2) >= index) { // range check index
      // index is okay

      // set the selected detail distance to 0
      posDetail.get<3>().get<0>()= 0; 
      // set the selected detail index to unknown -- this is for diagnostics and troubleshooting (logic does not use this)
      posDetail.get<3>().get<1>()= AXIDX_UNKNOWN; 
      
      posDetail.get<2>()[index]= false;  // clear the selected axis flag
      }
    } // AxisPositions::UnselectSelectedScsAxis(long index, SPosDetail &posDetail)
    
  // Iterate over the axis index enumeration vector and clear all the selections
  void AxisPositions::UnselectAllScsAxes(SPosDetail &posDetail) {

    // set the selected detail distance to 0
    posDetail.get<3>().get<0>()= 0; 
    // set the selected detail index to unknown -- this is for diagnostics and troubleshooting (logic does not use this)
    posDetail.get<3>().get<1>()= AXIDX_UNKNOWN; 

    for (std::vector<AxisIndexes>::size_type axi = 1; axi != vAxisIndexes_.size() - 1; ++axi) {
      // axi is the axis index, and iterates from 1-24
      // AxisIndexes size is 1 more than the number of columns because element 0 holds the enum value "Unknown axis ID"
      posDetail.get<2>()[axi]= false;  // clear the selected axis flag
      } // iterate over axis indexes
    } // AxisPositions::UnselectAllScsAxes(SPosDetail &posDetail) 
    

  // Map the specified angle to the position detail in the SCS Axis Position Map
  void AxisPositions::MapScsAxisMoves(long angle, const SPosDetail &posDetail) {
    // add the angle and associated properties to the map
    scsAxisPositionMap_[angle]=posDetail;
    } //  AxesPositions::MapScsAxisMoves()

  void AxisPositions::MapScsAxisMoves(double angle, const SPosDetail &posDetail) {
    // convert the angle and call the overload
    MapScsAxisMoves(static_cast<long>(round(angle)), posDetail);
  } //  AxesPositions::MapScsAxisMoves()

    // Enter the post load mode foot positions into the postion map at the specified angle.
      // Used to "seed" the starting point for relative adjustments to absolute positons later on.
    // Return value is error status
  long AxisPositions::MapPostLoadScsPositions(double angle, bool isInJoggleWindow) {
    // The Lb angle for the passed in angle should correspond to a Joggle (fc = "J") in the Coil Map.
    // A Local Zero (fc = 'L') should be next (differentiator from new layer case), but there may 
    // be a He pipe, so instead the hqp value is checked for a change.
    // This is the the place where Load Mode is used to load a HQP. 
    // Create and initialize a local position detail structure.
    // Determine the angle for the entry as follows:
      // Ria Angle (Load Hex) = (Coil Angle for the Joggle + Joggle Window) - Retreating Foot Offset
      // Where the Coil Angle for the Joggle should be the coil map Lb angle of the passed in angle
      // and the Joggle Window is a configured constant and the Retreating Foot Offset is a constant.
      // There are two configured constants for the Joggle window (turn 1 and turn 14), but this case
      // only ever applies to the turn 1 case, so this is the one that is always used.
      //
      // hqpAdj and layerAdj values are determined. These values are added to the layer and hqp values from the 
      // coil map to get the correct hqp and layer values.  Storing these in the position table makes the 
      // SQL view of the position table which shows the correct hqp and layer values much simpler. 
        // This Procedure is for new HQP, so the hqp and layer adjustment is as follows:
        // New hqp, not in a joggle -- Hqp and layer adjust are both +1
        // New hqp, in a joggle -- Hqp and layer adjust are both 0
    // Populate the local position detail structure and put the inner feet to the retracting foot start position, 
    // and the outer feet to the advancing foot start position at the calculated angle.
    // Mark the columns with a sentinel position, as their desired position is not
    // known until run time.  Mark the row as being a new HQP.
    // Even layer is false (this always happens on a odd layer)
    // Foot role, inTransition and isJoggleAdj flags aren't determined by logic

    SPosDetail posDetail; // local position detail
    // make space
    posDetail.get<0>().assign(COLUMN_COUNT, INITIAL_NO_POSITION); // foot position vector. size to the number of columns and init
    posDetail.get<1>().assign(COLUMN_COUNT, INITIAL_NO_POSITION); // column position vector. size to the number of columns and init
    posDetail.get<2>().assign((COLUMN_COUNT * 2) + 1, false); // selected axis vector. make space for each axis plus 1 for the flag, and init to false.

    // make a place to store a trace string, the new hqp angle, and the hqp and layer adjustments
    std::string trace;
    double newHqpAngle;
    long hqpAdj, layerAdj;
    
    if ( !isInJoggleWindow) {
      // Look forward to get the next joggle
      newHqpAngle = coilMap_.GetJoggleUb(angle);
      // calculate the start angle of the HQP = (Joggle + Joggle Window) - Retreating Foot Offset
      newHqpAngle = (newHqpAngle + JOGGLE_LENGTH_MIN) - RET_FOOT_RIA_OFFSET_ANGLE;
      // new hqp and not in a joggle. Set adjustment values to +1
      hqpAdj= 1;
      layerAdj= 1;
      trace = "Post Load Positions. Column Angle: " + std::to_string(static_cast<long long>(angle)) + ". Inner feet to Retr. Start, Outer to Adv. Start. Columns not known, use sentinel.";
      }
//    else if (NO_FEATURE != newHqpAngle && isInJoggleWindow) { // a column is in a joggle window
    else if (isInJoggleWindow) { // a column is in a joggle window
    // Look back to the Joggle angle
      newHqpAngle = coilMap_.GetJoggleLb(angle);
      // valid Joggle angle found
      // calculate the start angle of the HQP = (Joggle + Joggle Window) - Retreating Foot Offset
      newHqpAngle = (newHqpAngle + JOGGLE_LENGTH_MIN) - RET_FOOT_RIA_OFFSET_ANGLE;
      // new hqp and in a joggle. Set adjustment values to 0
      hqpAdj= 0;
      layerAdj= 0;
      trace = "Post Load Positions (column in joggle window). Column Angle: " + std::to_string(static_cast<long long>(angle)) + ". Inner feet to Retr. Start, Outer to Adv. Start. Columns not known, use sentinel.";
     }
    else {
      // invalid joggle angle returned
      // use the sentinel value
      newHqpAngle= POSITION_NOT_CALCULATED;
      hqpAdj= 0;
      layerAdj= 0;
      trace= "Error looking up HQP LB at Column Angle: " + std::to_string(static_cast<long long>(angle)) + ". Trying to enter post load feet positions.";
      }
    // start new hqp with inner (retracting) feet at the retracting start position
      // and the outer (advancing) feet at the advancing start position
    // even layer is false, 
    // inTransition and isJoggle are not calculated and are set to false
    // isNewHqp is set true
    long popDetailStatus= PopScsPosDetail(angle, false, FOOT_ROLE_ADVANCING, IM_ABS_ALL,  // angle, isEven, foot role, insert mode
                                          RETREATING_FOOT_START_POS, ADVANCING_FOOT_START_POS,  // inner feet to posDist 1, outer feet to posDist 2
                                          trace, false, isInJoggleWindow, true, false, // trace, isTransition, isjoggle, isNewHqp, isNewLayer,
                                          false, false, hqpAdj, layerAdj, posDetail);         // isLastTurn, isLastLayer, hqpAdj, layerAdj, posDetail
    // add to the map if the populate was successful
    if (RTN_NO_ERROR == popDetailStatus)
      MapScsAxisMoves(newHqpAngle, posDetail);
    else 
      std::cout << std::endl << "** ERROR ** Populating new HPQ foot positions at Ria angle " << newHqpAngle << " degrees. Row Not added." << std::endl;

    // return status
    return popDetailStatus;
    } // AxisPositions::MapPostLoadScsPositions(double angle)

  // Add the post load scs positions and the initial retreat and advance moves at the specified angles.
  // After the post load SCS positions, at the beginning of a coil, the F Inner/Outer pair need to move to
    // release the tail so the lead can be lowered. F Inner is moved by the transition adjustment amount, and then
    // F Outer follows suit. From then on, moves and transition adjustment is calculated normally.  
  // Return via reference the transition adjustment value
  // Return value is error status
  long AxisPositions::MapCoilStartScsPositions(double socPostLoadAngle, double socInitRetrAngle, double socInitAdvAngle, double &tAdj) {
    // Post load scs positions
    // Populate the local position detail structure and put the inner feet to the retracting foot start position, 
    // and the outer feet to the advancing foot start position at the specified angle.
    // Mark the columns with a sentinel position, as their desired position is not
    // known until run time.  Mark the row as being a new HQP.
    // Even layer is false (layer 1)
    // Foot role, inTransition and isJoggleAdj flags aren't determined by logic
    // hqpAdj and layerAdj shoulc be zero.

    std::string trace; // make a place to store a trace string
    long popDetailStatus; // keep track of population of pos detail status
    long rtnStat= RTN_NO_ERROR; // function return value.  Assume function will be sucessful. An error will change this.

    SPosDetail posDetail; // local position detail
    // make space
    posDetail.get<0>().assign(COLUMN_COUNT, INITIAL_NO_POSITION); // foot position vector. size to the number of columns and init
    posDetail.get<1>().assign(COLUMN_COUNT, INITIAL_NO_POSITION); // column position vector. size to the number of columns and init
    posDetail.get<2>().assign((COLUMN_COUNT * 2) + 1, false); // selected axis vector. make space for each axis plus 1 for the flag, and init to false.

      // Set positions for the start of coil, an odd layer (1) -- Inner feet are retreating, and outer feet are advancing.
      // For the coil angle, use a position 10 degrees before the column F, specified as a negative azimuth.
      trace= "Start of coil positions. Column Angle: " + std::to_string(static_cast<long long>((F_COLUMN_AZIMUTH - 10) - 360.0)) + 
             ". Inner feet to retr. start, Outer feet to adv. start. Columns not known, use sentinel.";

      popDetailStatus= PopScsPosDetail(((E_COLUMN_AZIMUTH) - 360.0), false, FOOT_ROLE_ADVANCING, IM_ABS_ALL,  // coil angle, isEven, foot role (does not matter), insert mode
                                      RETREATING_FOOT_START_POS, ADVANCING_FOOT_START_POS,  // inner feet to posDist 1, outer feet to posDist 2
                                      trace, false, false, true, false, // trace, isTransition, isjoggle, isNewHqp, isNewLayer
                                      false, false, 0, 0, posDetail);         // isLastTurn, isLastLayer, hqpAdj, layerAdj, posDetail

      // add to the map if the populate was successful. Don't add to map, but set the return value to error if not sucessful.
      if (RTN_NO_ERROR == popDetailStatus)
        MapScsAxisMoves(socPostLoadAngle, posDetail); // ria angle used here
      else {
        std::cout << std::endl << "** ERROR ** Populating Soc Post Load Angle at Ria angle " << socPostLoadAngle << " degrees. Row Not added." << std::endl;
        rtnStat= RTN_ERROR;
        }

    // F Inner and then F Outer should always be the first two moves of a new coil. The "main loop"
    // iterates from column angle 30 on, and so the first move generated by the loop is for the A pair at column angle 30.
    // In this case, the F pair is done outside this loop at the specified angles.

    // use the F column azimuth to figure out the transition amount
    bool stat, result; // local results
    double degToPrevTrans;
    stat= coilMap_.isInTransitionLb(F_COLUMN_AZIMUTH, result, degToPrevTrans);
    if (stat && result) { // in a transition window -- this should always be true for the beginning of the coil

      // Mark the column as needing adjustment to keep track of this column
      // the the next time through (vs other columns no near the transition) and
      // then calculate transition adjustment
      // Normally, the adjustment for this turn would be the adjustment needed for this turn 
      // less the amount already adjusted: (tAdj - accumAdj). Since there have
      // been no previous adjustments, the entire adjustment is used.
      
      // mark this column as needing a transition adjustment
      SetTransAdjust(F_COLUMN_AZIMUTH);
      
      tAdj= CalculateTransitionAdjustment(F_COLUMN_AZIMUTH);

      // Make a record for F Inner Foot 
      // populate the trace message
      // Specify the coil angle as the F Column Azimuth as a negative azimuth to emphasize this is "before" the
      // start of the coil.
      trace= "Release lead.  Column Angle: " + std::to_string(static_cast<long long>(F_COLUMN_AZIMUTH - 360.0)) + 
             ". F Inner past trans. by "  + std::to_string(static_cast<long double>(degToPrevTrans)) + 
                        " degs. Retract " + std::to_string(static_cast<long double>(tAdj)) + " mm.";

      // Retract F Inner in by the transition adjustment amount (odd layer, retracting)
      // The distance specified is relative, but absolute update selected mode is used so in the db, an absolute entry based on the previous
      // positions gets created.
      // The retreating foot move is positive.
      popDetailStatus= PopScsPosDetail((F_COLUMN_AZIMUTH - 360.0), false, FOOT_ROLE_RETREATING, IM_ABS_UPDATE_SEL,  // coil angle, isEven, foot role, mode (absolute update select)
                                      abs(tAdj), 0,  // posDist 1, posDist 2 (not used)
                                      trace, true, false, false, false, // trace, isTransition, isjoggle, isNewHqp, isNewLayer,
                                      false, false, 0, 0, posDetail);         // isLastTurn, isLastLayer, hqpAdj, layerAdj, posDetail

      // add to the map if the populate was successful. Don't add to map, but set the return value to error if not sucessful.
      if (RTN_NO_ERROR == popDetailStatus)
        MapScsAxisMoves(socInitRetrAngle, posDetail); // ria angle used here
      else {
        std::cout << std::endl << "** ERROR ** Populating Soc Init Retr Angle at Ria angle " << socInitRetrAngle << " degrees. Row Not added." << std::endl;
        rtnStat= RTN_ERROR;
        }

      // Make a record for F Outer Foot 
      // populate the trace message
      // Specify the coil angle as the F Column Azimuth as a negative azimuth to emphasize this is "before" the
      // start of the coil.
      trace= "Lead released.  Column Angle: " + std::to_string(static_cast<long long>(F_COLUMN_AZIMUTH - 360.0)) + 
             ". F Outer past trans. by "  + std::to_string(static_cast<long double>(degToPrevTrans)) + 
                        " degs. Advance " + std::to_string(static_cast<long double>(tAdj)) + " mm.";

      // Advance F Outer in by the transition adjustment amount (odd layer, advancing)
      // The distance specified is relative, but absolute selected mode is used so in the db, an absolute entry based on the previous
      // positions gets created.
      // The advancing foot move is negative.
      popDetailStatus= PopScsPosDetail((F_COLUMN_AZIMUTH - 360.0), false, FOOT_ROLE_ADVANCING, IM_ABS_UPDATE_SEL,  // coil angle, isEven, foot role, mode (absolute update select)
                                      (-1 * abs(tAdj)), 0,  // posDist 1, posDist 2 (not used)
                                      trace, true, false, false, false, // trace, isTransition, isjoggle, isNewHqp, isNewLayer,
                                      false, false, 0, 0, posDetail);         // isLastTurn, isLastLayer, hqpAdj, layerAdj, posDetail

      // add to the map if the populate was successful. Don't add to map, but set the return value to error if not sucessful.
      if (RTN_NO_ERROR == popDetailStatus)
        MapScsAxisMoves(socInitAdvAngle, posDetail);  // ria angle used here
      else {
        std::cout << std::endl << "** ERROR ** Populating Soc Init Adb Angle at Ria angle " << socInitAdvAngle << " degrees. Row Not added." << std::endl;
        rtnStat= RTN_ERROR;
       }

    }
  else { // not in a transition window -- This should never be the case for the F column first move, because
         // the column position is fixed next to the relatively fixed lead position.
    }

  // return status
  return rtnStat;
  } // AxisPositions::MapCoilStartScsPositions()

    // Enter the new layer foot positions into the postion map at the specified angle.
      // Used to "seed" the starting point for relative adjustments to absolute positons later on.
      // Return value is error status
  long AxisPositions::MapNewLayerScsPositions(double riaAngle, double coilAngle, bool isEven, bool isLastLayer, bool isInJoggleWindow, bool isNewHqp) {
    // For an even layer, inner feet go to advancing start, outer feet go to retreating start.
    // Populate the local position detail structure with the starting positions at the passed in angle.
    // Mark the columns with a sentinel position, as their desired position is not
    // known until run time.  Mark the row as being a new Layer.
    // Foot role, inTransition and isJoggleAdj flags aren't determined by logic, and are set to false
    // Foot Role is not used with the Absolute all insert mode.
    //
    // hqpAdj and layerAdj values are determined. These values are added to the layer and hqp values from the 
    // coil map to get the correct hqp and layer values.  Storing these in the position table makes the 
    // SQL view of the position table which shows the correct hqp and layer values much simpler. 
    // This Procedure is for new layer, so the hqp and layer adjustment is as follows:
    // New layer, not in a joggle -- Hqp adj is 0 and layer adjust is +1
    // New layer, in a joggle -- Hqp and layer adjust are both 0

    SPosDetail posDetail; // local position detail
    // make space
    posDetail.get<0>().assign(COLUMN_COUNT, INITIAL_NO_POSITION); // foot position vector. size to the number of columns and init
    posDetail.get<1>().assign(COLUMN_COUNT, INITIAL_NO_POSITION); // column position vector. size to the number of columns and init
    posDetail.get<2>().assign((COLUMN_COUNT * 2) + 1, false); // selected axis vector. make space for each axis plus 1 for the flag, and init to false.

    // make a place to store a trace string, the new hqp angle, and the hqp and layer adjustments
    std::string trace;
    long hqpAdj, layerAdj;
    long popDetailStatus= RTN_ERROR; // keep track of population of pos detail status. Init to error in case if falls thru
       
    if (isEven && !isInJoggleWindow) {  // even layer, not in joggle window
      // Set positions for start of an even layer -- Inner feet are advancing, and outer feet are retreating
      trace = "New Even Layer Start Positions. Column Angle: " + std::to_string(static_cast<long long>(coilAngle)) + 
              ". Inner feet to adv. start, Outer feet to retr. start. Columns not known, use sentinel.";
      // at a new layer, but not at a joggle. hqp adj is 0 and layer adj is+1
      hqpAdj= 0;
      layerAdj= 1;
      popDetailStatus= PopScsPosDetail(coilAngle, isEven, FOOT_ROLE_ADVANCING, IM_ABS_ALL,  // angle, isEven, foot role, insert mode
                                      ADVANCING_FOOT_START_POS, RETREATING_FOOT_START_POS,  // inner feet to posDist 1, outer feet to posDist 2
                                      trace, false, isInJoggleWindow, isNewHqp, true, // trace, isTransition, isjoggle, isNewHqp, isNewLayer
                                      false, isLastLayer, hqpAdj, layerAdj, posDetail);         // isLastTurn, isLastLayer, posDetail
    }
    else if (isEven && isInJoggleWindow) {  // even layer, in joggle window
      // Set positions for start of an even layer -- Inner feet are advancing, and outer feet are retreating
      trace = "New Even Layer Start Positions (column in joggle window). Column Angle: " + std::to_string(static_cast<long long>(coilAngle)) +
              ". Inner feet to adv. start, Outer feet to retr. start. Columns not known, use sentinel.";
      // at a new layer, and at a joggle. hqp and layer adjustment are 0
      hqpAdj= 0;
      layerAdj= 0;
      popDetailStatus= PopScsPosDetail(coilAngle, isEven, FOOT_ROLE_ADVANCING, IM_ABS_ALL,  // angle, isEven, foot role, insert mode
                                      ADVANCING_FOOT_START_POS, RETREATING_FOOT_START_POS,  // inner feet to posDist 1, outer feet to posDist 2
                                      trace, false, isInJoggleWindow, isNewHqp, true, // trace, isTransition, isjoggle, isNewHqp, isNewLayer
                                      false, isLastLayer, hqpAdj, layerAdj, posDetail);         // isLastTurn, isLastLayer, posDetail
    }
    else if  (!isEven && !isInJoggleWindow) { // odd layer, not in joggle window
      // Set positions for start of an odd layer -- Inner feet are retreating, and outer feet are advancing.
      trace = "New Odd Layer Start Positions. Column Angle: " + std::to_string(static_cast<long long>(coilAngle)) + ". Inner feet to retr. start, Outer feet to adv. start. Columns not known, use sentinel.";
      // at a new layer, but not at a joggle. hqp adj is 0 and layer adj is +1
      hqpAdj= 0;
      layerAdj= 1;
      popDetailStatus= PopScsPosDetail(coilAngle, isEven, FOOT_ROLE_ADVANCING, IM_ABS_ALL,  // angle, isEven, foot role, insert mode
                                      RETREATING_FOOT_START_POS, ADVANCING_FOOT_START_POS,  // inner feet to posDist 1, outer feet to posDist 2
                                      trace, false, isInJoggleWindow, isNewHqp, true, // trace, isTransition, isjoggle, isNewHqp, isNewLayer
                                      false, isLastLayer, hqpAdj, layerAdj, posDetail);         // isLastTurn, isLastLayer, posDetail
    }
    else if (!isEven && isInJoggleWindow) { // odd layer, in joggle window
      // Set positions for start of an odd layer -- Inner feet are retreating, and outer feet are advancing.
      trace = "New Odd Layer Start Positions (column in joggle window). Column Angle: " + std::to_string(static_cast<long long>(coilAngle)) + ". Inner feet to retr. start, Outer feet to adv. start. Columns not known, use sentinel.";
      // at a new layer, and at a joggle. hqp and layer adjustment are 0
      hqpAdj= 0;
      layerAdj= 0;
      popDetailStatus= PopScsPosDetail(coilAngle, isEven, FOOT_ROLE_ADVANCING, IM_ABS_ALL,  // angle, isEven, foot role, insert mode
                                      RETREATING_FOOT_START_POS, ADVANCING_FOOT_START_POS,  // inner feet to posDist 1, outer feet to posDist 2
                                      trace, false, isInJoggleWindow, isNewHqp, true, // trace, isTransition, isjoggle, isNewHqp, isNewLayer
                                      false, isLastLayer, hqpAdj, layerAdj, posDetail);         // isLastTurn, isLastLayer, posDetail
    }
    // add to the map if the populate was successful
    if (RTN_NO_ERROR == popDetailStatus)
      MapScsAxisMoves(riaAngle, posDetail);
    else {
      std::cout << std::endl << "** ERROR ** Populating New layer positions at angle " << riaAngle << ". Row Not added." << std::endl;
      }

    // return status
    return popDetailStatus;
    } // AxisPositions::MapNewLayerScsPositions(double riaAngle, double coilAngle, bool isEven, bool isLastLayer, bool isInJoggleWindow, bool isNewHqp);

    
  // This function is the *main workhorse* of the class for creating the Scs position map.
  // It uses the other helper functions to make an entry in the Scs postion map for
  // each foot/column pair (in/out) azimuth.
  // The Cls position table is made by a sql stored procedure once the scs position table is made,
  // so there is no cls map.
  //
  // hqpAdj and layerAdj values are determined. These values are added to the layer and hqp values from the 
  // coil map to get the correct hqp and layer values.  Storing these in the position table makes the 
  // SQL view of the position table which shows the correct hqp and layer values much simpler. 
  // The adj values for the new hqp and new layer cases are handled within the member fuctions wthich make those rows. 
  // The adj values for the other rows are handled in this procedure. The hqp and layer adjustment is as follows:
  // In a joggle (that isn't a new layer or new hex), and **only on the last layer** -- Hqp adj is 0 and layer adjust is -1
  // Not in a joggle -- Hqp and layer adjust are both 0
  void AxisPositions::CalculateAxisMoves() {
    // keep track of which turn the feet are under
    long retreatingColumnTurn= 0;
    long advancingColumnTurn= 0;

    // hold looked up layer number, hex number, and their previous values
    long layerNumber= 0;
    long layerNumberPrev= 0;
    long hqpNumber= 0;
    long hqpNumberPrev= 0;
    // even/odd layer flag
    bool isEvenLayer = false;

    // transition adjustment
    bool inTransition = false;
    double tThisAdj = 0; // transition adjustment amount due to current turn
    double tAccumAdj = 0; // accumulated transition adjustments
    double degToPrevTrans; // degrees to the previous transition

    // joggle adjustment
    bool isJoggleAdj = false; // is a joggle affecting foot behavior
    joggleAdjustmentType jAdjType= JA_RET_NOM_ADV_NOM; // joggle adjustment type initialized to nominal
    double jAdj = 0;  // joggle adjustment amount
    double degToNextJoggle; // degrees to next joggle
    double degToPrevJoggle; // degrees to the previous joggle

    // last turn flag
    bool isLastTurn = false;
    
    // last layer flag
    bool isLastLayer = false;
  
    // SCS insert mode
    insertMode scsInsertMode; // which insert mode to use is determined below, and this determines which sql procedure to call
    
    // make a place to store a trace string, the new hqp angle, and the hqp and layer adjustments
    std::string logicTrace;
    long hqpAdj, layerAdj;

    long popDetailStatus; // keep track of population of pos detail status

    // CurrentAngle starts out at 30, and is incremented by 60
    // until it reaches the end of the coil.
    // Each value of currentAngle corresponds to a column azimuth.
    // A CLS foot move distance is calculated for each of these positions
    // A SCS foot position is calculated for each of these positions
    // The column positions are not known until run-time, so a positions
      // for the columns are not calculated, and a sentinel value is used for the columns
      // where a position is needed.
    
    bool result; // the results of the tested condition
    bool stat; // indicates status

    // variables to deal with displaying progress
    double iterations = COIL_ANGLE_MAX / COLUMN_INCREMENT;  // number of loop iterations for display purposes -- use double so pctDone does not use interger math
    long count= 0;  // loop counter for display purposes
    long pctDone= 0;  // percent done

    // display progress
    std::cout << "Approximately " << static_cast<long>(iterations) << " angles to iterate" << std::endl;
    std::cout << "between " << INITIAL_COLUMN_ANGLE << " and " << COIL_ANGLE_MAX << "." << std::endl;
    std::cout << "Only angles falling on column azimuths are processed." << std::endl;

    for (long currentAngle= INITIAL_COLUMN_ANGLE; currentAngle <= COIL_ANGLE_MAX; currentAngle += COLUMN_INCREMENT) {
      // display progress
      ++count;
      pctDone= static_cast<long>(100 * (count / iterations));  // truncate fractional percentages
      std::cout << "On angle " << currentAngle << " of " << COIL_ANGLE_MAX << " (" << pctDone << " %)\r" << std::flush; // no linefeed so this line will be overwritten next time thru

      // capture current angle in logic trace
      logicTrace= "Column Ang: " + std::to_string(static_cast<long long>(currentAngle)) + ", ";
      
      // See if the current angle (column) is the last retreat of the layer,
      // or the first move of the coil (beginning of the coil).
      // If we are at the start of the coil,
        // then make a new hex entry at the start of the table,
        // and add a row to move F Inner in by the transition adjustment amount to release 
        // the tail. This initial move is inserted because the tail/lead region at the beginning
        // of the coil behaves differently than the rest of the coil.
      // If we are not a the start of the coil, use the isLastMoveOfLayer test.
        // If this test is true, then make a new layer or a new hex entry ahead of where we are!
        // The moves for the feet corresponding to this column are made later (elsewhere).

      double joggleAngle= RTN_NO_RESULTS; // hold closest joggle angle if this is the last retreat of the layer/hex/quad
      bool isInJoggleWindow= false; // hold if in joggle window if this is the last retreat of the layer/hex/quad
      if (currentAngle == INITIAL_COLUMN_ANGLE) {
        // beginning of the coil
        // set the hqp number, layer number, and associated previous values to 1 so they will catch changes from this point on.
        hqpNumber= 1;
        hqpNumberPrev= 1;
        layerNumber= 1;
        layerNumberPrev= 1;

        // Make a new hqp record at an angle earlier than normal to make room for the additional column F moves that 
        // are needed at the start of the coil, then
        // Insert move for F pair to retract by the adjustment amount in order to release the lead tail at the start of the coil
        // Normally the post load position is = (newHqpAngle + JOGGLE_LENGTH_MIN) - RET_FOOT_RIA_OFFSET_ANGLE, 
        // which is -83 when truncated.  Instead, force these initial rows to the specified constants.  This preserves
          // the 10,50 retract, advance pattern
          //  -140 for the post load,
          //  -130 for the F Inner Retract
          //  -80 for the F Outer Advance
          // ... continue normally
        // Return via reference the transition adjustment value, and use it to update the accumulated
        // adjustment so the adjustment made here can be taken into consideration when making subsequent
        // moves of feet that are on, drift onto, or drift off of a transition.
        MapCoilStartScsPositions(SOC_POST_LOAD_ANGLE, SOC_INIT_RETR_ANGLE, SOC_INIT_ADV_ANGLE, tAccumAdj);

        }
      else { // not the beginning of the coil
        stat = coilMap_.isLastMoveOfLayer(currentAngle, result, joggleAngle, isInJoggleWindow);
        if (stat && result) {
          // A new layer/hex/quad is about to start. See if this is the start of a hex/quad by looking up the values at the
          // joggle angle and seeing if they have changed.  TRICKY: The order is important -- the layer number changes at every
          // joggle, but the hex/quad number only changes at a new hex -- check for hex first so it keeps from also checking new layer
          // One should always be true if we got here.

          // look up the hex/quad number and the layer value at the joggle
          hqpNumber= coilMap_.GetHexQuadNumberLb(joggleAngle);
          layerNumber= coilMap_.GetLayerLb(joggleAngle);

          // check for a new hex or layer number
          if (hqpNumber != hqpNumberPrev) {
            // a new hex is about to start -- Make new hqp record
            MapPostLoadScsPositions(currentAngle, isInJoggleWindow);
            // update the previous hqp value
            hqpNumberPrev= hqpNumber;
            } 
          else if (layerNumber != layerNumberPrev) {
            // a mew layer is about to start -- Make new layer record

            // see if we are on the last layer
            isLastLayer = coilMap_.isLastHqLayer(layerNumber);

            // Determine the ria angle where the starting positions should go.
            // Note that if the joggle angle (returned from the last move of layer check above) is > the current angle,
            // then we are before the joggle.  If the joggle angle is < the current angle, then we are within a joggle (past the start)
            // Either way, pass in the joggle angle so we can easily determine if the new layer is odd or even.
            angleIsEven_pair prAie = CalculateNewLayerRiaAngle(currentAngle, joggleAngle); // prAie -- (Angle,Is Even) pair
            // Create and map the resulting position detail
            MapNewLayerScsPositions(prAie.first, currentAngle, prAie.second, isLastLayer, isInJoggleWindow, false);
            // update the previous layer value
            layerNumberPrev= layerNumber;
            }
          else { // error case - should not get here if a new hex/quad was detected
            logicTrace.append("Error looking for new layer or hqp @ angle: " + std::to_string(static_cast<long long>(currentAngle)) + ". ");
            }
          }
        else if (stat & !result) {
          // a new layer/hex is not about to start -- do nothing
          }
        else { // error case - should never get here
          logicTrace.append("Error looking for new layer or hqp @ angle: " + std::to_string(static_cast<long long>(currentAngle)) + ". ");
          }
        }

      // Joggles affect moves when a foot is roughtly (need to take the joggle length into account):
        // Region 1: 360 degrees before a joggle,
        // Region 2: near or just past a joggle, and 
        // Region 3: 360 degrees after a joggle.
      // Check if an adjustment is needed for a joggle.  (Distance to joggle and adjustment value are passed back by reference)
      // If no joggle adjustment is needed, then adjustments may be needed for
      // columns starting layers near transitions, and for the end of a layer.
      jAdjType= CalculateJoggleAdjustmentType(currentAngle, degToNextJoggle, degToPrevJoggle, jAdj);
      if (JA_RET_ADJ_ADV_NOM == jAdjType) { // Joggle region 1 case: Joggle ahead of angle by about a turn
        // Adjust retracting foot, advancing foot nominal
        isJoggleAdj= true; // set the flag
        logicTrace.append("Joggle in " + std::to_string(static_cast<long double>(degToNextJoggle)) + " degs. Ret Ft Nom + Adj " +
                          std::to_string(static_cast<long double>(jAdj)) + "mm, Adv Ft Nom, ");
        // not a transition adjustment case, set to 0
        tThisAdj = 0;
        }
      else if (JA_RET_FULL_ADV_NOP == jAdjType) { // Joggle region 2 case: Joggle near the current angle
        // Retract retreating foot fully, NOP the advancing foot
        isJoggleAdj= true; // set the flag
        // make degrees positive and display
        logicTrace.append("Past Joggle by " + std::to_string(static_cast<long double>(-1.0 * degToPrevJoggle)) + " degs. Ret Ft Full Retract, Adv Ft No Move, ");
        // not a transition adjustment case, set to 0
        tThisAdj = 0;
        }
      else if (JA_RET_NOP_ADV_NOM == jAdjType) { // Joggle region 3 case: Joggle past angle by about a turn
        // NOP the retreating good, Advancing foot nominal
        isJoggleAdj= true; // set the flag
        logicTrace.append("Past Joggle by "  + std::to_string(static_cast<long double>(-1.0 * degToPrevJoggle)) + " degs. Ret Ft No Move, Adv Ft Nom, ");
        // not a transition adjustment case, set to 0
        tThisAdj = 0;
        }
      else {  // no joggle adjusment needed
        isJoggleAdj= false; // clear the flag

        // Determine Transition window adjustment
        // Once a foot falls after a transition, but within the transition window
        // then the transition adjustment is the effect of this turn less the effect
        // of the previous accumulated adjustments until the foot was out of the window
        // on the previous turn or the layer ends.
        
        stat= coilMap_.isInTransitionLb(currentAngle, result, degToPrevTrans);
        if (stat && result) { // in a transition window
          // Mark the column as needing adjustment to keep track of this column
          // the the next time through (vs other columns no near the transition) and
          // then calculate transition adjustment
          // Adjust for this turn is the adjustment needed for this turn 
          // less the amount already adjusted: (thisAdj - accumAdj)
          // Adjustment grows, so this will be positive.
          // The first time through, the accumulated adjustment will be zero, so the adjustment
          // is the full amount calculated for this layer
          
          // mark this column as needing a transition adjustment
          SetTransAdjust(currentAngle);
          
          tThisAdj= CalculateTransitionAdjustment(currentAngle) - tAccumAdj;
          // accumulate the adjustment value
          tAccumAdj += tThisAdj;
          // set the flag
          inTransition= true;
          logicTrace.append("No Joggle Adj, Past Trans by "  + std::to_string(static_cast<long double>(degToPrevTrans)) + 
                            " degs. Adj: " + std::to_string(static_cast<long double>(tThisAdj)) + " mm, ");
          }
        else if (stat && !result && isTransAdjustSet(currentAngle) ) { // not in a transition window anymore, 
                                                                       // but the column was on the previous turn
          // The maximum adjustment case happened between the last turn and this one, so the
          // adjustment should the maximum possible adjustment (a nominal index) less the accumulated value
          // TODO: Verify this
          tThisAdj= TURN_INDEX_NOMINAL - tAccumAdj;
          // this was the last time to adjust for this transition, clear the accumulator and clear the trans adjust mark
          tAccumAdj= 0;
          ClearAllTransAdjust();
          
          // set the flag
          inTransition= true;
          logicTrace.append("No Joggle Adj, Now off trans. Adj: " + std::to_string(static_cast<long double>(tThisAdj)) + " mm, ");
          }
        else if (stat && !result && !isTransAdjustSet(currentAngle) ) { // not in a transition window anymore and the last turn was not either
          // nominal or back to nominal after a foot "drifted" outside the window
          tThisAdj= 0;
          // clear the flag
          inTransition= false;
          logicTrace.append("No Joggle Adj, No Trans Adj, ");
          }
        else {  // something wrong with lookup
          tThisAdj= 0;
          tAccumAdj= 0;
          // clear the flag
          inTransition= false;
          logicTrace.append("Error looking up IsInTransitionLb @ angle: " + std::to_string(static_cast<long long>(currentAngle)) + ". ");
          }
        }

      // Get layer number and even or odd layer
      // need to consider the joggle adjustment type because the layer number lookup
      // when the angle is at or near a joggle (Region 2, JA_RET_FULL_ADV_NOP) gives the layer number 1 too many
      // from what we want (and therefore isEven layer is wrong) if we just use the angle directly. This is because
      // the joggle case is the transition to a higher layer number, and we need the feet to index per the layer
      // we have been working on rather than the one it would get out of the coil map directly.
      layerNumber= coilMap_.GetLayerLb(currentAngle);
      if (NO_FEATURE != layerNumber && JA_RET_FULL_ADV_NOP == jAdjType) {
        // Layer number look up worked and in joggle region 2 (near a joggle).
        // Looked up layer number value is 1 too many. Subtract 1.
        --layerNumber;
        }
      else if (NO_FEATURE != layerNumber && JA_RET_FULL_ADV_NOP != jAdjType) {
        // Layer number look up worked and not in joggle region 2 (near a joggle).
        // Looked up layer number is okay. Nothing to do, block is here for documentation and completeness
        
        }
      else { // something wrong with the layer lookup
        logicTrace.append("Error looking up layer number @ angle: " + std::to_string(static_cast<long long>(currentAngle)) + ". ");
        // TODO: what in case of lookeup error
        }

      // determine even or odd layer based on layer number determined above
      // layer MOD 2 = 0 then even
      isEvenLayer = (0 == (layerNumber % 2) ? true : false);

      if (!isEvenLayer) {  // odd layer
        // set retreating and advancing Column turn numbers
        // turn number does not have to be adjusted for joggle region 2 
        // because the turn number does not change within or very near the joggle region
        advancingColumnTurn= coilMap_.GetTurnLb(currentAngle);
        retreatingColumnTurn= advancingColumnTurn + 1;  
        logicTrace.append("Odd Layer(" + std::to_string(static_cast<long long>(layerNumber)) + "), ");
        }
      else { // even layer
        // set retreating and advancing Column turn numbers
        advancingColumnTurn= coilMap_.GetTurnLb(currentAngle);
        retreatingColumnTurn= advancingColumnTurn - 1; 
        logicTrace.append("Even Layer(" + std::to_string(static_cast<long long>(layerNumber)) + "), ");
        }

      // Check for the last turn condition using the advacing foot turn number (the value is unchanged from the value in the coil map)
      isLastTurn= coilMap_.isLastTurnLb(advancingColumnTurn, isEvenLayer);
      if (isLastTurn) { // last turn
        logicTrace.append("Last Turn, ");
        }
      else {  // not the last turn
        logicTrace.append("Not Last Turn, ");
        }
        
      // check for last layer
      // last l does not have to be adjusted for joggle region 2 
      // because the turn number does not change within or very near the joggle region
      isLastLayer= coilMap_.isLastHqLayer(layerNumber);
      if (isLastLayer) {  // last layer
        logicTrace.append("LastLayer. ");
        }
      else {  // not the last layer
        logicTrace.append("NotLastLayer. ");
        } // check for last layer

      // Now the joggle adjustment, transition adjustment, last turn and last layer info in known.

      // Set the layer and hqp adjustments based on  the joggle and last turn/last layer cases.
      // The new hex and new layer case are handled elsewhere, so this is just for the non new hex/layer cases.
      // The hex and layer adjustment values needs to be -1 in the joggle case on the
      // last turn (layer adj) / last layer aned last turn (hqp adj) because we are near the joggle, so the coil map values have
      // incremented, but the foot corresponding to the scs position row is not yet on the new layer/hex.
      // layer adj
      if (isLastTurn && isJoggleAdj) {
        layerAdj= -1;
      }
      else {
        layerAdj= 0;
      }
      //hqp adj
      if (isLastTurn && isLastLayer && isJoggleAdj) {
        hqpAdj= -1;
      }
      else {
        hqpAdj= 0;
      }

      // Calculate advancing column position (if not the last layer)
      long riaAngle= static_cast<long>(round(POSITION_NOT_CALCULATED)); // sentinel values. Should not remain with this value
      double FootPosDist= POSITION_NOT_CALCULATED;
      if (!isLastLayer) {
        // not the last layer
        // Capture RIA angle for advancing feet
        riaAngle= currentAngle - static_cast<long>(round(ADV_FOOT_RIA_OFFSET_ANGLE));
        
        // Look at the last turn staus and the joggle adjustment type (determined above) to figure out what the advancing
        // foot should do
        // Advancing is a negative distance
        if ((!isLastTurn && JA_RET_NOM_ADV_NOM == jAdjType) || JA_RET_ADJ_ADV_NOM == jAdjType || JA_RET_NOP_ADV_NOM == jAdjType) {
          // (not last turn AND joggle region 0 (no joggle adjust nominal case)) OR joggle region 1 OR joggle region 3
          // The advancing foot is a nominal move (negative value)
          // nominal index + transition adjust
          FootPosDist= -1 * (TURN_INDEX_NOMINAL + tThisAdj);
          scsInsertMode= IM_ABS_UPDATE_SEL; // foot position distance is a relative distance to be added to previous position
          }
        else if (JA_RET_FULL_ADV_NOP == jAdjType) { // joggle region 2 -- will also be a last turn (dfn of joggle)
          // advancing foot should NOP
          FootPosDist= 0;
          scsInsertMode= IM_ABS_UPDATE_SEL; // foot position distance is a relative distance to be added to previous position
          }
        else if (isLastTurn) {  // last turn but not a joggle
          // Advance to full extend position
          FootPosDist= INITIAL_FULL_EXTEND_POS;
          scsInsertMode= IM_ABS_SEL; // foot position distance is an absolute position
          // leaving this layer, clear the transition adjustment accumulator and clear the trans adjust mark
          tAccumAdj= 0;
          ClearAllTransAdjust();
          }
        else { // error case - should never get here
          // insert a zero move and note it in the logic trace
          FootPosDist= 0;
          scsInsertMode= IM_ABS_UPDATE_SEL; // foot position distance is a relative distance to be added to previous position
          logicTrace.append("Logic error determining advancing foot move.");
          }

        // populate position detail structure
        // isNewHQP and isNewLayer cases are handled elsewhere, so these are forced to false here

        
        popDetailStatus= PopScsPosDetail(currentAngle, isEvenLayer, FOOT_ROLE_ADVANCING, scsInsertMode,
                                        FootPosDist, 0, logicTrace, inTransition, isJoggleAdj, false, false, 
                                        isLastTurn, isLastLayer, hqpAdj, layerAdj, scsPosDetail_);
                        
        // Append to the move detail trace what move is happening.
        //*MS is a token for 'Move Summary" that can be searched for to just get this part of the longer logic trace.
        // Do this after the position detail population so the displayed data is for 
        // sure what is in the position detail.
        // Look at the insert mode so that absolute moves can be indicated in the logic trace
        if (IM_ABS_SEL == scsInsertMode) {  // display an absolute move
          scsPosDetail_.get<4>().get<0>().append("*MS: Adv Ft To Trn: " + std::to_string(static_cast<long long>(advancingColumnTurn)) + ". " +
                                        "Adv (abs) " + IndexToString(scsPosDetail_.get<3>().get<1>()) + " to " +
                                        std::to_string(static_cast<long double>(-1 * scsPosDetail_.get<3>().get<0>())) + " mm."); // multiply by -1 to dispay a positive distance
          }
        else {  // display a relative move distance
          scsPosDetail_.get<4>().get<0>().append("*MS: Adv Ft To Trn: " + std::to_string(static_cast<long long>(advancingColumnTurn)) + ". " +
                                        "Adv (rel) " + IndexToString(scsPosDetail_.get<3>().get<1>()) + " " +
                                        std::to_string(static_cast<long double>(-1 * scsPosDetail_.get<3>().get<0>())) + " mm."); // multiply by -1 to dispay a positive distance
          }

        // create entry in SCS position map if the population was successful
        if (RTN_NO_ERROR == popDetailStatus)
          MapScsAxisMoves(riaAngle, scsPosDetail_);
        else {
          std::cout << std::endl << "** ERROR ** Populating advancing foot position at angle " << riaAngle << ". Row Not added." << std::endl;
          }

        }
      else {
        // the last layer
        // leave advancing feet all the way out
        // they are already all the way out, so no move to make
        
        if (isLastTurn) {  // last turn of the last layer
          // leaving this layer, clear the transition adjustment accumulator and clear the trans adjust mark
          // this covers the case where a column is on a transition when the HQP ends
          tAccumAdj= 0;
          ClearAllTransAdjust();
          }

        }

      // calculate retreating column position
      // Capture RIA angle for retreating feet
      riaAngle= currentAngle - static_cast<long>(round(RET_FOOT_RIA_OFFSET_ANGLE));

      // It not the last turn, then look at the joggle adjustment type (determined above) to figure out what the retreating 
      // foot position should be.
      // Retreating is a positive distance
      if (JA_RET_ADJ_ADV_NOM == jAdjType) { // joggle region 1
        // will also be a last turn (dfn of joggle)
        // Not the last turn and retreating foot is a joggle adjusted move
        // nominal index + transition adjust + joggle adjust
        FootPosDist= TURN_INDEX_NOMINAL + tThisAdj + jAdj;
        scsInsertMode= IM_ABS_UPDATE_SEL; // foot position distance is a relative distance to be added to previous position
        }
      else if (JA_RET_FULL_ADV_NOP == jAdjType) { // joggle region 2
        // will also be a last turn (dfn of joggle)
        // not the last turn and retreating foot should fully retract
        // Use an absolute move to full retract position. 
        // For either inside or outside feet, full retract is a position near max
        FootPosDist= INITIAL_FULL_RETRACT_POS;
        scsInsertMode= IM_ABS_SEL; // foot position distance is an absolute position
        }
      else if (JA_RET_NOP_ADV_NOM == jAdjType) { // region 3
        // will also be a last turn (dfn of joggle)
        // not on the last turn, and retreating foot should NOP
        FootPosDist= 0;
        scsInsertMode= IM_ABS_UPDATE_SEL; // foot position distance is a relative distance to be added to previous position
        }
      else if (!isLastTurn && JA_RET_NOM_ADV_NOM == jAdjType) { // region 0 -- nominal case, not a joggle adjust and not last turn
        // retreating foot is a nominal move
        FootPosDist= TURN_INDEX_NOMINAL + tThisAdj; 
        scsInsertMode= IM_ABS_UPDATE_SEL; // foot position distance is a relative distance to be added to previous position
        }
      else if (isLastTurn) {  // last turn but not a joggle
        FootPosDist= INITIAL_FULL_RETRACT_POS; // Retract fully
        scsInsertMode= IM_ABS_SEL; // foot position distance is an absolute position
        }
      else { // error case - should never get here
        // insert a zero move and note it in the logic trace
        FootPosDist= 0;
        scsInsertMode= IM_ABS_UPDATE_SEL; // foot position distance is a relative distance to be added to previous position
        logicTrace.append("Logic error determining retreating foot move.");
        }

      // populate position detail structure
      // isNewHQP and isNewLayer cases are handled elsewhere, so these are forced to false here
      popDetailStatus= PopScsPosDetail(currentAngle, isEvenLayer, FOOT_ROLE_RETREATING, scsInsertMode,
                                      FootPosDist, 0, logicTrace, inTransition, isJoggleAdj, false, false,
                                      isLastTurn, isLastLayer, hqpAdj, layerAdj, scsPosDetail_);
                      
      // Append to the move detail trace what move is happening.
      //*MS is a token for 'Move Summary" that can be searched for to just get this part of the longer logic trace.
      // Do this after the position detail population so the displayed data is for 
      // sure what is in the position detail.
      // Look at the insert mode so that absolute moves can be indicated in the logic trace
      if (IM_ABS_SEL == scsInsertMode) {  // display an absolute move
        scsPosDetail_.get<4>().get<0>().append("*MS: Ret Ft To Trn: " + std::to_string(static_cast<long long>(retreatingColumnTurn)) + ". " +
                                      "Ret (abs) " + IndexToString(scsPosDetail_.get<3>().get<1>()) + " to " +
                                      std::to_string(static_cast<long double>(scsPosDetail_.get<3>().get<0>())) + " mm.");
        }
        else {  // display a relative move distance
          scsPosDetail_.get<4>().get<0>().append("*MS: Ret Ft To Trn: " + std::to_string(static_cast<long long>(retreatingColumnTurn)) + ". " +
                                        "Ret (rel) " + IndexToString(scsPosDetail_.get<3>().get<1>()) + " " +
                                        std::to_string(static_cast<long double>(scsPosDetail_.get<3>().get<0>())) + " mm.");
        }
        
        // create entry in SCS position map if the population was successful
        if (RTN_NO_ERROR == popDetailStatus)
          MapScsAxisMoves(riaAngle, scsPosDetail_);
        else {
          std::cout << std::endl << "** ERROR ** Populating retreating foot position at angle " << riaAngle << ". Row Not added." << std::endl;
        }

      } // for currentAngle
    
    // end the line and add an extra line after the output in the for loop.
    std::cout << std::endl << std::endl;

    } // AxisPositions::CalculateAxisMoves()
    
  long AxisPositions::DbConnect() {
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
      // set return value to indicate an error
      rtnValue= RTN_ERROR;
      }
    return rtnValue;
    } // AxisPositions::DbConnect()

  // insert a row into the Cls db table at the Ria angle. Use values from the Cls Position Detail
  // assumes valid connection has been made
  // return value indicates success or error
  long AxisPositions::InsertIntoClsDb() {

    // variable to hold return value
    long rtnValue= 0;

    try {
      // Set the command text of the command object
      dbCommand_.setCommandText(SPNAME_CALC_CLS_POS.c_str(), SA_CmdStoredProc);

      // set the input parameters
        // (there are not any input parameters for this stored procedure)
        
      // execute the command
      dbCommand_.Execute();
      // set return value for all okay
      rtnValue= RTN_NO_ERROR;
      }
    catch(SAException &ex) {
      // get error message4
      errorText_= (const char*)ex.ErrText();
      std::cout << errorText_ << std::endl;
      // set return value to indicate an error
      rtnValue= RTN_ERROR;
      }
    return rtnValue;
    } // AxisPositions::InsertIntoClsDb(double angle, const CPosDetail &posDetail)

  // insert a row into the SCS db table at the Ria angle. Use values from the SCS Position Detail
  // assumes valid connection has been made
  // return value indicates success or error
  long AxisPositions::InsertIntoScsDb(double riaAngle, const SPosDetail &posDetail) {

    // variable to hold return value
    long rtnValue= 0;
    
    // Construct the move summary string from the logic trace.
    // It will populate the action description field below.
    msPos_= posDetail.get<4>().get<0>().find(MS_TOKEN); // look for the move summary token
    if (msPos_ != std::string::npos) {
      // move summary token was found. Ignore the first character, and put it in the move summary string.
      moveSum_= posDetail.get<4>().get<0>().substr(msPos_ + 1);
      }
    else {
      // move summary string not found. Make the move summary the entire logic trace.
      moveSum_= posDetail.get<4>().get<0>();
      }
    
    // Look at the isSelectedAxes flag (element 0 in the SelectedAxes vector),
      // in the posDetail to know which SQL procedure to call, 
      // and which parameters are used.
      // The other bits in the SelectedAxes vector are only used for a selected axis 
      // insert.
    
    if (posDetail.get<2>()[0]) {  // isSelectedAxes is true -- insert for selected axes only
      try {

        // Set the command text of the command object, and set mode dependent parameters
        // look at the absolute adjust flag to know which procedure to call
        if (posDetail.get<3>().get<2>()) {  // absolute adjust flag is true
          // this procedure always inserts an absolute row for the selected axes.
          dbCommand_.setCommandText(SPNAME_INSERT_SEL_ADJ_ABS_SCS_POS.c_str(), SA_CmdStoredProc);
          // the absolute flag is not used
          // set the distance parameter name and value
          dbCommand_.Param(SAP_ABS_ADJ_DIST_SEL_PARAM.c_str()).setAsDouble() = posDetail.get<3>().get<0>();    // distance from SelectedDetail tuple
          }
        else { // absolute adjust flag is false
          // this procedure inserts a relative or absolute row based on the parameter for the selected axes
          dbCommand_.setCommandText(SPNAME_INSERT_SEL_SCS_POS.c_str(), SA_CmdStoredProc);
          // set the isAbsolute parameter name and value
          dbCommand_.Param(SAP_ISABSOLUTE_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<1>();    // isAbsolute flag
          // set the distance/position parameter name
          dbCommand_.Param(SAP_POSDIST_SEL_PARAM.c_str()).setAsDouble() = posDetail.get<3>().get<0>(); // position / distance from SelectedDetail tuple
          }

          // construct the 
          // set the input parameters
          dbCommand_.Param(SAP_RIAANGLE_PARAM.c_str()).setAsDouble() = riaAngle;                      // ria angle
          dbCommand_.Param(SAP_COILANGLE_PARAM.c_str()).setAsDouble() = posDetail.get<4>().get<8>();  // coil angle
          dbCommand_.Param(SAP_LOGICTRACE_PARAM.c_str()).setAsString() = posDetail.get<4>().get<0>().c_str();  // logic trace
          dbCommand_.Param(SAP_ACTIONDESC_PARAM.c_str()).setAsString() = moveSum_.c_str();  // action description - part of the logic trace
          dbCommand_.Param(SAP_ISTRANSITION_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<2>(); // isInTransition
          dbCommand_.Param(SAP_ISJOGGLE_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<3>();     // isInJoggle
          dbCommand_.Param(SAP_ISNEWHQP_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<4>();     // isNewHqp
          dbCommand_.Param(SAP_ISNEWLAYER_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<5>();   // isNewLayer
          dbCommand_.Param(SAP_ISLASTTURN_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<6>();   // isLastTurn
          dbCommand_.Param(SAP_ISLASTLAYER_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<7>();   // isLastLayer
          dbCommand_.Param(SAP_HQPADJ_PARAM.c_str()).setAsLong() = posDetail.get<5>().get<0>();   // hqpAdjust
          dbCommand_.Param(SAP_LAYERADJ_PARAM.c_str()).setAsLong() = posDetail.get<5>().get<1>();   // layerAdjust
          dbCommand_.Param(SAP_FTAIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[1];          // axis selections
          dbCommand_.Param(SAP_FTAOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[2];
          dbCommand_.Param(SAP_FTBIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[3];
          dbCommand_.Param(SAP_FTBOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[4];
          dbCommand_.Param(SAP_FTCIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[5];
          dbCommand_.Param(SAP_FTCOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[6];
          dbCommand_.Param(SAP_FTDIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[7];
          dbCommand_.Param(SAP_FTDOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[8];
          dbCommand_.Param(SAP_FTEIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[9];
          dbCommand_.Param(SAP_FTEOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[10];
          dbCommand_.Param(SAP_FTFIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[11];
          dbCommand_.Param(SAP_FTFOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[12];
          dbCommand_.Param(SAP_COLAIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[13];
          dbCommand_.Param(SAP_COLAOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[14];
          dbCommand_.Param(SAP_COLBIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[15];
          dbCommand_.Param(SAP_COLBOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[16];
          dbCommand_.Param(SAP_COLCIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[17];
          dbCommand_.Param(SAP_COLCOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[18];
          dbCommand_.Param(SAP_COLDIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[19];
          dbCommand_.Param(SAP_COLDOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[20];
          dbCommand_.Param(SAP_COLEIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[21];
          dbCommand_.Param(SAP_COLEOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[22];
          dbCommand_.Param(SAP_COLFIN_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[23];
          dbCommand_.Param(SAP_COLFOUT_SEL_PARAM.c_str()).setAsBool() = posDetail.get<2>()[24];

          // execute the command
          dbCommand_.Execute();
          // set return value for all okay
          rtnValue= RTN_NO_ERROR;
          } // try block
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
    else { // isSelectedAxes is false -- insert a positon for all axes

      try {    
        // Set the command text of the command object
        dbCommand_.setCommandText(SPNAME_INSERT_ALL_SCS_POS.c_str(), SA_CmdStoredProc);

        // set the input parameters
        dbCommand_.Param(SAP_RIAANGLE_PARAM.c_str()).setAsDouble() = riaAngle;                      // ria Angle
        dbCommand_.Param(SAP_COILANGLE_PARAM.c_str()).setAsDouble() = posDetail.get<4>().get<8>();  // coil angle
        dbCommand_.Param(SAP_ISABSOLUTE_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<1>();   // isAbsolute flag
        dbCommand_.Param(SAP_LOGICTRACE_PARAM.c_str()).setAsString() = posDetail.get<4>().get<0>().c_str();  // logic trace
        dbCommand_.Param(SAP_ACTIONDESC_PARAM.c_str()).setAsString() = moveSum_.c_str();  // action description - part of the logic trace
        dbCommand_.Param(SAP_ISTRANSITION_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<2>(); // isInTransition
        dbCommand_.Param(SAP_ISJOGGLE_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<3>();     // isInJoggle
        dbCommand_.Param(SAP_ISNEWHQP_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<4>();     // isNewHqp
        dbCommand_.Param(SAP_ISNEWLAYER_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<5>();   // isNewLayer
        dbCommand_.Param(SAP_ISLASTTURN_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<6>();   // isLastTurn
        dbCommand_.Param(SAP_ISLASTLAYER_PARAM.c_str()).setAsBool() = posDetail.get<4>().get<7>();   // isLastLayer
        dbCommand_.Param(SAP_HQPADJ_PARAM.c_str()).setAsLong() = posDetail.get<5>().get<0>();   // hqpAdjust
        dbCommand_.Param(SAP_LAYERADJ_PARAM.c_str()).setAsLong() = posDetail.get<5>().get<1>();   // layerAdjust
        dbCommand_.Param(SAP_FTAIN_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[0];            // Distance position values
        dbCommand_.Param(SAP_FTAOUT_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[1];
        dbCommand_.Param(SAP_FTBIN_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[2];
        dbCommand_.Param(SAP_FTBOUT_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[3];
        dbCommand_.Param(SAP_FTCIN_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[4];
        dbCommand_.Param(SAP_FTCOUT_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[5];
        dbCommand_.Param(SAP_FTDIN_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[6];
        dbCommand_.Param(SAP_FTDOUT_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[7];
        dbCommand_.Param(SAP_FTEIN_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[8];
        dbCommand_.Param(SAP_FTEOUT_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[9];
        dbCommand_.Param(SAP_FTFIN_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[10];
        dbCommand_.Param(SAP_FTFOUT_PARAM.c_str()).setAsDouble() = posDetail.get<0>()[11];
        dbCommand_.Param(SAP_COLAIN_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[0];
        dbCommand_.Param(SAP_COLAOUT_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[1];
        dbCommand_.Param(SAP_COLBIN_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[2];
        dbCommand_.Param(SAP_COLBOUT_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[3];
        dbCommand_.Param(SAP_COLCIN_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[4];
        dbCommand_.Param(SAP_COLCOUT_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[5];
        dbCommand_.Param(SAP_COLDIN_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[6];
        dbCommand_.Param(SAP_COLDOUT_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[7];
        dbCommand_.Param(SAP_COLEIN_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[8];
        dbCommand_.Param(SAP_COLEOUT_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[9];
        dbCommand_.Param(SAP_COLFIN_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[10];
        dbCommand_.Param(SAP_COLFOUT_PARAM.c_str()).setAsDouble() = posDetail.get<1>()[11];

        // execute the command
        dbCommand_.Execute();
        // set return value for all okay
        rtnValue= RTN_NO_ERROR;
        } // try block
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
    } // AxisPositions::InsertIntoScsDb(double angle, const SPosDetail &posDetail)

  // Iterate thru Axis position map and insert a row for each map entry
  // assumes valid connection has been made
  long AxisPositions::InsertIntoScsDb() {
    // flag to keep track of an error occuring
    bool errorFlag = false;
    // variable to hold return value
    long rtnValue= 0;
    // variables to deal with displaying progress
    double records= static_cast<double>(scsAxisPositionMap_.size()); // number of records to insert -- use double so pctDone does not use interger math
    long count= 0;  // loop counter for display purposes
    double pctDone= 0;  // percent done

    std::cout << "There are " << static_cast<long>(records) << " to insert." << std::endl;

    for(sapm_const_iter mci = scsAxisPositionMap_.begin(); mci != scsAxisPositionMap_.end(); ++mci) {
      // display progress
      ++count;
      pctDone = static_cast<long>(100 * (count / records));
      std::cout << "On record " << count << " of " << records << " (" << pctDone << " %)\r" << std::flush;

      // do the insert
      rtnValue = InsertIntoScsDb(mci->first, mci->second);
      if (rtnValue != RTN_NO_ERROR)
        errorFlag = true;
      } // for loop

    // end the line and add an extra spece after the output in the for loop.
    std::cout << std::endl << std::endl;

    // check for an error
    if (!errorFlag) // no error flag, return ok
      return RTN_NO_ERROR;
    else  // error
      return RTN_ERROR;
    } // AxisPositions::InsertIntoScsDb()

  // delete all rows from CLS and SCS position tables
  // assumes valid connection has been made
  // return value indicates success or error
  long AxisPositions::DeleteAllPositions() {

    // variable to hold return value
    long rtnValue= 0;

    try {
      // Set the command text of the command object
      dbCommand_.setCommandText(SPNAME_DELETE_ALL_POS.c_str(), SA_CmdStoredProc);

      // there are no input parameters

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
    } // AxisPositions::DeleteAllPositions() 
    
    
  long AxisPositions::DbDisconnect() {
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
    } // AxisPositions::DbDisconnect()

} // namespace gaScsData
