//
// Build a dictionary.
//
// $Id: classes.h,v 1.9 2012/02/28 22:26:01 gianipez Exp $
// $Author: gianipez $
// $Date: 2012/02/28 22:26:01 $
//
// Original author Rob Kutschke
//

#include <vector>

#include "art/Persistency/Common/Wrapper.h"

#include "RecoDataProducts/inc/StrawHitCollection.hh"
#include "RecoDataProducts/inc/StrawClusterCollection.hh"
#include "RecoDataProducts/inc/CaloHitCollection.hh"
#include "RecoDataProducts/inc/CaloCrystalHitCollection.hh"
#include "RecoDataProducts/inc/ExtMonUCITofHitCollection.hh"
#include "RecoDataProducts/inc/HoughCircleCollection.hh"
#include "RecoDataProducts/inc/SubEventCollection.hh"
#include "RecoDataProducts/inc/TrackerHitTimeClusterCollection.hh"
#include "RecoDataProducts/inc/SctrSttnClusterGroupCollection.hh"
#include "RecoDataProducts/inc/ZRotStrawHitMapCollection.hh"

//added by Gianipez
#include "RecoDataProducts/inc/CaloClusterCollection.hh"

// Cannot use the typedefs in here - not sure why.
template class art::Ptr<mu2e::CaloHit>;
template class std::vector<art::Ptr<mu2e::CaloHit> >;
template class art::Ptr<mu2e::ExtMonUCITofHit>;
template class std::vector<art::Ptr<mu2e::ExtMonUCITofHit> >;
template class art::Ptr<mu2e::StrawHit>;
template class std::vector<art::Ptr<mu2e::StrawHit> >;
template class art::Ptr<mu2e::TrackerHitTimeCluster>;
template class std::vector<art::Ptr<mu2e::TrackerHitTimeCluster> >;

//added by Gianipez
template class art::Ptr<mu2e::CaloCrystalHit>;
template class std::vector<art::Ptr<mu2e::CaloCrystalHit> >;
template class art::Ptr<mu2e::CaloCluster>;
template class std::vector<art::Ptr<mu2e::CaloCluster> >;



template class art::Wrapper<mu2e::StrawHitCollection>;
template class art::Wrapper<mu2e::StrawClusterCollection>;
template class art::Wrapper<mu2e::CaloHitCollection>;
template class art::Wrapper<mu2e::CaloCrystalHitCollection>;

template class art::Wrapper<mu2e::CaloClusterCollection>;

template class art::Wrapper<mu2e::ExtMonUCITofHitCollection>;
template class art::Wrapper<mu2e::HoughCircleCollection>;
template class art::Wrapper<mu2e::SubEventCollection>;
template class art::Wrapper<mu2e::TrackerHitTimeClusterCollection>;
template class art::Wrapper<mu2e::SctrSttnClusterGroupCollection>;
template class art::Wrapper<mu2e::ZRotStrawHitMapCollection>;
