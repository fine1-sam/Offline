//
// An EDAnalyzer module that reads back the stopped pi info generated by G4 
// and makes histograms.
//
// $Id: ReadStoppedPis_module.cc,v 1.7 2014/09/03 15:51:18 knoepfel Exp $
//
// Original author M Fischler
//

#include "CLHEP/Units/SystemOfUnits.h"
#include "GlobalConstantsService/inc/GlobalConstantsHandle.hh"
#include "GlobalConstantsService/inc/ParticleDataTable.hh"
#include "GeometryService/inc/GeomHandle.hh"
#include "GeometryService/inc/GeometryService.hh"
#include "GeometryService/inc/getTrackerOrThrow.hh"
#include "MCDataProducts/inc/GenParticleCollection.hh"
#include "MCDataProducts/inc/PhysicalVolumeInfoCollection.hh"
#include "MCDataProducts/inc/SimParticleCollection.hh"
#include "DataProducts/inc/PDGCode.hh"
#include "MCDataProducts/inc/StatusG4.hh"
#include "MCDataProducts/inc/StatusG4.hh"
#include "MCDataProducts/inc/StepPointMCCollection.hh"
#include "MCDataProducts/inc/PtrStepPointMCVectorCollection.hh"
#include "TDirectory.h"
#include "TGraph.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TNtuple.h"
#include "TTrackerGeom/inc/TTracker.hh"
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Principal/Handle.h"
#include "cetlib/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include <cmath>
#include <iostream>
#include <string>

using namespace std;

using CLHEP::Hep3Vector;
using CLHEP::keV;

namespace mu2e {

  class ReadStoppedPis : public art::EDAnalyzer {
  public:

    explicit ReadStoppedPis(fhicl::ParameterSet const& pset);
    virtual ~ReadStoppedPis() { }

    virtual void beginJob();
    virtual void endJob();

    // This is called for each event.
    virtual void analyze(const art::Event& e);

  private:

    // Start: run time parameters

    // Diagnostics printout level
    int _diagLevel;

    int _sampleMax;

    // Module label of the g4 module that made the hits
    //   The hits going to the stopping target
    std::string _g4ModuleLabel;
    //   The hits from VD5
    std::string _g4VD5ModuleLabel;

    // Module label of the generator module that was passed as input to G4.
    std::string _generatorModuleLabel;

   // Name of the stopping target StepPoint collection
    std::string _targetStepPoints;

    // Cut on the minimum energy.
    double _minimumEnergy;

    // Limit on number of events for which there will be full printout.
    int _maxFullPrint;

    // End: run time parameters

    // Number of events analyzed.
    int _nAnalyzed;

    // Control of which particles to put into the ntuple
    bool _pionsOnly;
    bool _primariesOnly;

    // Pointers to histograms, ntuples, TGraphs.

    TH1F* _hStoppedVolume;
    TH1F* _hStoppedZ;
    TH1F* _hStoppedT;
    TH1F* _hStoppedTau;
    TNtuple* _pionTargNtup;
    TH2F* _hXYpions2D;
    
    // Other class variables
    int _nBadG4Status;
    int _nBadSimsAtTargetHandle;
    
    // Do the work specific to the stopping target.
    void doStoppingTarget(const art::Event& event);

    // A helper function.
    int countHitNeighbours( Straw const& straw,
                            art::Handle<StepPointMCCollection>& hits );

  };
  ReadStoppedPis::ReadStoppedPis(fhicl::ParameterSet const& pset) :
    art::EDAnalyzer(pset),

    // Run time parameters
    _diagLevel(pset.get<int>("diagLevel",0)),
    _sampleMax(pset.get<int>("sampleMax",0)),
    _g4ModuleLabel(pset.get<string>("g4ModuleLabel")),
    _generatorModuleLabel(pset.get<string>("generatorModuleLabel")),
    _targetStepPoints(pset.get<string>("targetStepPoints","stoppingtarget")),
    _minimumEnergy(pset.get<double>("minimumEnergy")),
    _maxFullPrint(pset.get<int>("maxFullPrint",5)),
    _pionsOnly(pset.get<bool>("pionsOnly",false)),
    _primariesOnly(pset.get<bool>("primariesOnly",false)),
    // Histograms
    _hStoppedVolume(0),
    _hStoppedZ(0),
    _hStoppedT(0),
    _hStoppedTau(0),
    _pionTargNtup(0),
    _hXYpions2D(0),
    // Remaining member data
    _nBadG4Status(0),
    _nBadSimsAtTargetHandle(0)
  {
        std::cout << " \n\n_diaglevel = " << _diagLevel << "\n\n";

  }

  std::string pionTargetNtupleDescriptor() {
    std::string d;
    d = "evt:";         //   0  event.id()
// Information directly in SimParticle
    d += "pdgId:";      //   1  pdgID()
    d += "prm:";        //   2  isPrimary()
    d += "sec:";        //   3  isSecondary()
    d += "endX:";       //   4  endPosition().x()  
    d += "endY:";       //   5  endPosition().y()  
    d += "endZ:";       //   6  endPosition().z()  
    d += "endPx:";      //   7  endMomentum().x()  
    d += "endPy:";      //   8  endMomentum().y()  
    d += "endPz:";      //   9  endMomentum().z()  
    d += "endE:";       //  10  endMomentum().t()  
    d += "endT:";       //  11  endGlobalTime()  
    d += "endTau:";     //  12  endProperTime()  
    d += "endVol:";     //  13  endVolumeIndex()  
    d += "endG4stat:";  //  14  endG4Status()  
    d += "stopCode:";   //  15  stoppingCode()  
    d += "startX:";     //  16  startPosition().x()  
    d += "startY:";     //  17  startPosition().y()  
    d += "startZ:";     //  18  startPosition().z()  
    d += "startPx:";    //  19  startMomentum().x()  
    d += "startPy:";    //  20  startMomentum().y()  
    d += "startPz:";    //  21  startMomentum().z()  
    d += "startE:";     //  22  startMomentum().t()  
    d += "startT:";     //  23  startGlobalTime()  
    d += "startTau:";   //  24  startProperTime()  
    d += "startVol:";   //  25  startVolumeIndex()  
    d += "startG4stat:";//  26  startG4Status()  
    d += "createCode:"; //  27  creationCode()  
    d += "plsE:";       //  28  preLastStepKineticEnergy()  
    d += "nsteps:";     //  29  nSteps()  
    d += "weight:";     //  30  weight()  
    d += "endDef";      //  31  endDefined()  
// Derived information
// Planned (maybe) --
//    d += "startX:";     //   0  startPosition().x()  primary pions only 
//    d += "startY:";     //   0  startPosition().y()  primary pions only 
//    d += "startZ:";     //   0  startPosition().z()  primary pions only 
   
// When adding to this list, don't forget:
//      The last item should NOT have the ending colon :
//      The size of the nt array is automatically adjusted, but...
//      The numbers are NOT automatically kept in synch.  That is why the
//      numbers are commented here.  Keep this correct, and use it.
      
    return d;
  }
  void ReadStoppedPis::beginJob(){

    // Get access to the TFile service.
    art::ServiceHandle<art::TFileService> tfs;

    // Create target pions ntuple.
    _pionTargNtup = tfs->make<TNtuple> ( 
        "piontargntup", 
        "Pion target ntuple",
         pionTargetNtupleDescriptor().c_str()  );

    // Create stopping target histograms
     _hStoppedVolume = tfs->make<TH1F>( "hStoppedVolume",
                                    "Volume in which pion stopped",
                                    100, 425., 450. );
    _hStoppedZ = tfs->make<TH1F>( "_hStoppedZ",
                                          "Z of stopped pion",
                                          100, 5400., 6400. );
    _hStoppedT = tfs->make<TH1F>( "_hStoppedT",
                                      "pion stopping t",
                                      100, 0., 1000. );
    _hStoppedTau = tfs->make<TH1F>( "_hStoppedTau",
                                      "pion stopping tau",
                                      100, 0., 500. );
    _hXYpions2D = tfs->make<TH2F>( "_hXYpions2D",
                                        "XY of stopped pions",
                                        20, -4000., -3800., 20, -100., 100. );
  }

  void ReadStoppedPis::analyze(const art::Event& event) {

    mf::LogPrint ("newEvent") << "FilterStoppedPis begin event " <<event.id();

    // Maintain a counter for number of events seen.
    ++_nAnalyzed;

    // Inquire about the completion status of G4.
    art::Handle<StatusG4> g4StatusHandle;
    event.getByLabel( _g4ModuleLabel, g4StatusHandle);
    StatusG4 const& g4Status = *g4StatusHandle;
    if ( _nAnalyzed < _maxFullPrint ){
      cerr << g4Status << endl;
    }

    // Abort if G4 did not complete correctly.
    // Use your own judgement about whether to abort or to continue.
    if ( g4Status.status() > 1 ) {
      ++_nBadG4Status;
      mf::LogError("G4")
        << "Aborting ReadStoppedPis::analyze due to G4 status\n"
        << g4Status;
      return;
    }

    doStoppingTarget(event);

  }

static void 
outputSimInfo ( std::ostream & os, const art::Event& event,
 SimParticle const & sim, 
 art::Handle<PhysicalVolumeInfoCollection> const & volumes) 
{      
  os << " Event " << event.id() << "  ";
  if ( sim.creationCode() != ProcessCode::mu2ePrimary ) {
    os << sim.parent()->id();
  }
  os << " --> simParticle " << sim.id() << " ";
  std::string pn =  HepPID::particleName( sim.pdgId() );
  if (pn == "not defined") {
    os << sim.pdgId() << "  ";
  } else {
    os << HepPID::particleName( sim.pdgId() ) << "  ";
  }
  os << sim.startGlobalTime() << "-" << sim.endGlobalTime() << "\n" 
            << "        " << sim.endPosition() << "  "
            << sim.creationCode().name() << " --> "
            << sim.stoppingCode().name() << "\n";
  os << sim.startVolumeIndex() << " " 
            << volumes->at(sim.startVolumeIndex()).name() << " -- "
            << sim.endVolumeIndex() << " " 
            << volumes->at( sim.endVolumeIndex()).name() << "\n";
}

  //
  // Reading information about stopping target
  //
  // Here we assume that the primary particles are the pions
  // saved at VD5.
  //
  void ReadStoppedPis::doStoppingTarget(const art::Event& event) {

    // Find original G4 steps in the stopping target
    art::Handle<StepPointMCCollection> sthits;
    event.getByLabel(_g4ModuleLabel,_targetStepPoints,sthits);

    // Find original SimParticles in the event -- 
    // not just those in the stopping target necessarily
    art::Handle<SimParticleCollection> sims_handle;
    event.getByLabel(_g4ModuleLabel,sims_handle);

    if ( sims_handle->empty() ) {
      mf::LogPrint("empty") << "sims_handle->empty() for event " << event.id();
      return;
    }

    // Prepare to look at volumes
    art::Handle<PhysicalVolumeInfoCollection> volumes;
    event.getRun().getByLabel(_g4ModuleLabel,volumes);

    // Prepare the array for the ntuple
    float nt[_pionTargNtup->GetNvar()];  // Automatic array size adjustment

    int sampleCount = 0;
    bool atLeastOneStoppedPion = false;
    
    // Loop over Simparticles in the event.
    SimParticleCollection::const_iterator i = sims_handle->begin();
    SimParticleCollection::const_iterator e = sims_handle->end();

    for ( ; i!=e; ++i ) {
      // Alias, used for readability. 
      SimParticle const & sim = i->second;
      if (sampleCount++ < _sampleMax) 
          outputSimInfo ( std::cout, event, sim, volumes );
      bool sim_is_stopped_pi = false;
      bool primary_is_as_expected = true;   
      if ( sim.creationCode() == ProcessCode::mu2ePrimary ) {
        // Check that this primary is a pi minus that stopped in target:
        if (   ( sim.pdgId() != PDGCode::pi_minus )
            || ( sim.stoppingCode() != ProcessCode::CHIPSNuclearCaptureAtRest )
            || ( volumes->at(sim.endVolumeIndex()).name().compare(0,11,"TargetFoil_") != 0 )) { 
          primary_is_as_expected = false;
        }     
        if ( primary_is_as_expected ) {
          sim_is_stopped_pi = true;
        } else {
          // std::cout << "\n???? " << event.id() 
          //          << "Primary is not a pion that stopped in target!\n";
        }
        if (sim_is_stopped_pi) {
          atLeastOneStoppedPion = true;
          nt[0]  = event.id().event();
          nt[1]  = sim.pdgId();
          nt[2]  = sim.isPrimary();
          nt[3]  = sim.isSecondary();
          CLHEP::Hep3Vector stopPos = sim.endPosition();
          nt[4]  = stopPos.x();
          nt[5]  = stopPos.y();
          nt[6]  = stopPos.z();
          CLHEP::HepLorentzVector stopMom =  sim.endMomentum();      
          nt[7]  = stopMom.x();
          nt[8]  = stopMom.y();
          nt[9]  = stopMom.z();
          nt[10]  = stopMom.t();
          nt[11]  = sim.endGlobalTime();
          nt[12]  = sim.endProperTime();
          nt[13]  = sim.endVolumeIndex();
          nt[14] = sim.endG4Status();
          nt[15] = sim.stoppingCode();
          CLHEP::Hep3Vector startPos = sim.startPosition();
          nt[16] = startPos.x();
          nt[17] = startPos.y();
          nt[18] = startPos.z();
          CLHEP::HepLorentzVector startMom =  sim.startMomentum();      
          nt[19]  = startMom.x();
          nt[20]  = startMom.y();
          nt[21]  = startMom.z();
          nt[22]  = startMom.t();
          nt[23] = sim.startGlobalTime();
          nt[24] = sim.startProperTime();
          nt[25] = sim.startVolumeIndex();
          nt[26] = sim.startG4Status();
          nt[27] = sim.creationCode();
          nt[28] = sim.preLastStepKineticEnergy();
          nt[29] = sim.nSteps();
          nt[30] = sim.weight();
          nt[31] = sim.endDefined();
          _pionTargNtup->Fill(nt);
          // Fill histograms
          double t = sim.endGlobalTime();
          _hStoppedVolume->Fill(sim.endVolumeIndex());
          _hStoppedZ->Fill(stopPos.z());
          _hStoppedT->Fill(t);
          _hStoppedTau->Fill(sim.endProperTime());
          _hXYpions2D->Fill(stopPos.x(),stopPos.y());
        }
      } // end of if primary
    } // end of for loop over sims  
    if ( !atLeastOneStoppedPion ) {
      std::cout << "Event << " << event.id() << " had no stopped pions\n";
    }

  } // end doStoppingTarget

  void ReadStoppedPis::endJob(){
    cout << "ReadStoppedPis::endJob Number of events skipped "
         << "due to G4 completion status: "
         << _nBadG4Status
         << endl;
  }


}  // end namespace mu2e

DEFINE_ART_MODULE(mu2e::ReadStoppedPis);
