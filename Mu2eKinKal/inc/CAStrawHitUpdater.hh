//
// Simple updater of StrawHits based on box cuts of Closest Approach (CA) information.  This mimics BTrk
//
#ifndef Mu2eKinKal_CAStrawHitUpdater_hh
#define Mu2eKinKal_CAStrawHitUpdater_hh
#include "KinKal/Trajectory/ClosestApproachData.hh"
#include "Offline/Mu2eKinKal/inc/WireHitState.hh"
#include "Offline/Mu2eKinKal/inc/WHSMask.hh"
#include "Offline/Mu2eKinKal/inc/DriftInfo.hh"
#include "Offline/Mu2eKinKal/inc/StrawHitUpdaters.hh"
#include <tuple>
#include <string>
#include <iostream>

namespace mu2e {
  // Update based just on PTCA to the wire
  class CAStrawHitUpdater {
    public:
      using CASHUConfig = std::tuple<float,float,float,std::string>;
      CAStrawHitUpdater() : maxdoca_(0), minrdrift_(0), maxrdrift_(0) {}
      CAStrawHitUpdater(CASHUConfig const& cashuconfig);
      static std::string const& configDescription(); // description of the variables
      // set the state based on the current PTCA value
      WireHitState wireHitState(WireHitState const& input, KinKal::ClosestApproachData const& tpdata,DriftInfo const& dinfo) const;
    private:
      double maxdoca_; // maximum DOCA to use hit
      double minrdrift_; // minimum rdrift to use drift information
      double maxrdrift_; // maximum rdrift to use hit
      WHSMask freeze_; // states to freeze
  };
}
#endif
