#pragma once

#ifndef GA_ScsDataConstants_H_
#define GA_ScsDataConstants_H_

namespace gaScsData {

// namespace constants

// return values
  const long RTN_NO_ERROR= 1;
  const long RTN_ERROR= -1;
  const long RTN_NO_RESULTS= -2;

// axis index enumeration
  // WARNING: these correspond to vector indexes, so they must start at 0 and use sequential values.
  // Some routines may use this enumeration to iterate through a vector so be careful about changing it
  enum AxisIndexes {
    AXIDX_UNKNOWN= 0,
    AXIDX_A_FT_IN= 1,
    AXIDX_A_FT_OUT,
    AXIDX_B_FT_IN,
    AXIDX_B_FT_OUT,
    AXIDX_C_FT_IN,
    AXIDX_C_FT_OUT,
    AXIDX_D_FT_IN,
    AXIDX_D_FT_OUT,
    AXIDX_E_FT_IN,
    AXIDX_E_FT_OUT,
    AXIDX_F_FT_IN,
    AXIDX_F_FT_OUT,
    AXIDX_A_COL_IN,
    AXIDX_A_COL_OUT,
    AXIDX_B_COL_IN,
    AXIDX_B_COL_OUT,
    AXIDX_C_COL_IN,
    AXIDX_C_COL_OUT,
    AXIDX_D_COL_IN,
    AXIDX_D_COL_OUT,
    AXIDX_E_COL_IN,
    AXIDX_E_COL_OUT,
    AXIDX_F_COL_IN,
    AXIDX_F_COL_OUT };
  

// number of columns
  const long COLUMN_COUNT= 12;

// column azimuth angles
  const long A_COLUMN_AZIMUTH= 30;
  const long B_COLUMN_AZIMUTH= 90;
  const long C_COLUMN_AZIMUTH= 150;
  const long D_COLUMN_AZIMUTH= 210;
  const long E_COLUMN_AZIMUTH= 270;
  const long F_COLUMN_AZIMUTH= 330;

// position sentinel values
  // initial non-position sentinel value
  const double INITIAL_NO_POSITION= -20000.0;
  // not calculated position sentinel value
  const double POSITION_NOT_CALCULATED= -10000.0;
  // not a valid feature location sentinel value
  const long NO_FEATURE= -1;
  const std::string NO_FEATURE_STR= "none";
  
// coil and RIA features mostly used for position calculation
  const double PI= 3.14159;
  const double DEG_TO_RADIANS= PI/180.0;  // convert between degrees and radians for trig functions
  const double RADIANS_TO_DEG= 180.0/PI;
  
  const long TURNS_PER_LAYER= 14;
  const long LAYERS_PER_COIL= 40;
//  const long COIL_ANGLE_MAX= 79560; // mockup
  const long COIL_ANGLE_MAX= (LAYERS_PER_COIL * TURNS_PER_LAYER * 360) - (360 * 6);	// actual coil length is 6 turns less than a full 14x40 coil
  const long COLUMN_INCREMENT= 60;		// degrees. 6 fold symmetry
  const long INITIAL_COLUMN_ANGLE= 30;	// degrees - start at column A
  const double TURN_INDEX_NOMINAL= 53; // mm, nominal turn to turn index

  const double INITIAL_FULL_RETRACT_POS= 735.0; //mm. nominal all the way retracted position (past the 14th turn)
  const double INITIAL_FULL_EXTEND_POS= -13.0; //mm. nominal all the way extended position - some minimal value
  const double RETREATING_FOOT_START_POS= -13.0; // mm. nominal starting position (new layer or new hqp (post load mode))
  const double ADVANCING_FOOT_START_POS= 729.0; // mm. nominal starting position (new layer or new hqp (post load mode))
  const double NEW_LAYER_OFFSET= 5; // degrees. Offset needed to place a new layer row between the old last advance and the new first retract
  const double ADV_FOOT_RIA_OFFSET_ANGLE= 50.0;
  const double RET_FOOT_RIA_OFFSET_ANGLE= 100.0;
  // start of coil (soc) ria angle constants
  const double SOC_POST_LOAD_ANGLE= -140.0;
  const double SOC_INIT_RETR_ANGLE= -130.0;
  const double SOC_INIT_ADV_ANGLE= -80.0;

  // joggle adjustment
  const double JOGGLE_LENGTH_MIN= 16.18;	// degrees @ turn 1
  const double JOGGLE_LENGTH_MAX= 28.12;	// degrees @ turn 14
  // Joggle adjustment thresholds. The joggle length is subtracted from these threshold to get a range of angles
  // where the adjustment applies.  A positive angle means the joggle is in front of a foot (joggle has not been reached yet)
  // and a negative angle means the joggle is behind a foot (the joggle has been passed)
  const double JOGGLE_RETRACT_ADJ_THRESHOLD= 360.0; // number of degrees prior to a joggle that a retract adjustment is made
  const double JOGGLE_FULL_RETRACT_THRESHOLD= 0.0; // number of degrees prior to (or after) a joggle that a full retract is made
  const double JOGGLE_ADV_TO_FIRST_THRESHOLD= -360.0; // number of degrees past a joggle that the advancing foot moves
                                                   // to the first turn (turn 1 or 14) (after role switch)

  // transition adjustment
  const double TRANS_STRAIGHT_LENGTH= 220.25; // mm length of transition straight segment
  const double TRANS_ARC_DEG= 27.06;  // degrees of transition arc angle
  const double TRANS_Ro= TRANS_STRAIGHT_LENGTH / sin(TRANS_ARC_DEG * DEG_TO_RADIANS);
  

  // feature codes
  const std::string FC_TRANSITION= "T";
  const std::string FC_OUTLET= "O";
  const std::string FC_INLET= "I";
  const std::string FC_JOGGLE= "J";
  const std::string FC_WINDING_LOCK= "W";
  const std::string FC_LOCAL= "L";

// Event related values

  // RIA features
  // round and cast to integer types to avoid warnings
  const long OFFSET_RET_FOOT= -1 * static_cast<long>(round(abs(RET_FOOT_RIA_OFFSET_ANGLE))); // make negative
  const long OFFSET_PLOW= -55;  // TODO: confirm
  const long OFFSET_ADV_FOOT= -1 * static_cast<long>(round(abs(ADV_FOOT_RIA_OFFSET_ANGLE))); // make negative
  const long OFFSET_0U= 0;
  const long OFFSET_2U= 160;
  const long OFFSET_AWH1= 207;
  const long OFFSET_AWH2= 247;
  const long OFFSET_AWH3= 327;  // TODO: confirm
  
  // Landing roller move events are based on the landed turn, but the coil map
  // angles are based on the CLS and 0U roller. These offsets are necessary to
  // make the landing roller events to coincide with when the coil map angles 
  // are being landed.
  // Post CSM1 Update: Use the below offsets. Now there are two LR locations
  // const long OFFSET_LANDING_ROLLER= 760;   
  const long LR_ODD_LAYER_OFFSET= 660;  // Lr -> 40  Deg
  const long LR_ODD_LAYER_TURN= 8;      // Lr -> 40  Deg on this turn 
  const long LR_EVEN_LAYER_OFFSET= 820; // Lr -> 200 Deg
  const long LR_EVEN_LAYER_TURN= 7;     // Lr -> 200 Deg on this turn

  const long OFFSET_LANDED_TURN= 960;
  
  const long OFFSET_FIDUCIAL_LASER= 1005;

  const long ANGLE_OFFSET_SMALL= 8;   // degrees
  const long ANGLE_OFFSET_LARGE= 30;   // degrees
  const long ANGLE_OFFSET_CE= 5;  // degree offset used in consolidation event angle calc

  const long CONSOLIDATION_INTERVAL= 120; // how often to make an odd layer consolidation event

  const long FIDUCIAL_LASER_EVENT_LOCAL_OFFSET= 65;


  // list of layer numbers where coil measurement and compression take place
  // mnockup
  //const long LA_ME_CO[] = { 4, 7, 9, 11, 14, 17 }; // name is from LAyers for MEasurment and COmpression
  // normal coil
  // Note the values in the list below are 1 higher than the landed turn where the measurement and compression
  // is occurring. This is becasue the event generation is tied in with Ria Angle, which deals with turns
  // being dropped, but the measurement and compression actions are done on landed turns.
  // When turn 4 is being droppped, turn 3 is landed, for example.
  const long LA_ME_CO[] = { 4, 7, 10, 13, 16, 19, 21, 23, 26, 29, 32, 35, 38, 41 }; // name is from LAyers for MEasurment and COmpression
  const size_t NUM_OF_LA_ME_CO = sizeof(LA_ME_CO) / sizeof(LA_ME_CO[0]);
// db constants
  // test db
  // const std::string DB_SERVER_NAME= "VMUSERHOST\\STN06DEVTEST1"; 
  // const std::string DB_DATABASE_NAME= "gaStn06_PostCsm1Test";
  // real ows db
  const std::string DB_SERVER_NAME= "10.6.1.10";
  const std::string DB_DATABASE_NAME= "gaStn06";
  const std::string DB_USER_NAME= "ScsStn06";
  const std::string DB_PASSWORD= "scswrapperstn06";

  const std::string MS_TOKEN= "*MS:"; // sequence in the logic trace used to denote the start of the move summary.
  
  // db stored procedure parameter names
  // Coil Map
  const std::string CM_ANGLE_PARAM= "coilAngle";
  const std::string CM_FEATURECODE_PARAM= "featureCode";
  const std::string CM_HQP_PARAM= "hqp";
  const std::string CM_LAYER_PARAM= "layer";
  const std::string CM_TURN_PARAM= "turn";
  const std::string CM_OVERALL_TURN_PARAM= "overallTurn";
  const std::string CM_AZIMUTH_PARAM= "azimuth";
  const std::string CM_RADIUS_PARAM= "radius";
  
  // Scs Axis Position
  const std::string SAP_RIAANGLE_PARAM= "riaAngle";
  const std::string SAP_COILANGLE_PARAM= "coilAngle";
  const std::string SAP_LOGICTRACE_PARAM= "logicTrace";
  const std::string SAP_ACTIONDESC_PARAM= "actionDesc";
  const std::string SAP_ISSELECTEDAXES_PARAM= "isSelectedAxes";
  const std::string SAP_ISABSOLUTE_PARAM= "isAbsoluteEntry";
  const std::string SAP_ISTRANSITION_PARAM= "isInTransition";
  const std::string SAP_ISJOGGLE_PARAM= "isInJoggle";
  const std::string SAP_ISNEWHQP_PARAM= "isNewHqp";
  const std::string SAP_ISNEWLAYER_PARAM= "isNewLayer";
  const std::string SAP_ISLASTTURN_PARAM= "isLastTurn";
  const std::string SAP_ISLASTLAYER_PARAM= "isLastLayer";
  const std::string SAP_HQPADJ_PARAM = "hqpAdj";
  const std::string SAP_LAYERADJ_PARAM = "layerAdj";
  const std::string SAP_FTAIN_PARAM= "footAInPosDist";
  const std::string SAP_FTAOUT_PARAM= "footAOutPosDist";
  const std::string SAP_FTBIN_PARAM= "footBInPosDist";
  const std::string SAP_FTBOUT_PARAM= "footBOutPosDist";
  const std::string SAP_FTCIN_PARAM= "footCInPosDist";
  const std::string SAP_FTCOUT_PARAM= "footCOutPosDist";
  const std::string SAP_FTDIN_PARAM= "footDInPosDist";
  const std::string SAP_FTDOUT_PARAM= "footDOutPosDist";
  const std::string SAP_FTEIN_PARAM= "footEInPosDist";
  const std::string SAP_FTEOUT_PARAM= "footEOutPosDist";
  const std::string SAP_FTFIN_PARAM= "footFInPosDist";
  const std::string SAP_FTFOUT_PARAM= "footFOutPosDist";
  const std::string SAP_COLAIN_PARAM= "columnAInPosDist";
  const std::string SAP_COLAOUT_PARAM= "columnAOutPosDist";
  const std::string SAP_COLBIN_PARAM= "columnBInPosDist";
  const std::string SAP_COLBOUT_PARAM= "columnBOutPosDist";
  const std::string SAP_COLCIN_PARAM= "columnCInPosDist";
  const std::string SAP_COLCOUT_PARAM= "columnCOutPosDist";
  const std::string SAP_COLDIN_PARAM= "columnDInPosDist";
  const std::string SAP_COLDOUT_PARAM= "columnDOutPosDist";
  const std::string SAP_COLEIN_PARAM= "columnEInPosDist";
  const std::string SAP_COLEOUT_PARAM= "columnEOutPosDist";
  const std::string SAP_COLFIN_PARAM= "columnFInPosDist";
  const std::string SAP_COLFOUT_PARAM= "columnFOutPosDist";
  const std::string SAP_POSDIST_SEL_PARAM= "posDist"; // distance or position value for selected absolute or relative inserts
  const std::string SAP_ABS_ADJ_DIST_SEL_PARAM= "dist"; // distance adjustment for selected absolute adjustment inserts
  const std::string SAP_FTAIN_SEL_PARAM= "footAIn";
  const std::string SAP_FTAOUT_SEL_PARAM= "footAOut";
  const std::string SAP_FTBIN_SEL_PARAM= "footBIn";
  const std::string SAP_FTBOUT_SEL_PARAM= "footBOut";
  const std::string SAP_FTCIN_SEL_PARAM= "footCIn";
  const std::string SAP_FTCOUT_SEL_PARAM= "footCOut";
  const std::string SAP_FTDIN_SEL_PARAM= "footDIn";
  const std::string SAP_FTDOUT_SEL_PARAM= "footDOut";
  const std::string SAP_FTEIN_SEL_PARAM= "footEIn";
  const std::string SAP_FTEOUT_SEL_PARAM= "footEOut";
  const std::string SAP_FTFIN_SEL_PARAM= "footFIn";
  const std::string SAP_FTFOUT_SEL_PARAM= "footFOut";
  const std::string SAP_COLAIN_SEL_PARAM= "columnAIn";
  const std::string SAP_COLAOUT_SEL_PARAM= "columnAOut";
  const std::string SAP_COLBIN_SEL_PARAM= "columnBIn";
  const std::string SAP_COLBOUT_SEL_PARAM= "columnBOut";
  const std::string SAP_COLCIN_SEL_PARAM= "columnCIn";
  const std::string SAP_COLCOUT_SEL_PARAM= "columnCOut";
  const std::string SAP_COLDIN_SEL_PARAM= "columnDIn";
  const std::string SAP_COLDOUT_SEL_PARAM= "columnDOut";
  const std::string SAP_COLEIN_SEL_PARAM= "columnEIn";
  const std::string SAP_COLEOUT_SEL_PARAM= "columnEOut";
  const std::string SAP_COLFIN_SEL_PARAM= "columnFIn";
  const std::string SAP_COLFOUT_SEL_PARAM= "columnFOut";

  // db stored procedure names
  // coil map related
  const std::string SPNAME_SELECT_COIL_MAP= "coil.sprocSelectCoilMap"; // Select everything from the coil map
  const std::string SPNAME_SELECT_CMOLT14FCT= "coil.sprocSelectCmOlT14FcT"; // Coil Map Odd Layer Turn 14 Feature Code "T"
  const size_t MAX_NUM_OF_CNOLT14FCT = 21; // nominally the number of odd layers, add 1 just in case
  const std::string SPNAME_SELECT_JOGGLE_ANGLES= "coil.sprocSelectJoggleAngles"; // Coil Map Joggle Angles
  const size_t MAX_NUM_OF_JOGGLE_ANGLES = 41; // nominally the number of layers, add 1 just in case
  // Cls and Scs position related
  const std::string SPNAME_DELETE_ALL_POS= "coil.sprocDeleteAllAxisPositions"; // Delete all rows in the CLS and SCS position tables
  
  const std::string SPNAME_INSERT_ALL_SCS_POS= "coil.sprocInsertPosDistScs"; // insert row for all SCS axes
                                                                             // (absolute position or relative distance)
  const std::string SPNAME_INSERT_SEL_ADJ_ABS_SCS_POS= "coil.sprocInsertSelectPosFromPreviousScs"; // insert absolute position row for selected axis, adjusting
                                                                                                   // adjusting from previous values by specified distance.
  const std::string SPNAME_INSERT_SEL_SCS_POS= "coil.sprocInsertSelectPosDistScs"; // insert row for selected SCS axes
                                                                                   // (absolute position or relative distance)
  const std::string SPNAME_CALC_CLS_POS= "coil.sprocCalcClsPosFromScs"; // calculate cls moves from SCS position table

  // event list related
  // Delete all rows in event list table which are not complete and don't have associated completed actions 
  const std::string SPNAME_DELETE_ALL_INC_EVENTS= "events.sprocDeleteUndoneEvents"; 
  const std::string SPNAME_INSERT_EVENTLIST= "events.sprocInsertToEventList"; // Inserts a record into the Event List
  const std::string SPNAME_SELECT_HQPSTART_ANGLES= "events.sprocSelectStartHqpAngles"; // New Hqp start angles from ScsPosTable
  const size_t MAX_NUM_OF_HQP_START_ANGLES = 8; // nominally the number of HQPs, add 1 just in case
  const std::string SPNAME_SELECT_LAYERSTART_ANGLES= "events.sprocSelectStartLayerAngles"; // New Layer start angles from ScsPosTable
  const size_t MAX_NUM_OF_LAYER_START_ANGLES = 41; // nominally the number of layer, add 1 just in case
  }
#endif  //GA_ScsDataConstants_H_
