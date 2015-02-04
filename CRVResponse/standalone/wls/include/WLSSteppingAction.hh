#ifndef WLSSteppingAction_h
#define WLSSteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "G4VPhysicalVolume.hh"
#include <vector>
#include <map>

class TFile;
class TH3D;
class MakeCrvPhotonArrivals;

class WLSSteppingAction : public G4UserSteppingAction
{
  public:

    WLSSteppingAction(int mode, const std::string &lookupFileName = "");
    ~WLSSteppingAction();

    void                      UserSteppingAction(const G4Step*);
    static WLSSteppingAction* Instance() {return _fgInstance;}
    void                      Reset();
    const std::vector<double> &GetArrivalTimes(int i, int SiPM);
    const std::vector<int>    &GetFiberEmissions(int SiPM);
    void                      SetScintillationYield(double scintillationYield) {_scintillationYield=scintillationYield;}
    void                      SetScintillatorDecayTimeFast(double decayTime) {_scintillatorDecayTimeFast=decayTime;}
    void                      SetScintillatorDecayTimeSlow(double decayTime) {_scintillatorDecayTimeSlow=decayTime;}
    void                      SetFiberDecayTime(double decayTime) {_fiberDecayTime=decayTime;}

  private:

    static WLSSteppingAction *_fgInstance;  
    MakeCrvPhotonArrivals    *_crvPhotonArrivals;
    std::vector<double>       _arrivalTimes[2][4];
    std::vector<int>          _fiberEmissions[4];
    double                    _scintillationYield;
    double                    _scintillatorDecayTimeFast, _scintillatorDecayTimeSlow; 
    double                    _fiberDecayTime;
    int                       _mode;

    std::map<int,int>         _wlsTracks;
};

#endif
