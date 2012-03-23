#ifndef ForwardTracking_h
#define ForwardTracking_h 1

#include <string>

#include "marlin/Processor.h"
#include "lcio.h"
#include "EVENT/TrackerHit.h"
#include "EVENT/Track.h"
#include "MarlinTrk/IMarlinTrkSystem.h"
#include "gear/BField.h"

#include "Segment.h"
#include "Criteria.h"
#include "SectorSystemFTD.h"

using namespace lcio ;
using namespace marlin ;
using namespace FTrack;

/** a simple typedef, making writing shorter. And it makes sense: a track consists of hits. But as a real track
 * has more information, a vector of hits can be considered as a "raw track". */
typedef std::vector< IHit* > RawTrack;


/**  Standallone Forward Tracking Processor for Marlin.
 * 
 * 
 *  <h4>Input - Prerequisites</h4>
 *  The hits in the FTDs
 *
 *  <h4>Output</h4> 
 *  A collection of Tracks.
 * 
 * @param FTDHitCollections The collections containing the FTD hits <br>
 * (default value "FTDTrackerHits FTDSpacePoints" (string vector) ) <br>
 * 
 * @param ForwardTrackCollection Name of the Forward Tracking output collection
 * (default value  "ForwardTracks" (output collection) )
 * 
 * @param MultipleScatteringOn Whether to take multiple scattering into account when fitting the tracks
 * (default value true )
 * 
 * @param EnergyLossOn Whether to take energyloss into account when fitting the tracks
 * (default value true )
 * 
 * @param SmoothOn Whether to smooth all measurement sites in fit
 * (default value false )
 * 
 * @param Chi2ProbCut Tracks with a chi2 probability below this will get sorted out
 * (default value 0.005 )
 * 
 * @param OverlappingHitsDistMax The maximum distance of hits from overlapping petals belonging to one track
 * (default value 3.5 )
 * 
 * @param HitsPerTrackMin The minimum number of hits to create a track
 * (default value 3 )
 * 
 * @param BestSubsetFinder The method used to find the best non overlapping subset of tracks. Available are: TrackSubsetHopfieldNN and TrackSubsetSimple.
 * Any other value means, that no final search for the best subset is done and overlapping tracks are possible. (If you want that, don't
 * leave the string empty or the default value will be used!)
 * (default value TrackSubsetHopfieldNN )
 * 
 * @param Criteria A vector of the criteria that are going to be used. For every criterion a min and max needs to be set!!!
 * (default value is defined in class Criteria )
 * 
 * @param NameOfACriterion_min/max For every used criterion a minimum and maximum value needs to be set. If a criterion is
 * named "Crit_Example", then the min parameter would be: Crit_Example_min and the max parameter Crit_Example_max.
 * 
 * @author Robin Glattauer HEPHY, Wien
 *
 */






class ForwardTracking : public Processor {
  
 public:
  
  virtual Processor*  newProcessor() { return new ForwardTracking ; }
  
  
  ForwardTracking() ;
  
  /** Called at the begin of the job before anything is read.
   * Use to initialize the processor, e.g. book histograms.
   */
  virtual void init() ;
  
  /** Called for every run.
   */
  virtual void processRunHeader( LCRunHeader* run ) ;
  
  /** Called for every event - the working horse.
   */
  virtual void processEvent( LCEvent * evt ) ; 
  
  
  virtual void check( LCEvent * evt ) ; 
  
  
  /** Called after data processing for clean up.
   */
  virtual void end() ;
  

  
  
 protected:
    

    
   /** Input collection names.
   */
   std::vector<std::string> _FTDHitCollections;
   
   /** Output collection name.
   */
   std::string _ForwardTrackCollection;


   int _nRun ;
   int _nEvt ;


   double _Bz; //B field in z direction


   double _chi2ProbCut;



   bool _MSOn ;
   bool _ElossOn ;
   bool _SmoothOn ;
   
   
   
   /** A map to store the hits according to their sectors */
   std::map< int , std::vector< IHit* > > _map_sector_hits;
   
   /**
    *  @return a map that links hits with overlapping hits on the petals behind
    */
   std::map< IHit* , std::vector< IHit* > > getOverlapConnectionMap( 
            std::map< int , std::vector< IHit* > > & map_sector_hits, 
            const SectorSystemFTD* secSysFTD,
            float distMax);

   /** Adds hits from overlapping areas to a RawTrack in every possible combination.
    *
    * @return all of the resulting RawTracks
    * 
    * @param rawTrack a RawTrack (vector of IHit* ), we want to add hits from overlapping regions
    * 
    * @param map_hitFront_hitsBack a map, where IHit* are the keys and the values are vectors of hits that
    * are in an overlapping region behind them. (TODO: what this function does is actually more general, maybe rename)
    */
   std::vector < RawTrack > getRawTracksPlusOverlappingHits( RawTrack rawTrack , std::map< IHit* , std::vector< IHit* > >& map_hitFront_hitsBack );
      
   
   std::vector< std::string > _criteriaNames;
   std::map< std::string , float > _critMinima;
   std::map< std::string , float > _critMaxima;
   
   int _hitsPerTrackMin;
   
   std::vector <ICriterion*> _crit2Vec;
   std::vector <ICriterion*> _crit3Vec;
   std::vector <ICriterion*> _crit4Vec;
    
   const SectorSystemFTD* _sectorSystemFTD;
   
   bool _useCED;
   
   double _overlappingHitsDistMax;
   
   bool _takeBestVersionOfTrack;
   
   std::string _bestSubsetFinder;
   
   
   /** @return Info on the content of _map_sector_hits */
   std::string getInfo_map_sector_hits();
   
   MarlinTrk::IMarlinTrkSystem* _trkSystem;

   
} ;

#endif



