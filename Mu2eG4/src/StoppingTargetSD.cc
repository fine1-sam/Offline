//
// Define a sensitive detector for stopping target
// 
// Original author Ivan Logashenko
//

#include <cstdio>

// Framework includes
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Exception.h"

// Mu2e incldues
#include "Mu2eG4/inc/StoppingTargetSD.hh"
#include "Mu2eUtilities/inc/SimpleConfig.hh"

// G4 includes
#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"
#include "G4ios.hh"

using namespace std;

namespace mu2e {

  G4ThreeVector StoppingTargetSD::_mu2eOrigin;

  StoppingTargetSD::StoppingTargetSD(G4String name, const SimpleConfig& config):
    G4VSensitiveDetector(name),
    _collection(0),
    _debugList(0),
    _sizeLimit(config.getInt("g4.stepsSizeLimit",0)),
    _currentSize(0)
  {

    // Get list of events for which to make debug printout.
    string key("g4.virtualSDEventList");
    if ( config.hasName(key) ){
      vector<int> list;
      config.getVectorInt(key,list);
      _debugList.add(list);
    }

  }


  StoppingTargetSD::~StoppingTargetSD(){ }

  void StoppingTargetSD::Initialize(G4HCofThisEvent* HCE){

    _currentSize=0;

  }
  

  G4bool StoppingTargetSD::ProcessHits(G4Step* aStep,G4TouchableHistory*){

    _currentSize += 1;

    if( _sizeLimit>0 && _currentSize>_sizeLimit ) {
      if( (_currentSize - _sizeLimit)==1 ) {
	edm::LogWarning("G4") << "Maximum number of particles reached in StoppingTargetSD: " 
			      << _currentSize << endl;
      }
      return false;
    }

    //G4Event const* event = G4RunManager::GetRunManager()->GetCurrentEvent();
    //const G4TouchableHandle & touchableHandle = aStep->GetPreStepPoint()->GetTouchableHandle();
    //G4int eventId = event->GetEventID();
    //G4int trackId = aStep->GetTrack()->GetTrackID();
    //G4int copyNo = touchableHandle->GetCopyNumber();

    // The points coordinates are saved in the mu2e world

    // we add the hit to the framework collection
    _collection->
      push_back(StepPointMC(aStep->GetTrack()->GetTrackID(),
                            aStep->GetPreStepPoint()->GetTouchableHandle()->GetVolume()->GetCopyNo(),
                            aStep->GetTotalEnergyDeposit(),
                            aStep->GetPreStepPoint()->GetGlobalTime(),
                            aStep->GetPreStepPoint()->GetProperTime(),
                            aStep->GetPreStepPoint()->GetPosition() - _mu2eOrigin,
                            aStep->GetPreStepPoint()->GetMomentum(),
                            aStep->GetStepLength()
                            ));

    return true;

  }


  void StoppingTargetSD::EndOfEvent(G4HCofThisEvent*){

    if( _sizeLimit>0 && _currentSize>=_sizeLimit ) {
      edm::LogWarning("G4") << "Total of " << _currentSize 
			    << " stopping target hits were generated in the event." 
			    << endl
			    << "Only " << _sizeLimit << " are saved in output collection." 
			    << endl;
      cout << "Total of " << _currentSize 
	   << " stopping target hits were generated in the event." 
	   << endl
	   << "Only " << _sizeLimit << " are saved in output collection." 
	   << endl;
    }

    if (verboseLevel>0) { 
      G4int NbHits = _collection->size();
      G4cout << "\n-------->Hits Collection: in this event they are " << NbHits 
             << " hits in the stopping target: " << G4endl;
      for (G4int i=0;i<NbHits;i++) (*_collection)[i].print(G4cout);
    } 

  }


  void StoppingTargetSD::beforeG4Event(StepPointMCCollection& outputHits) {
    _collection = &outputHits;
    return;
  } // end of beforeG4Event


} //namespace mu2e
