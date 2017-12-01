/********************************************************************
 * COPYRIGHT -- General Atomics
 ********************************************************************
 * Library: 
 * File: AxisPositions.hpp
 * Author: J. Sheeron (x2315)
 * Created: November 10th, 2014
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
#pragma once

#ifndef GA_AxesPositions_H_
#define GA_AxesPositions_H_

// standard c/c++ libraries

// GA headers
#include "gaScsDataConstants.hpp"
#include "CoilMap.hpp"


namespace gaScsData {

class AxisPositions : private boost::noncopyable {  

public:
  // typedefs and enums

  // The PosAttributes tuple holds additional "meta data" type info about a move. It is defined to reduce the
      // number of tuple elements in Scs PosDetail tuple definitions, as otherwise we exceed the 10 element maximum configured
      // in the boost library.
      // The members are:(0) logic trace string (diag/troubleshooting), (1) isAbsolute flag, (2) the transition flag,
      //                 (3) the joggle adjust flag, (4) the isNewHqp flag, (5) the isNewLayer flag, (6) the isLastTurn flag
      //                 (7) the isLastLayer flag, (8) coil angle
    typedef boost::tuple<std::string, bool, bool, bool, bool, bool, bool, bool, double> PosAttributes;

    // The HqpLayerAdj tuple holds a value that the Hqp value or the layer value from the coil map should be adjusted by.
      // This is necessary becasue the hqp and layer number is stored in the coil map, but the start of the hpq or layer
      // relative to the joggle requires the layer and/or hqp value to be adjusted up or down, or not at all.
      // The members are: (0) hqp adj (0 -- normal no adjust, -1, +1)
      //                  (1) layer adj (0 -- normal no adjust, -1, +1)
    typedef boost::tuple<long, long> HqpLayerAdj;

    // Scs Axis position map
      // comprised of a map angle key, and tuple holding the axis positions.
      // The tuple is comprised of two 12 element vectors, one vector for the foot positions
      // and one vector for the column positions.  Positions are generally absolute, but may
      // be relative. The isAbsolute flag (usually true) is used to denote absolute positions 
      // from relative distances
      // the vector positions are assigned as follows:
      //  0 -- A Inner
      //  1 -- A Outer
      //  2 -- B Inner
      //  3 -- B Outer
      //  4 -- C Inner
      //  5 -- C Outer
      //  6 -- D Inner
      //  7 -- D Outer
      //  8 -- E Inner
      //  9 -- E Outer
      //  10 -- F Inner
      //  11 -- F Outer
      //
      // The selected axes vector controls if positions are being specified for every axis, or only for selected axes.  This, in turn, determines
        // which SQL stored procedure gets called in order to insert new data in the table.  The selected axes vector is assigned like the the
        // index enumeration above, with positon 0 being used as a flag indication that an axis is being selected. True = insert a positon
        // using the indiecated selected axes, and use the selected axes SQL insert procedure. False = A positon for every axis is specified,
        // and use the non selected axes SQL insert procedure. The enumeration is as follows:
        // Foot In then Out, A thru F, then the same pattern for columns:
        // 0 the isSelectedAxes flag -- determines how to insert value into the table and if the other elements of this vector are to be used
        // 1 Foot A In
        // 2 Foot A Out
        // 3 Foot B In
        // ...
        // 12 Foot F Out
        // 13 Col A In
        //  ...
        // 24 Col F Out

      // The Selected Detail tuple holds the distance and axis index for a selected axis move. The index is used for diagnostics and 
        // troubleshooting.
      typedef std::vector<double> Positions;
      typedef std::vector<bool> SelectedAxes;
      // The SelectedDetail holds info about Selected Insert modes. The first two elements (distance and selected index) are used
        // to be able to display useful info in the logic trace.  The third element (adjust absolute flag) is used in conjunciton with 
        // Position Detail(2).0 to determine which sql procedure to call.
        // If PositionDetail(2).0 is true (selected mode) and SelectedDetail(2) is true
        // use SPNAME_INSERT_SEL_ADJ_ABS_SCS_POS sql procedure name.  If same case, but SelectedDetail(2) is false,
        // use SPNAME_INSERT_SEL_SCS_POS sql procedure name.
        typedef boost::tuple<double, AxisIndexes, bool> SelectedDetail; // (0) distance, (1) selected axis index, (2) adjust absolute flag
      
      // foot and column position detail tuple: (0)foot position vector, (1)column position vector,
        //                                      (2) selected Axes vector, (3) Selected detail (distance and index)
        //                                      (4) PosAttributes, (5) HqpLayerAdj
        
        // The coil angle is used to reference the resulting sql table back to the coil map. This
          // gives access to layer number, even layer, and everything else in the coil map without needing to write that data to the 
          // position map.
      typedef boost::tuple<Positions, Positions, SelectedAxes, SelectedDetail, PosAttributes, HqpLayerAdj> SPosDetail; // Scs Position Detail
        
      // the position map is implemented as a map of <angle, FCPositions>
      typedef boost::container::map<long, SPosDetail> ScsAxesPositionMap;
      // define iterators to allow iterating over the Axes Positions map
      typedef ScsAxesPositionMap::iterator sapm_iter;
      typedef ScsAxesPositionMap::const_iterator sapm_const_iter;      
      
      // Define a (angle, isEven) pair
      typedef std::pair<double,bool> angleIsEven_pair;

      enum footRole { FOOT_ROLE_ADVANCING=1, FOOT_ROLE_RETREATING };
      
      enum insertMode {IM_REL_SEL,  // use Insert Select Pos Dist SQL procedure
                      IM_ABS_SEL,   // use Insert Select Pos Dist SQL procedure
                      IM_ABS_UPDATE_SEL,  // use Insert Select Pos From Previous SQL procedure
                      IM_REL_ALL,   // Use Insert Pos Dist SQL procedure
                      IM_ABS_ALL};  // Use Insert Pos Dist SQL procedure

      // Types of joggle adjusments needed for foot moves affected by joggles
      enum joggleAdjustmentType {
        JA_RET_ADJ_ADV_NOM,       // Region 1, Retreating foot add an adjustment, Advancing foot nominal move
        JA_RET_FULL_ADV_NOP,      // Region 2, Retreating foot to full retracted, Advancing foot do nothing
        JA_RET_NOP_ADV_NOM,       // Region 3, Retreating foot do nothing, Advancing foot nominal move
        JA_RET_NOM_ADV_NOM };     // Nominal, no joggle adjust, nominal case Retreating foot nominal, Advancing foot nominal (no joggle adjustment case)

    // constants

    // ctors and dtor
    AxisPositions();
    ~AxisPositions();

  // accessors

  // public methods
    // Connects to the Db, retrieves the coil map and populates the member data structure
    // return value indicates success or error
    long GenerateCoilMap();

    // 1) Iterate thru azimuth positions, and using the coil map, calculate the CLS move distances and SCS positions.
    // 2) Add them to the SCS position map. 
    // 3) Delete all the existing (old) rows from the CLS and SCS position tables.
    // This also resets the identity index so rows will start at id = 1
    // 4) Iterate thru the SCS map, and insert the positions in the SCS position map into the SCS position table in the DB
    // 5) Build the db CLS positon table from the SCS position table. 
    // Return value indicates success or error
    long GeneratePositionTables();

    // convert an axis index to a string
    std::string IndexToString(long index) const;
    std::string IndexToString(gaScsData::AxisIndexes index) const;

  private:
    // helper functions
      // Calculate the transition adjustment (return value)
      // This is dependent on where in the transition window the passed in angle falls,
      // and on odd or even layer. The transition past the start (transAngleChange_) could have been passed
      // in as a parameter, but it was not to make this function more stand alone.
      double CalculateTransitionAdjustment(double angle); // modifies member variables
      
      // Given an angle, the corresponding foot pair is marked as having been adjusted 
      // for a transition (via the member boolean array)
      // return value indicates true on success or false on bad angle
      bool SetTransAdjust(double angle); // modified member variables
      // Clear all trans adjust marks
      void ClearAllTransAdjust(); // modified member variables
      // Is the trans adjust mark set
      bool isTransAdjustSet(double angle) const ;

      // Look at the next joggle, and return what type of adjustment is needed due to the joggle
      // and its locaiton relative to the current angle (column azimuth) 
      // Pass back by reference the distance to the joggle (in degrees) and the joggle adjustment
      joggleAdjustmentType CalculateJoggleAdjustmentType(double angle, double &degToNextJoggle, double &degToPrevJoggle, double &jAdj) const;

      // Calculate the Ria Angle where the new layer positions row should go
      // and determine if the angle marks the beginning of an odd or even layer.
      // The joggle angle, which is assumed to be one near the current angle,
      // is used to determine if the new layer is even or odd.
      angleIsEven_pair CalculateNewLayerRiaAngle(double coilAngle, double joggleAngle) const;
      
      // populates the SCS axis move position detail structure passed in as a reference.
      // Look at the passed in mode parameter to determine which SQL procedure needs to be called make the call as needed.
      // If insert mode is INSERT_MODE_ALL, then move the inside axes to pos 1 and outside axes to pos 2.
      // If insert mode is INSERT_MODE_SELECTED, then move the selected axes to pos 1, and pos 2 is unused.
        // Used the passed in coil angle, the odd/even layer, and the role to determine the prober axis to apply the distance/position value to
        // It is expected that the passed in angle will fall on a column azimuth, and not in between columns.
      //
      // return value indicates success or error
      long PopScsPosDetail(double coilAngle, bool isEven, footRole role, insertMode mode,
                           double fDistPos1, double fDistPos2, const std::string &trace, 
                           bool inTransition, bool inJoggle, bool isNewHqp, bool isNewLayer, 
                           bool isLastTurn, bool isLastLayer, long hqpAdj, long layerAdj, 
                           SPosDetail &posDetail);
                           
      // Set the selection bit for the specfied axis and set the distance in element 0 of 
      // the foot axis positions vector.
      void SetSelectedScsAxis(AxisIndexes index, double distPos, SPosDetail &posDetail, insertMode mode);
      void SetSelectedScsAxis(long index, double distPos, SPosDetail &posDetail, insertMode mode);

      // Clear the selection bit for the specified axis.
      void UnselectSelectedScsAxis(AxisIndexes index, SPosDetail &posDetail);
      void UnselectSelectedScsAxis(long index, SPosDetail &posDetail);

      // Clear the selection bit for all axes
      void UnselectAllScsAxes(SPosDetail &posDetail);

      // Map the specified angle to the position detail in the SCS Axis Position Map
      void MapScsAxisMoves(long angle, const SPosDetail &posDetail);
      void MapScsAxisMoves(double angle, const SPosDetail &posDetail);

      // Enter the post load mode foot positions into the postion map at the specified angle.
        // Used to "seed" the starting point for relative adjustments to absolute positons later on.
      // isInJoggleWindow is used to signal that start positions need to be adjusted because the column corresponding to 
      // the coil angle is within a joggle window and it needs to start retreated by 1 turn
      // Return value is error status
      long MapPostLoadScsPositions(double angle, bool isInJoggleWindow);

      // After the post load SCS positions, at the beginning of a coil, the F Inner/Outer pair need to move to
        // release the tail so the lead can be lowered. F Inner is moved by the transition adjustment amount, and then
        // F Outer follows suit. From then on, moves and transition adjustment is calculated normally.  
      // Return via reference the transition adjustment value
      // Return value is error status
      long MapCoilStartScsPositions(double socPostLoadAngle, double socInitRetrAngle, double socInitAdvAngle, double &tAdj);

      // Enter the new layer foot positions into the postion map at the specified angle.
        // Used to "seed" the starting point for relative adjustments to absolute positons later on.
        // isLastTurn and isInJoggleWindow are used to signal that start positions need to be adjusted because the column corresponding to 
        // the coil angle is within a joggle window and the foot start positions need to be adjusted.
        // Return value is error status
      long MapNewLayerScsPositions(double riaAngle, double coilAngle, bool isEven, bool isLastLayer, bool isInJoggleWindow, bool isNewHqp);

      // This function is the *main workhorse* of the class for creating the Cls and Scs position maps.
      // It uses the other helper functions to make an entry in the Cls position map and the Scs postion map for
      // each foot/column pair (in/out) azimuth
      void CalculateAxisMoves();
      
      // connect to the db
      long DbConnect();
      
       // insert a row into the SCS db table at the Ria angle. Use values from the SCS Position Detail
      // assumes valid connection has been made
      // return value indicates success or error
      long InsertIntoScsDb(double riaAngle, const SPosDetail& posDetail);
      // Iterate thru Axis position map and insert a row for each map entry using the overload.
      long InsertIntoScsDb(); 

      // Create Cls moves from the Scs Position table, and populate the Cls position table.
      long InsertIntoClsDb(); 
     
      // delete all rows from CLS and SCS position tables
      // assumes valid connection has been made
      // return value indicates success or error
      long DeleteAllPositions();
      
      // disconnect from the db
      long DbDisconnect();

    // member variables

      // coil map
      CoilMap coilMap_;

      // vector of axis enumerations to allow iterating over
      std::vector<AxisIndexes> vAxisIndexes_;

      // Scs axis position details
      SPosDetail scsPosDetail_;
      // Scs Axis Position Map
      ScsAxesPositionMap scsAxisPositionMap_;

      // specify server and db string
      std::string serverText_;
      // command text
      std::string commandText_;
      // error text
      std::string errorText_;
      
      // db objects
      SAConnection dbConnection_; // create connection object
      SACommand dbCommand_; // create connection object
      
      // Member variables for calculating transition adjustments.
      std::vector<bool> arrAdjMark_;
      
      // The following could have been function scope, but used member variables
      // instead so they don't have to be constructed and 
      // destructed for each calculation
      double transR1_; // turn radius 1, smaller radius of a transition
      double transR2_; // turn radius 2, larger radius of a transition
      double transAngleFromStart_; // degrees into transition
      double transAngleChange_; // angle where transitions transitions between an arc and a straight section
      double transRArc_;  // radius of sharp arc (r2 - Ro)
      double transR_; // calculated radius within the transition
      bool transResult_, transStat_;  // result and status of a boolean check
      
      // similarly, I don't want to make these for each row insertion
      std::size_t msPos_; // move summary position with in the logic trace
      std::string moveSum_; // move summary string portion of the logic trace

};

} //namespace gaCoilMap
#endif // GA_AxesPositions_H_

