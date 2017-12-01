/********************************************************************
 * COPYRIGHT -- General Atomics
 ********************************************************************
 * Library: 
 * File: CoilMap.hpp
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
#pragma once

#ifndef GA_CoilMap_H_
#define GA_CoilMap_H_


// standard c/c++ libraries

// GA headers

#include "gaScsDataConstants.hpp"

namespace gaScsData {

class CoilMap : private boost::noncopyable { 

public:
  // typedefs and enums

    // tuple to hold the feature code (0 string), hex/quad number (1 long), layer (2 long), turn (3 long), azimuth (4 double), nominal radius (5 double)
      typedef boost::tuple<std::string, long, long, long, double, double> fhltar;

     // coil map tuble and map structure
      // same structure as defined above
      typedef fhltar angle_properties;
        
      // the coil map is implemented as a map of <angle, angle_properties>
      typedef boost::container::map<double, angle_properties> coil_map;
      // define iterators to allow iterating over the var_map
      typedef coil_map::const_iterator cm_cit;
      typedef coil_map::const_reverse_iterator cm_crit;

    // pair to hold an angle pair
      typedef std::pair<double, double> angle_pair;
    // pair to hold a feature code pair
      typedef std::pair<std::string, std::string> fc_pair;
    // pair to hold a layer number pair
      typedef std::pair<long, long> layer_pair;

  // Define a map to contain a list of layers (key) and coil angles, but only including odd layer turn 14 Transitions.
    // another map was used here instead of a brute force search of the full map looking up layer numbers, turn numbers for each angle.
    // This map allows a lookup of the angle directly from a given layer number.
    // This map is used to get the ending angle for the odd layer consolidation events
    // the coil map is implemented as a map of <long, angle>
    // Use a flat_map here becuase the size is fixed (set it in the ctor to num of add layers),
    // and to optimize lookups. Google research shows insert performance over map is better (surprisingly) under about 256 elements.
      typedef boost::container::flat_map<long, double> layerAngle_map;  // for best performance, reserve size in ctor
      typedef layerAngle_map::const_iterator lam_cit;

  // Define a set to hold the layer numbers corresponding when to perform a layer compression and measurement
  // Use a flat_set here becuase the size is fixed (set it in the ctor to num of add layers),
  // and to optimize lookups. Google research shows insert performance over map is better (surprisingly) under about 256 elements.
      typedef boost::container::flat_set<long> layer_container;  // for best performance, reserve size in ctor
      typedef layer_container::const_iterator lc_cit;
      
  // Define a set to contain a list of angles.
    typedef boost::container::flat_set<double> angle_set;  // for best performance, reserve size in ctor
    typedef angle_set::const_iterator as_cit;

  // class constants

  // ctors and dtor
    CoilMap();

    ~CoilMap();

  // public coil map -- it would be better if this were private
  coil_map mapCoil_;
  
  // public member functions
  long PopulateCoilMap();

  // accessors
    std::string GetErrorText() const;
   
    // these accessor functions get property for the row with the specified angle, or 
    // the previous or equal to the passed in angle (i.e. "not after", or the Lower Bound, Hence the Lb ending), 
    // or the row after the passed in angle (i.e. "not before" or the Upper Bound, hence the Ub ending).
    // BUG: LB logic is erronous in the case where the passed in angle matches the last angle (row) in the map.
      // in this case, the LB functions behave as if there is no LB, but really this last row should count
    // fhltar structure is the feature code, hex/quad number, layer, turn, azimuth, nominal radius
    CoilMap::fhltar GetFhltar(double angle) const;
    CoilMap::fhltar GetFhltarLb(double angle) const;

    double GetAngle(double angle) const; // acts as a way to verify if angle is in the map. If it isn't return value will be a sentinel
    // Get the previous angle given an angle.
    // Different than Lb -- if the passed in angle matches an angle, then the previous angle is returned (not the angle as the LB case does)
    // if there is an angle in the map smaller than the passed in angle, the angle of the smaller entry is returned (like the LB case)
    double GetPrevAngle(double angle) const;
    double GetAngleLb(double angle) const;
    // Get angle after the passed in angle. (i.e. Upper bound, hence the Ub ending)
    double GetAngleUb(double angle) const;
    
    std::string GetFc(double angle) const;
    std::string GetFcLb(double angle) const;
    
    long GetHexQuadNumber(double angle) const;
    long GetHexQuadNumberLb(double angle) const;
    
    long GetLayer(double angle) const;
    long GetLayerLb(double angle) const;
    
    long GetTurn(double angle) const;
    long GetTurnLb(double angle) const;
    
    double GetAzimuth(double angle) const;
    double GetAzimuthLb(double angle) const;
    
    double GetRadius(double angle) const;
    double GetRadiusLb(double angle) const;
    
    double GetJoggleLb(double angle) const;
    // Given an angle, return the joggle window
    double GetJoggleLengthLb(double angle) const;
    // Get Joggle Angle of the joggle at or past the passed in angle (i.e. Upper bound, hence the Ub ending)
      // NOTE: Returns angle AT or past
    double GetJoggleUb(double angle) const;
    
    // Get the hqp start angle, lower bound
    double GetHqpStartLb(double angle) const;

    bool isExisting(double angle) const; // does angle exist 

    // Get data from the coil map using iterators
    CoilMap::fhltar GetFhltar(cm_cit cit) const;
    double GetAngle(cm_cit cit) const;
    double GetAngle(cm_crit crit) const;
    std::string GetFc(cm_cit cit) const; // Feature Code (Fc)
    long GetHexQuadNumber(cm_cit cit) const;
    long GetLayer(cm_cit cit) const;
    long GetTurn(cm_cit cit) const;
    double GetAzimuth(cm_cit cit) const;
    double GetRadius(cm_cit cit) const;
    double GetAngleOl14T(lam_cit cit) const;
    bool isEvenLayer(cm_cit cit, bool &condition) const;
    bool isOddLayer(cm_cit cit, bool &condition) const;
    bool isLastHqLayer(cm_cit cit, bool &condition) const;
    // get the current and next angle.
    // Return them as a pair (current angle, next angle)
    CoilMap::angle_pair GetCurrentNextAngle(cm_cit cit) const;
    // get the current and next feature code.
    // Return them as a pair (current FC, next FC)
    CoilMap::fc_pair GetCurrentNextFc(cm_cit cit) const;
    // get the current layer and the next layer.
    // Return them as a pair (current layer, next layer)
    CoilMap::layer_pair GetCurrentNextLayer(cm_cit cit) const;
    
    // Determine if the angle (column) is the last move (retreat) of the layer, 
      // and return by reference the boolean condiiton, the joggle angle, and if the move is within the joggle window.
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
    bool CoilMap::isLastMoveOfLayer(double angle, bool &condition, double &joggleAngle, bool &isInJoggleWindow) const;

    // was a local zero just passed
    bool isLocalZeroLb(double angle, bool &condition) const;  // are we just after a local zero -- in other words, did we just start a new hqp?
    
    // Even/odd layer 
    bool isEvenLayer(double angle, bool &condition) const;
    bool isEvenLayerLb(double angle, bool &condition) const;

    bool isOddLayer(double angle, bool &condition) const;
    bool isOddLayerLb(double angle, bool &condition) const;

    // determine if a angle corresponds to a transition window Lower bound
      // if previous angle was a transition AND
      // foot angle - angle of previous transition <= the transition window (in the window)
      // then the passed in angle is one where a transition adjustment needs to happen
    bool isInTransitionLb(double angle, bool &condition, double &degtoPrevTrans) const;
    
    // determine if a angle corresponds to (is within) a joggle window Lower bound
    bool isInJoggleLb(double angle, bool &condition) const;
    
    // Determine if a turn corresponds to the last turn depending on the turn number and even/odd layer bool
    // NOTE: Unlike other boolean tests, return value indicates test result (not status)
    bool isLastTurnLb(long turn, bool isEvenLayer) const;
    bool isLastTurnLb(double angle, bool &condition) const;
    
    // determine if the angle or layer corresponds to the last hex/quad layer
      // Last layer of a hex/quad is when layer
      // is 6, 12, 18, 22, 28, 34, or 40
      // return value true or false to indicate ok or error (except in long override)
      // condition indicates last layer (true) or not the last layer (false)
    // NOTE: Unlike other boolean tests, return value indicates test result (not status)
    bool isLastHqLayer(long layer) const;
    bool isLastHqLayer(double angle, bool &condition) const;
    bool isLastHqLayerLb(double angle, bool &condition) const;
    // looks for a value in the list of measurement and compression layers and returns true if the value was found
    bool isInLaMeCo(long layer) const;

    // get the current and next angle.
    // Return them as a pair (current angle, next angle)
    CoilMap::angle_pair GetCurrentNextAngle(double angle) const;
    CoilMap::angle_pair GetCurrentNextAngleLb(double angle) const;

    // get the current and next feature code.
    // Return them as a pair (current FC, next FC)
    CoilMap::fc_pair GetCurrentNextFc(double angle) const;
    CoilMap::fc_pair GetCurrentNextFcLb(double angle) const;  // Lower bound version

    // get the current layer and the next layer.
    // Return them as a pair (current layer, next layer)
    CoilMap::layer_pair GetCurrentNextLayer(double angle) const;
    CoilMap::layer_pair GetCurrentNextLayerLb(double angle) const;
    
    // get angle given a turn from the layerAngle map of Odd Layer Turn 14 Transition Layers
    double GetAngleOl14T(long turn) const;

  private:
    // helper functions
      // database functions
      long DbConnect();
      long DbDisconnect();
      long QueryDb(const std::string &sprocName); // executes the specified stored procedure, which is expected to return results (SELECT...) 
      long MapFeature();
      long MapCmOlT14FcT(); // maps Coil map rows of Turn 14 odd layers with feature codes of "T". Used for odd layer consolidation.
      long PopulateJoggleAngleSet();   // Populates a set of joggle angles sorted ascendingly
      
    // member variables
      // layer angle map1
      layerAngle_map mapOl14T_;  // Holds the layer (key) and angle of odd layer, turn 14 transitions
      // Set of joggle angles. Given and angle, this set allows the lookup of the angle of the next or previous joggle.
      angle_set setJoggleAngles_;
      // list of layer numbers when coil measurement and compression are needed
      layer_container laMeCo_;

      // db objects
      SAConnection dbConnection_; // create connection object
      SACommand dbCommand_; // create connection object

      // specify server and db string
      std::string serverText_;
      // error text
      std::string errorText_;

};

} // namespace gaScsData
#endif // GA_CoilMap_H_


