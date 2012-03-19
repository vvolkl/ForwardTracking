#include "TrueTrackCritAnalyser.h"

#include <algorithm>
#include <cmath>

#include "EVENT/LCCollection.h"
#include "EVENT/MCParticle.h"
#include "EVENT/Track.h"
#include "EVENT/MCParticle.h"
#include "IMPL/TrackerHitPlaneImpl.h"
#include "marlin/VerbosityLevels.h"
#include "marlin/Global.h"

#include "TVector3.h"
#include "Math/ProbFunc.h"  // Root, for calculating the chi2 probability. 


#include "FTrackILDTools.h"
#include "Criteria.h"
#include "FTDTrack.h"
#include "FTDHit00.h"

using namespace lcio ;
using namespace marlin ;
using namespace FTrack;





TrueTrackCritAnalyser aTrueTrackCritAnalyser ;


TrueTrackCritAnalyser::TrueTrackCritAnalyser() : Processor("TrueTrackCritAnalyser") {
   
   // modify processor description
   _description = "TrueTrackCritAnalyser: Analysis of different criteria for true tracks in the FTD" ;
   
   
   // register steering parameters: name, description, class-variable, default value
   
   registerInputCollection(LCIO::LCRELATION,
                           "MCTrueTrackRelCollectionName",
                           "Name of the TrueTrack MC Relation collection",
                           _colNameMCTrueTracksRel,
                           std::string("TrueTracksMCP"));
   
   
   
   registerProcessorParameter("RootFileName",
                              "Name of the root file for saving the results",
                              _rootFileName,
                              std::string("TrueTracksCritAnalysis.root") );
   
   
   //For fitting:
   
   registerProcessorParameter("MultipleScatteringOn",
                              "Use MultipleScattering in Fit",
                              _MSOn,
                              bool(true));
   
   registerProcessorParameter("EnergyLossOn",
                              "Use Energy Loss in Fit",
                              _ElossOn,
                              bool(true));
   
   registerProcessorParameter("SmoothOn",
                              "Smooth All Measurement Sites in Fit",
                              _SmoothOn,
                              bool(false));
   
   
   registerProcessorParameter("Chi2ProbCut",
                              "Tracks with a chi2 probability below this value won't be considered",
                              _chi2ProbCut,
                              double (0.005) ); 
   
   
   
   registerProcessorParameter("PtMin",
                              "The minimum transversal momentum pt above which tracks are of interest in GeV ",
                              _ptMin,
                              double (0.2)  );   
   
   registerProcessorParameter("DistToIPMax",
                              "The maximum distance from the origin of the MCP to the IP (0,0,0)",
                              _distToIPMax,
                              double (100. ) );   
   
   registerProcessorParameter("NumberOfHitsMin",
                              "The minimum number of hits a track must have",
                              _nHitsMin,
                              int (4)  );   
   
}



void TrueTrackCritAnalyser::init() { 
   
   streamlog_out(DEBUG) << "   init called  " << std::endl ;
   
   // usually a good idea to
   printParameters() ;
   
   
   // TODO: get this from gear
   unsigned int nLayers = 8; // layer 0 is for the IP
   unsigned int nModules = 16;
   unsigned int nSensors = 2;  
   
   _sectorSystemFTD = new SectorSystemFTD( nLayers, nModules , nSensors );
   
   
   _nRun = 0 ;
   _nEvt = 0 ;
   
   
   //Add the criteria that will be checked
   _crits2.push_back( new Crit2_RZRatio( 1. , 1. ) ); 
   _crits2.push_back( new Crit2_StraightTrackRatio( 1. , 1. ) );
   _crits2.push_back( new Crit2_DeltaPhi( 0. , 0. ) );
   _crits2.push_back( new Crit2_HelixWithIP ( 1. , 1. ) );
   _crits2.push_back( new Crit2_DeltaRho( 0. , 0. ) );
   
   
   _crits3.push_back( new Crit3_ChangeRZRatio( 1. , 1. ) );
   _crits3.push_back( new Crit3_PT (0.1 , 0.1) );
   _crits3.push_back( new Crit3_2DAngle (0. , 0.) );
   _crits3.push_back( new Crit3_3DAngle (0. , 0.) );
   _crits3.push_back( new Crit3_IPCircleDist (0. , 0.) );
   
   _crits4.push_back( new  Crit4_2DAngleChange ( 1. , 1. ) );
   _crits4.push_back( new  Crit4_3DAngleChange ( 1. , 1. ) );
   _crits4.push_back( new  Crit4_PhiZRatioChange ( 1. , 1. ) );
   _crits4.push_back( new  Crit4_DistToExtrapolation ( 1. , 1. ) );
   _crits4.push_back( new  Crit4_DistOfCircleCenters ( 1. , 1. ) );
   _crits4.push_back( new  Crit4_NoZigZag ( 1. , 1. ) );
   _crits4.push_back( new  Crit4_RChange ( 1. , 1. ) );
   

   
   
   std::set < std::string > branchNames2; //branch names of the 2-hit criteria
   std::set < std::string > branchNames3;
   std::set < std::string > branchNames4;
   std::set < std::string > branchNamesKalman;
   
   
   // Set up the root file
   // Therefore first get all the possible names of the branches
   
   // create a virtual hit
   IHit* virtualIPHit = FTrackILD::createVirtualIPHit(1 , _sectorSystemFTD );

   
   std::vector <IHit*> hitVec;
   hitVec.push_back( virtualIPHit );
   
   
   /**********************************************************************************************/
   /*                Set up the tree for the 1-segments (2 hit criteria)                         */
   /**********************************************************************************************/
   
   Segment virtual1Segment( hitVec );
   
   
   for ( unsigned int i=0; i < _crits2 .size() ; i++ ){ //for all criteria

      _crits2[i]->setSaveValues( true ); // so the calculated values won't just fade away, but are saved in a map
      //get the map
      _crits2 [i]->areCompatible( &virtual1Segment , &virtual1Segment ); // It's a bit of a cheat: we calculate it for virtual hits to get a map containing the
                                                                   // names of the values ( and of course values that are useless, but we don't use them here anyway)
      
      std::map < std::string , float > newMap = _crits2 [i]->getMapOfValues();
      std::map < std::string , float > ::iterator it;
      
      for ( it = newMap.begin() ; it != newMap.end() ; it++ ){ //over all values in the map

         
         branchNames2.insert( it->first ); //store the names of the values in the set critNames
         
      }
      
   }
   
   
   // Also insert branches for additional information
   branchNames2.insert( "MCP_pt" ); //transversal momentum
   branchNames2.insert( "MCP_distToIP" ); //the distance of the origin of the partivle to the IP
   branchNames2.insert( "layers" ); // a code for the layers the used hits had: 743 = layer 7, 4 and 3
   branchNames2.insert( "distance" ); // the distance between two hits
   // Set up the root file with the tree and the branches
   _treeName2 = "2Hit";
   FTrackILD::setUpRootFile( _rootFileName, _treeName2, branchNames2 );      //prepare the root file.
   
   
   
   
   /**********************************************************************************************/
   /*                Set up the tree for the 2-segments (3 hit criteria)                         */
   /**********************************************************************************************/
   
   hitVec.push_back( virtualIPHit );
   Segment virtual2Segment( hitVec );
   
   
   for ( unsigned int i=0; i < _crits3 .size() ; i++ ){ //for all criteria


      _crits3[i]->setSaveValues( true ); // so the calculated values won't just fade away, but are saved in a map

      //get the map
      _crits3 [i]->areCompatible( &virtual2Segment , &virtual2Segment ); // It's a bit of a cheat: we calculate it for virtual hits to get a map containing the
      // names of the values ( and of course values that are useless, but we don't use them here anyway)
      
      std::map < std::string , float > newMap = _crits3 [i]->getMapOfValues();
      std::map < std::string , float > ::iterator it;
      
      for ( it = newMap.begin() ; it != newMap.end() ; it++ ){ //over all values in the map

         
         branchNames3.insert( it->first ); //store the names of the values in the set critNames
         
      }
      
   }
   
   
   // Also insert branches for additional information
   branchNames3.insert( "MCP_pt" ); //transversal momentum
   branchNames3.insert( "MCP_distToIP" ); //the distance of the origin of the partivle to the IP
   branchNames3.insert( "layers" ); // a code for the layers the used hits had: 743 = layer 7, 4 and 3
   
   // Set up the root file with the tree and the branches
   _treeName3 = "3Hit"; 
   
   FTrackILD::setUpRootFile( _rootFileName, _treeName3, branchNames3 , false );      //prepare the root file.
  
   
   
   /**********************************************************************************************/
   /*                Set up the tree for the 3-segments (4 hit criteria)                         */
   /**********************************************************************************************/
   
   hitVec.push_back( virtualIPHit );
   Segment virtual3Segment( hitVec );
   
   
   for ( unsigned int i=0; i < _crits4 .size() ; i++ ){ //for all criteria

      _crits4[i]->setSaveValues( true ); // so the calculated values won't just fade away, but are saved in a map

      //get the map
      _crits4 [i]->areCompatible( &virtual3Segment , &virtual3Segment ); // It's a bit of a cheat: we calculate it for virtual hits to get a map containing the
      // names of the values ( and of course values that are useless, but we don't use them here anyway)
      
      std::map < std::string , float > newMap = _crits4 [i]->getMapOfValues();
      std::map < std::string , float > ::iterator it;
      
      for ( it = newMap.begin() ; it != newMap.end() ; it++ ){ //over all values in the map

         
         branchNames4.insert( it->first ); //store the names of the values in the set critNames
         
      }
      
   }
   
   
   // Also insert branches for additional information
   branchNames4.insert( "MCP_pt" ); //transversal momentum
   branchNames4.insert( "MCP_distToIP" ); //the distance of the origin of the partivle to the IP
   branchNames4.insert( "layers" ); // a code for the layers the used hits had: 743 = layer 7, 4 and 3
   
   // Set up the root file with the tree and the branches
   _treeName4 = "4Hit"; 
   
   FTrackILD::setUpRootFile( _rootFileName, _treeName4, branchNames4 , false );      //prepare the root file.
   
 
   delete virtualIPHit;
   
   
   
   /**********************************************************************************************/
   /*                Set up the tree for Kalman Fits                                             */
   /**********************************************************************************************/
   
   branchNamesKalman.insert( "chi2prob" );
   branchNamesKalman.insert( "chi2" );
   branchNamesKalman.insert( "Ndf" );
   branchNamesKalman.insert( "nHits" );
   branchNamesKalman.insert( "MCP_pt" ); //transversal momentum
   branchNamesKalman.insert( "MCP_distToIP" ); //the distance of the origin of the partivle to the IP
   
   // Set up the root file with the tree and the branches
   _treeNameKalman = "KalmanFit"; 
   
   FTrackILD::setUpRootFile( _rootFileName, _treeNameKalman, branchNamesKalman , false );      //prepare the root file.
   
 
   /**********************************************************************************************/
   /*                Set up the track fitter                                                     */
   /**********************************************************************************************/
   
   
   //Initialise the TrackFitter of the tracks:
   FTDTrack::initialiseFitter( "KalTest" , marlin::Global::GEAR , "" , _MSOn , _ElossOn , _SmoothOn  );
   
   
}


void TrueTrackCritAnalyser::processRunHeader( LCRunHeader* run) { 
   
   _nRun++ ;
} 



void TrueTrackCritAnalyser::processEvent( LCEvent * evt ) { 
   
   
   
   std::vector < std::map < std::string , float > > rootDataVec2;
   std::vector < std::map < std::string , float > > rootDataVec3;
   std::vector < std::map < std::string , float > > rootDataVec4;
   std::vector < std::map < std::string , float > > rootDataVecKalman;
  
   // get the true tracks 
   LCCollection* col = evt->getCollection( _colNameMCTrueTracksRel ) ;
   
   
   
   if( col != NULL ){
      
      int nMCTracks = col->getNumberOfElements();

      
      unsigned nUsedRelations = 0;

      for( int i=0; i < nMCTracks; i++){ // for every true track
      

         bool isOfInterest = true;  // A bool to hold information wether this track we are looking at is interesting for us at all
                                    // So we might apply different criteria to it.
                                    // For example: if a track is very curly we might not want to consider it at all.

     
      
         LCRelation* rel = dynamic_cast <LCRelation*> (col->getElementAt(i) );
         MCParticle* mcp = dynamic_cast <MCParticle*> (rel->getTo() );
         Track*    track = dynamic_cast <Track*>      (rel->getFrom() );

         
         
         
         //////////////////////////////////////////////////////////////////////////////////
         //If distance from origin is not too high      
         double dist = sqrt(mcp->getVertex()[0]*mcp->getVertex()[0] + 
         mcp->getVertex()[1]*mcp->getVertex()[1] + 
         mcp->getVertex()[2]*mcp->getVertex()[2] );
         
         
         
         if ( dist > _distToIPMax ) isOfInterest = false;   // exclude point too far away from the origin. Of course we want them reconstructed too,
                                                // but at the moment we are only looking at the points that are reconstructed by a simple
                                                // Cellular Automaton, which uses the point 0 as a point in the track
                                                //
                                                //////////////////////////////////////////////////////////////////////////////////
                                                
         //////////////////////////////////////////////////////////////////////////////////
         //If pt is not too low
                                                
         double pt = sqrt( mcp->getMomentum()[0]*mcp->getMomentum()[0] + mcp->getMomentum()[1]*mcp->getMomentum()[1] );
         
         if ( pt < _ptMin ) isOfInterest = false;
         //
         //////////////////////////////////////////////////////////////////////////////////
         
         //////////////////////////////////////////////////////////////////////////////////
         //If there are more than 4 hits in the track
         
         if ( (int) track->getTrackerHits().size() < _nHitsMin ) isOfInterest = false;
         //
         //////////////////////////////////////////////////////////////////////////////////
         
         
         //////////////////////////////////////////////////////////////////////////////////
         //If the chi2 probability is too low
         
         //Fit the track
         
         std::vector <TrackerHit*> trackerHits = track->getTrackerHits();
         // sort the hits in the track
         sort( trackerHits.begin(), trackerHits.end(), FTrackILD::compare_TrackerHit_z );
         // now at [0] is the hit with the smallest |z| and at [1] is the one with a bigger |z| and so on
         // So the direction of the hits when following the index from 0 on is:
         // from inside out: from the IP into the distance.
        
         
         std::vector <IHit*> hits;
         
         for ( unsigned j=0; j< trackerHits.size(); j++ ) hits.push_back( new FTDHit00( trackerHits[j] , _sectorSystemFTD ) );
        
         
         FTDTrack myTrack;
         for( unsigned j=0; j<hits.size(); j++ ) myTrack.addHit( hits[j] );
         
         myTrack.fit();
         
         if ( myTrack.getChi2Prob() < _chi2ProbCut ) isOfInterest = false;
         //
         //////////////////////////////////////////////////////////////////////////////////
         
         if ( isOfInterest ){ 
            
            
            nUsedRelations++;
               
            // Additional information on the track
            const double* p = mcp->getMomentum();
            
            double pt=  sqrt( p[0]*p[0]+p[1]*p[1] );
            
            const double* vtx = mcp->getVertex();
            double distToIP = sqrt( vtx[0]*vtx[0] + vtx[1]*vtx[1] + vtx[2]*vtx[2] );
            
            
            
            
            // Add the IP as a hit
            IHit* virtualIPHit = FTrackILD::createVirtualIPHit(1 , _sectorSystemFTD );
           
            hits.insert( hits.begin() , virtualIPHit );
            
           
            /**********************************************************************************************/
            /*                Manipulate the hits (for example erase some or add some)                    */
            /**********************************************************************************************/
            
            float distMin = 5.;
            //Erase hits that are too close. For those will be from overlapping petals
            for ( unsigned j=1; j < hits.size() ; j++ ){
               
               IHit* hitA = hits[j-1];
               IHit* hitB = hits[j];
               
               float dist = hitA->distTo( hitB );
               
               if( dist < distMin ){
                  
                  hits.erase( hits.begin() + j );
                  j--;
                  
               }               
               
            }
            
            /**********************************************************************************************/
            /*                Build the segments                                                          */
            /**********************************************************************************************/
            
            // Now we have a vector of autHits starting with the IP followed by all the hits from the track.
            // So we now are able to build segments from them
            
            std::vector <Segment*> segments1;
            
            for ( unsigned j=0; j < hits.size(); j++ ){
               
               
               std::vector <IHit*> segHits;
               segHits.insert( segHits.begin() , hits.begin()+j , hits.begin()+j+1 );
               
               segments1.push_back( new Segment( segHits ) );
               
            }
            
            std::vector <Segment*> segments2;
            
            for ( unsigned j=0; j < hits.size()-1; j++ ){
               
               
               std::vector <IHit*> segHits;
               
               segHits.push_back( hits[j+1] );
               segHits.push_back( hits[j] );
               
               segments2.push_back( new Segment( segHits ) );
               
            }
            
            std::vector <Segment*> segments3;
            
            for ( unsigned j=0; j < hits.size()-2; j++ ){
               
               
               std::vector <IHit*> segHits;
               
               segHits.push_back( hits[j+2] );
               segHits.push_back( hits[j+1] );
               segHits.push_back( hits[j] );
               
               segments3.push_back( new Segment( segHits ) );
               
            }
            
            // Now we have the segments of the track ( ordered) in the vector
            
            /**********************************************************************************************/
            /*                Use the criteria on the segments                                            */
            /**********************************************************************************************/
            
                     
            for ( unsigned j=0; j < segments1.size()-1; j++ ){
               
               // the data that will get stored
               std::map < std::string , float > rootData;
               
               //make the check on the segments, store it in the the map...
               Segment* child = segments1[j];
               Segment* parent = segments1[j+1];
               
               
               for( unsigned iCrit=0; iCrit < _crits2 .size(); iCrit++){ // over all criteria

                  
                  //get the map
                  _crits2 [iCrit]->areCompatible( parent , child ); //calculate their compatibility
                  
                  std::map < std::string , float > newMap = _crits2 [iCrit]->getMapOfValues(); //get the values that were calculated
                  
                  rootData.insert( newMap.begin() , newMap.end() );
                  
               }
               
               rootData["MCP_pt"] = pt;
               rootData["MCP_distToIP"] = distToIP;
               rootData["layers"] = child->getHits()[0]->getLayer() *10 + parent->getHits()[0]->getLayer();
               
               IHit* childHit = child->getHits()[0];
               IHit* parentHit = parent->getHits()[0];
               float dx = childHit->getX() - parentHit->getX();
               float dy = childHit->getY() - parentHit->getY();
               float dz = childHit->getZ() - parentHit->getZ();
               rootData["distance"] = sqrt( dx*dx + dy*dy + dz*dz );
               
               rootDataVec2.push_back( rootData );
               
            }
            
            
            for ( unsigned j=0; j < segments2.size()-1; j++ ){
               
               // the data that will get stored
               std::map < std::string , float > rootData;
               
               //make the check on the segments, store it in the the map...
               Segment* child = segments2[j];
               Segment* parent = segments2[j+1];
               
               
               for( unsigned iCrit=0; iCrit < _crits3 .size(); iCrit++){ // over all criteria

                  
                  //get the map
                  _crits3 [iCrit]->areCompatible( parent , child ); //calculate their compatibility
                  
                  std::map < std::string , float > newMap = _crits3 [iCrit]->getMapOfValues(); //get the values that were calculated
                  
                  rootData.insert( newMap.begin() , newMap.end() );
                  
               }
               
               rootData["MCP_pt"] = pt;
               rootData["MCP_distToIP"] = distToIP;
               rootData["layers"] = child->getHits()[1]->getLayer() *100 +
                                    child->getHits()[0]->getLayer() *10 + 
                                    parent->getHits()[0]->getLayer();
               
               rootDataVec3.push_back( rootData );
               
            }
            
            
            for ( unsigned j=0; j < segments3.size()-1; j++ ){
               
               // the data that will get stored
               std::map < std::string , float > rootData;
               
               //make the check on the segments, store it in the the map...
               Segment* child = segments3[j];
               Segment* parent = segments3[j+1];
               
               
               for( unsigned iCrit=0; iCrit < _crits4 .size(); iCrit++){ // over all criteria

                  
                  //get the map
                  _crits4 [iCrit]->areCompatible( parent , child ); //calculate their compatibility
                  
                  std::map < std::string , float > newMap = _crits4 [iCrit]->getMapOfValues(); //get the values that were calculated
                  
                  rootData.insert( newMap.begin() , newMap.end() );
                  
               }
               
               rootData["MCP_pt"] = pt;
               rootData["MCP_distToIP"] = distToIP;
               rootData["layers"] = child->getHits()[2]->getLayer() *1000 +
                                    child->getHits()[1]->getLayer() *100 +
                                    child->getHits()[0]->getLayer() *10 + 
                                    parent->getHits()[0]->getLayer();
               
               rootDataVec4.push_back( rootData );
               
            }
            
            
            
            /**********************************************************************************************/
            /*                Save the fit of the track                                                   */
            /**********************************************************************************************/
            
            // the data that will get stored
            std::map < std::string , float > rootData;
               
               
               
            float chi2 = myTrack.getChi2();
            int Ndf = int( myTrack.getNdf() );
            float chi2Prob = myTrack.getChi2Prob();
            
            
            
            rootData[ "chi2" ]          = chi2;
            rootData[ "Ndf" ]           = Ndf;
            rootData[ "nHits" ]         = myTrack.getHits().size();
            rootData[ "chi2prob" ]      = chi2Prob;
            
            rootData["MCP_pt"] = pt;
            rootData["MCP_distToIP"] = distToIP;
            
            rootDataVecKalman.push_back( rootData );
            
            
            
            
            /**********************************************************************************************/
            /*                Clean up                                                                    */
            /**********************************************************************************************/
            
            for (unsigned i=0; i<segments1.size(); i++) delete segments1[i];
            segments1.clear();
            for (unsigned i=0; i<segments2.size(); i++) delete segments2[i];
            segments2.clear();
            for (unsigned i=0; i<segments3.size(); i++) delete segments3[i];
            segments3.clear();
            for (unsigned i=0; i<hits.size(); i++) delete hits[i];
            hits.clear();
            
            
            
            
            
         }
       
      }
         
        
        
      /**********************************************************************************************/
      /*                Save all the data to ROOT                                                   */
      /**********************************************************************************************/
        

      FTrackILD::saveToRoot( _rootFileName, _treeName2, rootDataVec2 );
      FTrackILD::saveToRoot( _rootFileName, _treeName3, rootDataVec3 );
      FTrackILD::saveToRoot( _rootFileName, _treeName4, rootDataVec4 );
      FTrackILD::saveToRoot( _rootFileName, _treeNameKalman, rootDataVecKalman );
      
         
      streamlog_out (MESSAGE) << "\n Number of used mcp-track relations: " << nUsedRelations <<"\n";
    
   }
 

 

   //-- note: this will not be printed if compiled w/o MARLINDEBUG4=1 !

   streamlog_out(DEBUG) << "   processing event: " << evt->getEventNumber() 
   << "   in run:  " << evt->getRunNumber() << std::endl ;


   _nEvt ++ ;
   
   
}



void TrueTrackCritAnalyser::check( LCEvent * evt ) { 
   // nothing to check here - could be used to fill checkplots in reconstruction processor
}


void TrueTrackCritAnalyser::end(){ 
   
   //   streamlog_out( DEBUG ) << "MyProcessor::end()  " << name() 
   //      << " processed " << _nEvt << " events in " << _nRun << " runs "
   //      << std::endl ;
   
   for (unsigned i=0; i<_crits2 .size(); i++) delete _crits2 [i];
   for (unsigned i=0; i<_crits3 .size(); i++) delete _crits3 [i];
   for (unsigned i=0; i<_crits4 .size(); i++) delete _crits4 [i];
   
   delete _sectorSystemFTD;
   _sectorSystemFTD = NULL;
   
   
}






