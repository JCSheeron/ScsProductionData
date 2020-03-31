/********************************************************************
 * COPYRIGHT -- General Atomics
 ********************************************************************
 * Library: 
 * File: EventMap.hpp
 * Author: J. Sheeron (x2315)
 * Created: September 20, 2014
  *******************************************************************
 * Function:  
 *
 * Libraries used:
 *******************************************************************/
#pragma once

#ifndef GA_ScsData_EventMap_H_
#define GA_ScsData_EventMap_H_

// standard c/c++ libraries

// GA headers
#include "gaScsDataConstants.hpp"
#include "CoilMap.hpp"

namespace gaScsData {

class EventMap : private boost::noncopyable { 

public:
  // typedefs and enums

    // Event map definition
      // comprised of an angle key, and tuple holding the info for the event
      // The typle is comprised of the EventId (long), Logic Trace (string for debugging)
      typedef boost::tuple<long, std::string> EventDataTyp;
      typedef std::pair<double, EventDataTyp> EventValueTyp; // multimap value type -- key float value and the event data. Used for multimap insert
      // the event map is implemented as a multimap of <angle, eventData>. There may be one or more events at a given angle, so a multimap (instead of map) is used.
      typedef boost::container::multimap<double, EventDataTyp> EventMapTyp;
      // define iterators to allow iterating over the event map
      typedef EventMapTyp::iterator em_iter;
      typedef EventMapTyp::const_iterator em_const_iter;

      // Define a set to contain a list of angles.
      // Use a flat_set here becuase the size is fixed (set it in the ctor),
      // and to optimize lookups. Google research shows insert performance over map is better (surprisingly) under about 256 elements.
      // Each usage is expected to contain 40 or less elements
      typedef boost::container::flat_set<double> angleSetTyp;  // for best performance, reserve size in ctor
      typedef angleSetTyp::const_iterator as_cit;
 
      // The event ids need to match the event class table in the database.
      // This code does not generate instances of all these event ids.
      // In particular, dynamic events are not generated by this code.
      // User stop, alarm stop, burr detect, tape splice, and inspect fiducial are not generated for example.
      enum EventIds { EID_USER_STOP = 1000,
                      EID_ALARM_STOP = 1001,
                      EID_CAL_TOOL = 1002,
                      EID_INSTALL_COIL = 1003,
                      EID_LOAD_FIRST_HEX = 1004,
                      EID_INSTALL_RIA_AWH = 1005,
                      EID_INSPECT_BURRS = 1006,
                      EID_LAYER_INCREMENT = 1007,
                      EID_CONSOLIDATE_ODD = 1008, // Consolidate odd layer turns
                      EID_TEACH_FIDUCIAL = 1009,
                      EID_HQP_LOAD = 1010,
                      EID_TAPE_SPLICE1 = 1011,  // awh head 1
                      EID_TAPE_SPLICE2 = 1012,  // awh head 2
                      EID_TAPE_SPLICE3 = 1013,  // awh head 3
                      EID_INSPECT_FIDUCIAL_MARK = 1014,
                      EID_REMOVE_PLOW = 1015,
                      EID_HE_PIPE_INSULATION = 1016,
                      EID_END_ODD_LAYER = 1017,
                      EID_OPEN_LANDING_ROLLER = 1018, // avoid helium pipe
                      EID_HE_PIPE_MEASURE = 1024, // added event not in the original list of events
                      EID_END_EVEN_LAYER = 1019,
                      EID_LAYER_COMPRESSION = 1020,
                      EID_TURN_MEASUREMENT = 1021,
                      EID_MOVE_ECHAIN = 1022,  // Move FO E-chain and increment layer
                      EID_LONG_LEAD_ENDGAME = 1023,  // long lead end game and RIA disassembly
                      EID_MOVE_LR_TO_INNER_TURN_POS = 1025, // added event not in original list
                      EID_MOVE_LR_TO_OUTER_TURN_POS = 1026, // added event not in original list
                      EID_REMOVE_INNER_STRUTS = 1027}; // added event not in original list

     // constants

    // ctors and dtor
    EventMap();
    ~EventMap();

  // accessors

  // public methods
    long GenerateEventMapTable();

  private:
    // helper functions

    // event related functions
      // look at the coil map and see when an event should be created.
      // true - conditions are correct for making an event instance, false -- they are not.
      bool isEventLayerIncrement(CoilMap::cm_cit cit) const;
      bool isEventEndEvenLayer(CoilMap::cm_cit cit) const;
      bool isEventConsolidateOdd(CoilMap::cm_cit cit) const;
      bool isEventHqpLoad(CoilMap::cm_cit cit) const;
      bool isEventTeachFiducial(CoilMap::cm_cit cit) const;
      bool isEventRemovePlow(CoilMap::cm_cit cit) const;
      bool isEventHePipeInsulation(CoilMap::cm_cit cit) const;
      bool isEventHePipeMeasure(CoilMap::cm_cit cit) const;
      bool isEventOpenLandingRoller(CoilMap::cm_cit cit) const;
      bool isEventEndOddLayer(CoilMap::cm_cit cit) const;
      bool isEventLayerCompression(CoilMap::cm_cit cit) const;
      bool isEventTurnMeasurement(CoilMap::cm_cit cit) const;
      bool isEventMoveEChain(CoilMap::cm_cit cit) const;
      bool isEventRemoveInnerStruts(CoilMap::cm_cit cit) const;
      bool isEventLeadEndgame(CoilMap::cm_cit cit) const;
      bool isEventMoveLrInnerTurnPos(CoilMap::cm_cit cit) const;
      bool isEventMoveLrToOuterTurnPos(CoilMap::cm_cit cit) const;

      // add the specified event ID to the event map at the specified angle
      void AddEventToMap(double angle, long eventId, std::string logicTrace = "");
      // iterates over the operational range of angles and determies which events happen at which angles
      void MapEventInstances();

      long DbConnect();
      long DbDisconnect();
      // executes the specified stored procedure, which is expected to return results (SELECT...) 
      long QueryDb(const std::string &sprocName);

      // Get hqp and layer start angles. 
      // These functions rely on the result set being in the command member object
      // as a result of  the QueryDb call (do this call before calling these functions.)
      long PopulateHqpStartSet();
      long PopulateLayerStartSet();

      // Insert a row at the angle. Use the passed in event id.
      // Execute the specified stored procedure to do the insert
      // It is assumed the connection is already done, and that sometime after the call a disconnect is performed.
      long InsertIntoDb(double angle, long eventId, const std::string &sprocName, const std::string& trace = "none");
      // Iterate thru event map and insert a row for each map entry.
      long InsertIntoDb(const std::string &sprocName); 

      // delete all rows from event list table
      // that are not referenced in the event history or
      // action history tables (i.e. are not done or partially done)
      // assumes valid connection has been made
      // return value indicates success or error
      long DeleteAllUndoneEvents();

    // member variables

       // coil map
      CoilMap coilMap_;

      // angle set of the hqp start angles (from the Scs Positon Table)
      angleSetTyp hqpStartSet_;

      // angle set of the layer start angles (from the Scs Positon Table)
      angleSetTyp layerStartSet_;

        // Event map
      EventMapTyp eventMap_;

      // Event value type
      EventValueTyp eventValue_;

      // specify server and db string
      std::string serverText_;
      // error text
      std::string errorText_;
      
      // db objects
      SAConnection dbConnection_; // create connection object
      SACommand dbCommand_; // create command object
};

} //namespace gaScsData
#endif // GA_ScsData_EventMap_H_

