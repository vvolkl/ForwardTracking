#include "ForwardTracking.h"
#include <iostream>
#include <algorithm>

#include <EVENT/TrackerHit.h>
#include <EVENT/Track.h>
#include <EVENT/LCCollection.h>
#include <IMPL/LCCollectionVec.h>

// For the virtual IP
#include <IMPL/TrackerHitPlaneImpl.h>

// ----- include for verbosity dependend logging ---------
#include "marlin/VerbosityLevels.h"

#include <MarlinCED.h>

#include <gear/BField.h>


// Root, for calculating the chi2 probability. 
#include "Math/ProbFunc.h"


//--------------------------------------------------------------
//My own classes begin

#include "FTDRepresentation.h"
#include "AutCode.h"

#include "SegmentBuilder.h"
#include "Automaton.h"

#include "AutHit.h"

// the criteria
#include "CritRZRatio.h"
#include "Crit3_3DAngle.h"
#include "Crit2_StraightTrack.h"


// the hit connectors
#include "HitCon.h"


//My own classe end
//--------------------------------------------------------------


using namespace lcio ;
using namespace marlin ;
using namespace FTrack;
using namespace MarlinTrk ;





ForwardTracking aForwardTracking ;


ForwardTracking::ForwardTracking() : Processor("ForwardTracking") {

   // modify processor description
   _description = "ForwardTracking tests the Cellular Automaton" ;


   // register steering parameters: name, description, class-variable, default value
   registerInputCollection(LCIO::TRACKERHIT,
                           "FTDHitCollectionName",
                           "FTD Hit Collection Name",
                           _FTDHitCollection,
                           std::string("FTDTrackerHits")); 


   registerOutputCollection(LCIO::TRACK,
                           "AutTrkCollection",
                           "Name of Cellular Automaton Track output collection",
                           _AutTrkCollection,
                           std::string("AutTracks"));

   
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
   
                                
                                
}




void ForwardTracking::init() { 

   streamlog_out(DEBUG) << "   init called  " << std::endl ;

   // usually a good idea to
   printParameters() ;

   _nRun = 0 ;
   _nEvt = 0 ;
   

   MarlinCED::init(this) ;    //CED
    
  

   //Initialise the TrackFitter (as the system and the method of fitting will stay the same over the events, we might set it up here,
   //( otherwise we would have to repeat this over and over again)
   
   //First set some bools
   _trackFitter.setMSOn(_MSOn);        // Multiple scattering on
   _trackFitter.setElossOn( _ElossOn );     // Energy loss on
   _trackFitter.setSmoothOn( _SmoothOn );   // Smoothing off
   
   //Then initialise
   _trackFitter.initialise( "KalTest" , marlin::Global::GEAR , "" ); //Use KalTest as Fitter
  

  

}


void ForwardTracking::processRunHeader( LCRunHeader* run) { 

    _nRun++ ;
} 



void ForwardTracking::processEvent( LCEvent * evt ) { 


//--CED---------------------------------------------------------------------
// Reset drawing buffer and START drawing collection

  MarlinCED::newEvent(this , 0) ; 

  CEDPickingHandler &pHandler=CEDPickingHandler::getInstance();

  pHandler.update(evt); 

//-----------------------------------------------------------------------



   _Bz = Global::GEAR->getBField().at( gear::Vector3D(0., 0., 0.) ).z();    //The B field in z direction
  

  LCCollection* col = evt->getCollection( _FTDHitCollection ) ;

  
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                                                                                              //
  //                                                                                                              //
  //                            Track Reconstruction in the FTDs                                                  //
  //                                                                                                              //
  //                                                                                                              //
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  


  //First: collect all the hits and store them
  if( col != NULL ){

     
     std::vector <Track*> trackCandidates;
     
      int nHits = col->getNumberOfElements()  ;

      
      streamlog_out( MESSAGE0 ) << "\n\nNumber of hits on the FTDs: " << nHits <<"\n";

      //TODO: get those from gear
      unsigned int nLayers = 8; // layer 0 is for the IP
      unsigned int nModules = 16;
      unsigned int nSensors = 2;
      
      AutCode autCode ( nLayers, nModules , nSensors );
      
      streamlog_out( MESSAGE0 ) << "\n--FTDRepresentation--" ;
      
      FTDRepresentation ftdRep ( &autCode );
      
      for(int i=0; i< nHits ; i++){

       
         TrackerHit* trkHit = dynamic_cast<TrackerHit*>( col->getElementAt( i ) );
         
         //Make an AutHit from the TrackerHit 
         AutHit* autHit = new AutHit ( trkHit );
         
       
         ftdRep.addHit( autHit );
         
                
        
      }
      
      //TODO: put this somewhere else? Maybe a method or another place? To keep the processor more readable
      // Add the virtual IP to the hits (one for forward and one for backward)
      
      for ( int side=-1 ; side <= 1; side+=2 ){

         
         TrackerHitPlaneImpl* virtualIPHit = new TrackerHitPlaneImpl ;
         
         //TODO: keep in mind, that it might be a good idea to implement more things here (like u and v and stuff?)
         
         double pos[] = {0. , 0. , 0.};
         virtualIPHit->setPosition(  pos  ) ;
         
         // create the AutHit and set its parameters
         AutHit* virtualIPAutHit = new AutHit ( virtualIPHit );
         virtualIPAutHit->setIsVirtual ( true );
         virtualIPAutHit->setSide( side );
         virtualIPAutHit->setLayer(0);
         virtualIPAutHit->setModule(0);
         virtualIPAutHit->setSensor(0);
         
         // Add the AutHit to the FTDRepresentation
         ftdRep.addHit( virtualIPAutHit );
         
      
      }
      
      
      
      

      
      
      streamlog_out( MESSAGE0 ) << "\n--SegementBuilder--" ;
      
      //Create a segmentbuilder
      SegmentBuilder segBuilder( &ftdRep );
      
      //Load in some criteria
      segBuilder.addCriterion ( new Crit2_StraightTrack(1.001) ); 
      
      //Also load hit connectors
      segBuilder.addHitConnector ( new HitCon( &autCode ) );
      
      
      // And get out the 1-segments
      Automaton automaton = segBuilder.get1SegAutomaton();
      
      
      /**********************************************************************************************/
      /*                Automaton                                                                   */
      /**********************************************************************************************/
      
      
      
      streamlog_out( MESSAGE0 ) << "\n--Automaton--" ;
      
      // Let the automaton lengthen its 1-segments to 2-segments
      // Because for 1-segments (== single hits) and automaton isn't very useful. TODO: verify this hyphothesis
      automaton.lengthenSegments();
      
      
      // So now we have 2-segments and are ready to perform the cellular automaton.
      // Load some criteria for the automaton:
      automaton.addCriterion ( new Crit3_3DAngle( 6. ) );
      
      // Perform the automaton
      automaton.doAutomaton();
      
      
      //Clean segments with bad states
      automaton.cleanBadStates();
      
      //Clean connections of segments (this uses the same criteria again as before)
      automaton.cleanBadConnections();
      
      //Reset the states of all segments
      automaton.resetStates();
      
      //Get the track candidates
      std::vector <Track*> autTrackCandidates = automaton.getTracks();
      
      trackCandidates = autTrackCandidates;
           
      /**********************************************************************************************/
      /*                Fitting                                                                     */
      /**********************************************************************************************/
 /*
      streamlog_out( MESSAGE0 ) << "\n--Fitting--\n" ;
      
      //first: empty the stored tracks (if there are any)
      _trackFitter.clearTracks();
      
      //then: fill in our trackCandidates:
      _trackFitter.addTracks( autTrackCandidates );
      
      //And get back fitted tracks
      std::vector <Track*> fittedTracks = _trackFitter.getFittedTracks();
  */
      
      /**********************************************************************************************/
      /*                Store the good tracks, delete the bad ones                                  */
      /**********************************************************************************************/
 /*     
      
      
      unsigned nTracksRejected = 0;
      unsigned nTracksKept = 0;
      
      for ( unsigned i=0; i < fittedTracks.size(); i++ ){ //over all fitted tracks

         Track* track = fittedTracks[i];
         // calculate the chi squared probability
         double chi2 = track->getChi2();
         int Ndf = track->getNdf();
         
         double chi2Prob = ROOT::Math::chisquared_cdf_c( chi2 , Ndf );
         
         
         streamlog_out( DEBUG2 ) << "\n Track " << i << " chi2Prob = " << chi2Prob 
                                 << "( chi2=" << chi2 <<", Ndf=" << Ndf << " )";

      
         if ( chi2Prob > 0.01 ){
            
            trackCandidates.push_back( track );
            nTracksKept++;
            
         }
         else{
            
            nTracksRejected++;
            
         }
                                 
      }      
   
   
      streamlog_out (MESSAGE0) << "\n Kept " <<  nTracksKept 
                             << " tracks with good chi2Prob, and rejected " << nTracksRejected << "\n";
      
      
      
                             
      for ( unsigned i=0; i< trackCandidates.size(); i++ ){
         
         std::vector < TrackerHit* > trackerHits = trackCandidates[i]->getTrackerHits();
         
         streamlog_out(MESSAGE0) << "\n\nTrack " << i << ":";
         
         for ( unsigned j=0; j < trackerHits.size(); j++ ){
            
            const double* pos = trackerHits[j]->getPosition();
            
            streamlog_out(MESSAGE0) << "\n( " << pos[0] << " , " << pos[1] << " , " << pos[2] << " )";
         
         }
         
      }
      
      
      
      */
      
      
      //finally: save the tracks

      LCCollectionVec * trkCol = new LCCollectionVec(LCIO::TRACK);
      for (unsigned int i=0; i < trackCandidates.size(); i++) trkCol->addElement( trackCandidates[i] );
      evt->addCollection(trkCol,_AutTrkCollection.c_str());
      

      
      
      
  }





        
    _nEvt ++ ;
}





void ForwardTracking::check( LCEvent * evt ) {}


void ForwardTracking::end(){}



