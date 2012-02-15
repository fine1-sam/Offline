//
// Build a BFieldManager.
//
// $Id: BFieldManagerMaker.cc,v 1.25 2012/02/15 15:26:30 gandr Exp $
// $Author: gandr $
// $Date: 2012/02/15 15:26:30 $
//

// Includes from C++
#include <iostream>
#include <iomanip>
#include <fstream>
#include <set>

// Includes from C ( needed for block IO ).
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

// Includes from boost
#include <boost/regex.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>

// Framework includes
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "art/Utilities/Exception.h"
#include "cetlib/exception.h"

// Includes from Mu2e
#include "BFieldGeom/inc/BFieldManagerMaker.hh"
#include "BFieldGeom/inc/BFieldManager.hh"
#include "BFieldGeom/inc/DiskRecord.hh"
#include "Mu2eUtilities/inc/SimpleConfig.hh"
#include "GeneralUtilities/inc/MinMax.hh"

// CLHEP includes
#include "CLHEP/Units/SystemOfUnits.h"

using namespace std;

namespace mu2e {

  namespace {
    // Passing a string (not a const string&) potentially more efficient b/c of the copying
    std::string basename(std::string file) {
      std::string::size_type i = file.rfind('.');
      if(i != std::string::npos) {
        file.erase(i);
      }

      i = file.rfind('/');
      if(i != std::string::npos) {
        file.erase(0, i+1);
      }
      return file;
    }

    std::string strip_Mu2e(std::string file) {
      const string prefix("Mu2e_");
      if(!file.compare(0, prefix.length(), prefix)) {
        file.erase(0, prefix.length());
      }
      return file;
    }

  }

  //
  // Utility function which adds filter to decompress file if neccessary
  //
  void decompressFile(const string& filename, boost::iostreams::filtering_istream & in) {
    boost::regex re_gz("^.*\\.gz\\s*$");
    boost::regex re_bz("^.*\\.bz2\\s*$");
    if( boost::regex_match(filename.c_str(),re_gz) ) {
      in.push (boost::iostreams::gzip_decompressor());
    } else if( boost::regex_match(filename.c_str(),re_bz) ) {
      in.push (boost::iostreams::bzip2_decompressor());
    }
  }

  BFieldManagerMaker::BFieldManagerMaker( const SimpleConfig& config ):
    _resolveFullPath(),
    _config(config),
    _bfmgr(new BFieldManager())
  {

    _bfmgr->_xOffset =_config.getDouble("mu2e.solenoidOffset");

    string format = _config.getString("bfield.format","GMC");

    if( format=="GMC" ) {

      // These maps require torus radius of 2926 mm
      std::string torusName("toyTS.rTorus");
      const double torusRadius = 2926.0; // Required number
      if( fabs(_config.getDouble(torusName,0.0)-torusRadius)>0.1 ){
        throw cet::exception("GEOM")
          << "The GMC magnetic field files require torus radius of 2926 mm."
          << " Check " << torusName << " value in the config file."
          << " Maps are not loaded.\n";
      }

      _bfmgr->_type   = BFMapType::GMC;
      _bfmgr->_rTorus = torusRadius;

      // Add the DS, TS and PS field maps.
      loadGMC( "DS", "bfield.dsFile", "bfield.dsDimensions" );
      loadGMC( "TS", "bfield.tsFile", "bfield.tsDimensions" );
      loadGMC( "PS", "bfield.psFile", "bfield.psDimensions" );

      //throw cet::exception("GEOM") << "Temporal end." << "\n";

    } else if( format=="G4BL" ) {

      // These maps require torus radius of 2929 mm
      std::string torusName("toyTS.rTorus");
      const double torusRadius = 2929.0; // Required number
      if( fabs(_config.getDouble(torusName,0.0)-torusRadius)>0.1 ){
        throw cet::exception("GEOM")
          << "The G4BL magnetic field files require torus radius of 2929 mm."
          << " Check " << torusName << " value in the config file."
          << " Maps are not loaded.\n";
      }
      _bfmgr->_type   = BFMapType::G4BL;
      _bfmgr->_rTorus = torusRadius;

      // Read list of files with maps from the config file
      // The list can be specified directly with
      // bfield.files = { ... }
      // or using old style bfield.dsFile=... etc.

      vector<string> filesToLoad;
      if ( config.hasName("bfield.files") ){
        config.getVectorString("bfield.files",filesToLoad);
      } else {

        // This form is deprecated.
        vector<string> keysToLoad;
        keysToLoad.push_back("bfield.dsFile");
        keysToLoad.push_back("bfield.psFile");
        keysToLoad.push_back("bfield.tsuFile");
        keysToLoad.push_back("bfield.tsdFile");
        for( unsigned int i=0; i<keysToLoad.size(); ++i ) {
          if ( ! _config.hasName(keysToLoad[i]) ){
            cout << "No magnetic field file specified for: "
                 << keysToLoad[i]
                 << "   Hope that's OK." << endl;
          } else {
            filesToLoad.push_back(_config.getString(keysToLoad[i]));
          }
        }
      }

      bool writeBinaries = _config.getBool("bfield.writeG4BLBinaries", false);
      for( unsigned int i=0; i<filesToLoad.size(); ++i ) {
        string filename = filesToLoad[i];
        cout << "Read " << filename << endl;
        std::string mapkey = strip_Mu2e(basename(filename));
        loadG4BL( mapkey,  filename  );
        if( writeBinaries ){
          writeG4BLBinary( i, mapkey );
        }
      }

    } else {

      throw cet::exception("GEOM")
        << "Unknown format of file with magnetic field maps: " << format
        << "\n";

    }

    // For debug purposes: print the field in the target region
    CLHEP::Hep3Vector b = _bfmgr->getBField(CLHEP::Hep3Vector(3900.0,0.0,-6550.0));
    cout << "B-field at the proton target: ("
         << b.x() << ","
         << b.y() << ","
         << b.z() << ")"
         << endl;

    // Special case for the uniform DS field.
    loadUniformDS();

  }

  BFieldManagerMaker::~BFieldManagerMaker(){
  }

  // Parse the config file to learn about one magnetic field map.
  // Create an empty map and call the code to load the map from the file.
  void BFieldManagerMaker::loadGMC( const std::string& key,
                                    const std::string& fileKey,
                                    const std::string& dimensionKey ){

    if ( ! _config.hasName(fileKey) ){
      cout << "No magnetic field file specified for: "
           << fileKey
           << "   Hope that's OK." << endl;
      return;
    }

    // Get filename and expected dimensions.
    string filename = _config.getString(fileKey);
    vector<int> dim;
    _config.getVectorInt(dimensionKey,dim, 3);

    double scaleFactor(_config.getDouble("bfield.scaleFactor",1.0));

    // Create an empty map.
    BFMap& bfmap = _bfmgr->addBFMap( key,
                                     dim[0],
                                     dim[1],
                                     dim[2],
                                     BFMapType::GMC,
                                     scaleFactor );

    // Fill the map.
    readGMCMap( filename, bfmap );

    //dsmap.print(std::cout);
  }

  // Anonymous namespace to hold some helper functions
  namespace {

    // Helper function for parseG4BLHeader.
    void fillGrid ( boost::cmatch const&  matches,
                    std::vector<double>&  X0,
                    std::vector<int>&     dim,
                    std::vector<double>&  dX ){

      for( int i=1; i<=3; i++ ) {
        double value;
        istringstream sin(string(matches[i].first, matches[i].second));
        sin >> value;
        X0.push_back(value);
      }

      for( int i=4; i<=6; i++ ) {
        int value;
        istringstream sin(string(matches[i].first, matches[i].second));
        sin >> value;
        dim.push_back(value);
      }

      for( int i=7; i<=9; i++ ) {
        double value;
        istringstream sin(string(matches[i].first, matches[i].second));
        sin >> value;
        dX.push_back(value);
      }

    } // end fillGrid

    // Another helper function for parseG4BLHeader.
    void fillOffset( boost::cmatch const&  matches,
                     CLHEP::Hep3Vector&    offset ){

      for( int i=1; i<=3; i++ ) {
        double value;
        istringstream sin(string(matches[i].first, matches[i].second));
        sin >> value;
        offset(i-1) = value;
      }

    } // end fillOffset

    // Helper function for loadG4BL
    // Parse the header section of a G4BL format .txt or .header file.
    // First argument is an input argument, the others are output arguments.
    void parseG4BLHeader( std::string const&   path,
                          std::vector<double>& X0,
                          std::vector<int>&    dim,
                          std::vector<double>& dX,
                          CLHEP::Hep3Vector&   offset ){

      // The offset parameter is not present in files earlier than Mau7.
      // The value set here is the correct value for all G4BL format files earlier than Mau7.
      // For Mau7 and later, the file will contain a structured comment that contains the
      // correct value.  If the structured comment is present in the file, let it define
      // value of the offset;  if not, use the default given here.
      static const CLHEP::Hep3Vector offsetDefault(-3904.0,0.0,7929.0);

      // Open the input file.
      if( path.empty())
        throw "BFieldManagerMaker::loadG4BL: find_file failure!";  // TODO: improve exception
      ifstream fin(path.c_str());
      if ( !fin.is_open() ) {
        throw cet::exception("GEOM")
          << "Could not open file containing the magnetic field data. "
          << "Filename: "
          << path
          << "\n";
      }
      boost::iostreams::filtering_istream in;
      decompressFile(path,in);
      in.push(fin);

      // Regex to parse the string that contains the grid description
      boost::regex reGrid("^\\s*grid"
                          "\\s+X0=([eE\\d\\-\\+\\.]+)\\s+Y0=([eE\\d\\-\\+\\.]+)\\s+Z0=([eE\\d\\-\\+\\.]+)"
                          "\\s+nX=([eE\\d\\-\\+\\.]+)\\s+nY=([eE\\d\\-\\+\\.]+)\\s+nZ=([eE\\d\\-\\+\\.]+)"
                          "\\s+dX=([eE\\d\\-\\+\\.]+)\\s+dY=([eE\\d\\-\\+\\.]+)\\s+dZ=([eE\\d\\-\\+\\.]+)"
                          ".*$");

      // Regex to parse the string that contains the offset.
      boost::regex reOffset("^\\s*#\\s+Origin\\s+shift\\s+for\\s+Mu2e:"
                            "\\s+([eE\\d\\-\\+\\.]+)\\s+([eE\\d\\-\\+\\.]+)\\s+([eE\\d\\-\\+\\.]+)"
                            ".*$");

      // Search for the two lines of interest and, if present, extract their information.
      string cbuf;
      boost::cmatch matches;
      bool gridFound(false);
      bool offsetFound(false);
      int nread=100;           // Safety - don't read more than 100 lines
      while( (!in.eof()) && (--nread>0) ) {
        getline(in,cbuf);
        if( boost::regex_match(cbuf.c_str(),matches,reGrid) ) {
          gridFound = true;
          fillGrid( matches, X0, dim, dX);
        }
        if( boost::regex_match(cbuf.c_str(),matches,reOffset) ) {
          offsetFound = true;
          fillOffset( matches, offset );
        }
      }
      in.pop();
      fin.close();

      // The grid description must be present.
      if( ! gridFound ) {
        throw cet::exception("GEOM")
          << "Could not find param string in magnetic firld map. "
          << "Filename: " << path
          << ", found " << matches.size() << " items."
          << "\n";
      }

      // The offset description is optional and has a default.
      if ( !offsetFound ) offset = offsetDefault;

      // Adjust the lower bounds of the grid.
      X0[0] = X0[0] - offset.x();
      X0[1] = X0[1] - offset.y();
      X0[2] = X0[2] - offset.z();

  } // end parseHeader


  } // end anonymous namespace

  // Parse the config file to learn about one magnetic field map.
  // Create an empty map and call the code to load the map from the file.
  void BFieldManagerMaker::loadG4BL( const std::string& key,
                                     const std::string& filename ) {

    // Find the file under MU2E_SEARCH_PATH
    string path(_resolveFullPath(filename));

    // Extract information from the header.
    vector<double> X0;
    vector<int>    dim;
    vector<double> dX;
    CLHEP::Hep3Vector G4BL_offset;
    parseG4BLHeader( path, X0, dim, dX, G4BL_offset );

    // Extract information from the run-time configuration.
    double scaleFactor(_config.getDouble("bfield.scaleFactor",1.0));

    // Create an empty map.
    BFMap& dsmap = _bfmgr->addBFMap( key,
                                     dim[0],
                                     dim[1],
                                     dim[2],
                                     BFMapType::G4BL,
                                     scaleFactor );

    // Set defined region for the map
    dsmap.setLimits(X0[0],X0[0]+(dim[0]-1)*dX[0],
                    X0[1],X0[1]+(dim[1]-1)*dX[1],
                    X0[2],X0[2]+(dim[2]-1)*dX[2]);

    // Fill the map from the disk file.
    if ( filename.find(".header") != string::npos ) {
      readG4BLBinary( filename, dsmap );
    } else {
      readG4BLMap( filename, dsmap, G4BL_offset );
    }

    //dsmap.print(std::cout);

  }

  //
  // Read one magnetic field map file in MECO GMC format.
  //
  // This does a 2 pass operation"
  // 1) Pass 1:
  //      Read the input file into a temporary image in memory.
  //      Find the min and max values of the grid points.
  //      A the end of this pass, compute the grid spacing.
  // 2) Pass 2:
  //      Fill the 3D array from the image in memory.
  //

  void BFieldManagerMaker::readGMCMap( const string& filename,
                                       BFMap& bfmap ){

    // Open the input file.
    string path(_resolveFullPath(filename));
    if( path.empty())
      throw "BFieldManagerMaker::readGMCMap: find_file failure!";  // TODO: improve exception
    int fd = open( path.c_str(), O_RDONLY );
    if ( !fd ) {
      throw cet::exception("GEOM")
        << "Could not open file containing the magnetic filed map for: "
        << bfmap.getKey() << "\n"
        << "Filename: "
        << filename
        << "\n";
    }

    // Compute number of records in the input file.
    const int nrecords = computeArraySize(fd,filename);

    // Image of the file in memory.
    vector<DiskRecord> data(nrecords, DiskRecord());

    // Read file into memory.
    const int nbytes = nrecords*sizeof(DiskRecord);
    ssize_t s = read( fd, &data[0], nbytes );
    if ( s != nbytes ) {
      if ( s == -1 ){
        throw cet::exception("GEOM")
          << "Error reading magnetic field map: "
          << bfmap.getKey() << "\n"
          << "Filename: "
          << filename
          << "\n";
      } else{
        throw cet::exception("GEOM")
          << "Wrong number of bytes read from magnetic field map: "
          << bfmap.getKey() << "\n"
          << "Filename: "
          << filename
          << "\n";
      }
    }

    // Tool to find min and max values of grid points.
    MinMax mmX, mmY, mmZ;

    // Collect distinct values of (X,Y,Z) on the grid points.
    set<float> X, Y, Z;

    // Multiply by this factor to convert from kilogauss to tesla.
    double ratio = CLHEP::kilogauss/CLHEP::tesla;

    // For the image in memory:
    //   1) Transform into the correct set of units.
    //   2) Find min/max of each dimension.
    //   3) Collect unique values of (X,Y,Z) of the grid points.
    for ( vector<DiskRecord>::iterator i=data.begin();
          i != data.end(); ++i ){

      // Modify in place.
      DiskRecord& r = *i;

      // Unit conversion: from (cm, kG) to (mm,T).
      r.x  *= CLHEP::cm;
      r.y  *= CLHEP::cm;
      r.z  *= CLHEP::cm;
      r.bx *= ratio;
      r.by *= ratio;
      r.bz *= ratio;

      // The one check I can do.
      if ( r.head != r.tail ){
        throw cet::exception("GEOM")
          << "Error reading magnetic field map.  "
          << "Mismatched head and tail byte counts at record: " << data.size() << "\n"
          << "Could not open file containing the magnetic filed map for: "
          << bfmap.getKey() << "\n"
          << "Filename: "
          << filename
          << "\n";
      }

      // Update min/max information.
      mmX.accumulate(r.x);
      mmY.accumulate(r.y);
      mmZ.accumulate(r.z);

      // Populate the set of all unique grid values.
      X.insert(r.x);
      Y.insert(r.y);
      Z.insert(r.z);
    }

    // Expected grid dimentsions.
    const size_t nx = bfmap._nx;
    const size_t ny = bfmap._ny;
    const size_t nz = bfmap._nz;

    // Cross-check that the grid read from the file has the size we expected.
    // This is not really a perfect check since there could be round off error
    // in the computation of the grid points.  But the MECO GMC files were written
    // in a way that this test works.
    if ( X.size() != nx ||
         Y.size() != ny ||
         Z.size() != nz     ){
      throw cet::exception("GEOM")
        << "Mismatch in expected and observed number of grid points for BField map: "
        << bfmap.getKey() << "\n"
        << "From file: "
        << filename
        << "\n"
        << "Expected/Observed x: " << nx << " " << X.size() << "\n"
        << "Expected/Observed y: " << ny << " " << Y.size() << "\n"
        << "Expected/Observed z: " << nz << " " << Z.size() << "\n";
    }

    // Cross-check that we did not find more data than we have room for.
    if ( data.size() > nx*ny*nz-1){
      throw cet::exception("GEOM")
        << "Too many values read into the field map: "
        << bfmap.getKey() << "\n"
        << "From file: "
        << filename
        << "\n"
        << "Expected/Observed size: " << nx*ny*nz << " " << data.size() << "\n";
    }

    // Tell the magnetic field map what its limits are.
    bfmap.setLimits( mmX.min(), mmX.max(),
                     mmY.min(), mmY.max(),
                     mmZ.min(), mmZ.max() );

    // Store grid points and field values into 3D arrays
    for (vector<DiskRecord>::const_iterator i=data.begin(), e=data.end();
         i != e; ++i){

      DiskRecord const& r(*i);

      // Find indices corresponding to this grid point.
      // By construction the indices must be in bounds ( we set the limits above ).
      std::size_t ix = bfmap.iX(r.x);
      std::size_t iy = bfmap.iY(r.y);
      std::size_t iz = bfmap.iZ(r.z);

      // Store the information into the 3d arrays.
      bfmap._grid.set (ix, iy, iz, CLHEP::Hep3Vector(r.x,r.y,r.z));
      bfmap._field.set(ix, iy, iz, CLHEP::Hep3Vector(r.bx,r.by,r.bz));
      bfmap._isDefined.set(ix, iy, iz, true);

    }

    return;

  }

  //
  // Read one magnetic field map file in G4BL (TD) format.
  //
  void BFieldManagerMaker::readG4BLMap( const string& filename,
                                        BFMap& bfmap,
                                        CLHEP::Hep3Vector G4BL_offset ){


    // Open the input file.
    string path(_resolveFullPath(filename));
    if(path.empty())
      throw "BFieldManagerMaker::readG4BLMap: find_file failure!";  // TODO: improve exception
    ifstream fin(path.c_str());
    if ( !fin.is_open() )
      throw cet::exception("GEOM")<<"Could not open file "<<filename<<"\n";

    // Skip lines until "data" keyword
    boost::iostreams::filtering_istream in;
    decompressFile(filename,in);
    in.push(fin);
    std::string cbuf;
    boost::regex re("^\\s*data.*$");
    while( ! in.eof() ) {
      getline(in, cbuf);
      if( boost::regex_match(cbuf,re) ) break;
    }
    if( in.eof() ) throw cet::exception("GEOM")<<"Can't find data keyword in "<<filename<<"\n";

    // Expected grid dimentsions.
    const int nx = bfmap._nx;
    const int ny = bfmap._ny;
    const int nz = bfmap._nz;

    // Calculate expected number of lines to read
    const int nrecord = nx*ny*nz;

    // Read data
    double x[3], b[3];
    int nread = 0;
    while( (!in.eof()) && (nread<nrecord) ) {

      // Calculate indexes
      int ix = (nread/(ny*nz));
      int iy = (nread/nz)%ny;
      int iz = nread%nz;

      // Read and parse line
      nread++;
      getline(in, cbuf);
      istringstream sin(cbuf);
      if( (sin>>x[0]>>x[1]>>x[2]>>b[0]>>b[1]>>b[2]).fail() ) break;

      // Store the information into the 3d arrays.
      CLHEP::Hep3Vector pos = CLHEP::Hep3Vector(x[0],x[1],x[2])-G4BL_offset;
      bfmap._grid.set (ix, iy, iz, pos);
      bfmap._field.set(ix, iy, iz, CLHEP::Hep3Vector(b[0],b[1],b[2]));
      bfmap._isDefined.set(ix, iy, iz, true);

    }

    if( nread!=nrecord ) {
      throw cet::exception("GEOM")
        <<"Error while reading "<<filename<<"\n"
        <<"Read "<<nread<<" out of expected "<<nrecord<<" lines.\n"
        <<"Last line:\n"<<cbuf<<"\n";
    }

    return;

  }

  void BFieldManagerMaker::readG4BLBinary( const string& headerFilename,
                                           BFMap& bf ){

    // Form the name of the binary file from the name of the header file.
    string::size_type i = headerFilename.find(".header");
    if ( i == string::npos ){
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:readG4BLBinary Expected a file type of .header: "
        << headerFilename << "\n";
    }
    string binFilename = headerFilename.substr(0,i);
    binFilename += ".bin";

    // Resolve filename into a full path.
    string path(_resolveFullPath(binFilename));
    if( path.empty() )
      throw art::Exception(art::errors::FileOpenError)
        << "BFieldManagerMaker::readG4BLBinary: find_file failure: \n"
        << binFilename <<  "\n"
        << path << "\n";

    // Number of points in each big array.
    int nPoints = bf.nx()*bf.ny()*bf.nz();

    // Number of bytes in each big array.
    ssize_t nbytes = sizeof(CLHEP::Hep3Vector)*nPoints;

    // Open the binary file.
    int fd = open( path.c_str(), O_RDONLY );
    if ( fd < 0 ) {
      int errsave = errno;
      char* errmsg = strerror(errsave);
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:readG4BLBinary Error opening " << path
        << "  errno: " << errsave << " " << errmsg << "\n";
    }

    // A word to receive the endian marker.
    unsigned int marker(0);

    // Addresses of first elements in each of the two big arrays.
    CLHEP::Hep3Vector* gridAddr  = &bf._grid.get(0,0,0);
    CLHEP::Hep3Vector* fieldAddr = &bf._field.get(0,0,0);

    // Read the endian marker and check that it is OK.
    ssize_t s0 = read( fd, &marker, sizeof(unsigned int) );
    if ( s0 != sizeof(unsigned int) ){
      int errsave = errno;
      char* errmsg = strerror(errsave);
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:readG4BLBinary Error reading endian marker from " << path << "\n"
        << "Status: " << s0
        << "  errno: " << errsave << " " << errmsg << "\n";
    }

    static const unsigned int deadbeef(0XDEADBEEF);
    if ( marker != deadbeef ){
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:readG4BLBinary endian mismatch" << path
        << "  returned value: " << std:: hex << marker
        << "  expected value: " << deadbeef
        << std::dec << "\n"
        << "Suggestion: change from binary format field maps to the text or gzipped text format.\n"
        << "\n";
    }

    // Read the grid point positions.
    ssize_t s1 = read( fd, gridAddr, nbytes );
    if ( s1 != nbytes ){
      int errsave = errno;
      char* errmsg = strerror(errsave);
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:readG4BLBinary Error reading grid points from " << path << "\n"
        << "Status: " << s1
        << "  errno: " << errsave << " " << errmsg << "\n";
    }

    // Read the field values at the grid points.
    ssize_t s2 = read( fd, fieldAddr, nbytes );
    if ( s2 != nbytes ){
      int errsave = errno;
      char* errmsg = strerror(errsave);
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:readG4BLBinary Error reading field values from " << path << "\n"
        << "Status: " << s2
        << "  errno: " << errsave << " " << errmsg << "\n";
    }

    // These maps fill the full box so mark all grid points as valid.
    for ( int ix=0; ix<bf.nx(); ++ix ){
      for ( int iy=0; iy<bf.ny(); ++iy ){
        for ( int iz=0; iz<bf.nz(); ++iz ){
          bf._isDefined.set(ix,iy,iz,true);
        }
      }
    }

  } // end BFieldManagerMaker::readG4BLBinary

  void BFieldManagerMaker::writeG4BLBinary( unsigned int i, std::string const& key ){

    // Get the name of the output file.
    vector<string> outputfiles;
    _config.getVectorString("bfield.binaryFiles",outputfiles);
    string const& outputfile = outputfiles.at(i);

    // Get the map to be written out.
    BFMap const& bf = _bfmgr->getContainedMapByName(key);

    // Number of points in each big array.
    int nPoints = bf.nx()*bf.ny()*bf.nz();

    // Number of bytes in each big array.
    size_t nBytes = sizeof(CLHEP::Hep3Vector)*nPoints;

    cout << "Writing G4BL Magnetic field map in binary format: "
         << "map key: " << key
         << "file: " << outputfile
         << endl;

    // A marker to catch endian mismatch on readback.
    unsigned int deadbeef(0XDEADBEEF);

    // Addresses of first elements in each of the two big arrays.
    CLHEP::Hep3Vector const* gridAddr  = &bf._grid.get(0,0,0);
    CLHEP::Hep3Vector const* fieldAddr = &bf._field.get(0,0,0);

    // Open the output file.
    mode_t mode = S_IRUSR| S_IWUSR | S_IRGRP | S_IROTH;
    int flags   = O_CREAT|O_WRONLY|O_TRUNC|O_EXCL;
    int fd = open( outputfile.c_str(), flags, mode );
    int errsave = errno;

    // Check for errors.
    if ( fd < 0 ) {
      if ( errsave ==  EEXIST ){
        throw cet::exception("GEOM")
          << "BFieldManagerMaker:writeG4BLBinary Error opening " << outputfile
          << "  File already exists.\n";
      }
      char* errmsg = strerror(errsave);
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:writeG4BLBinary Error opening " << outputfile
        << "  errno: " << errsave << " " << errmsg << "\n";
    }

    // Write the endian marker and the big arrays.
    ssize_t s0 = write( fd, &deadbeef, sizeof(unsigned int) );
    if ( s0 == -1 ){
      errsave = errno;
      char* errmsg = strerror(errsave);
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:writeG4BLBinary Error writing endian marker to " << outputfile
        << "  errno: " << errsave << " " << errmsg << "\n";
    }
    ssize_t s1 = write( fd, gridAddr,  nBytes );
    if ( s1 == -1 ){
      errsave = errno;
      char* errmsg = strerror(errsave);
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:writeG4BLBinary  Error writing grid points to " << outputfile
        << "  errno: " << errsave << " " << errmsg << "\n";
    }
    ssize_t s2 = write( fd, fieldAddr, nBytes );
    if ( s2 == -1 ){
      errsave = errno;
      char* errmsg = strerror(errsave);
      throw cet::exception("GEOM")
        << "BFieldManagerMaker:writeG4BLBinary Error writing field values to " << outputfile
        << "  errno: " << errsave << " " << errmsg << "\n";
    }

    close(fd);

    cout << "Writing complete for map key: " << key << endl;

  }  // end BFieldManagerMaker::writeG4BLBinary

  // Compute the size of the array needed to hold the raw data of the field map.
  int BFieldManagerMaker::computeArraySize( int fd, const string& filename ){

    // Get the file size, in bytes, ( info.st_size ).
    struct stat info;
    fstat( fd, &info);

    // Check that an integral number of records fits in the file.
    int remainder = info.st_size % sizeof(DiskRecord);
    if ( remainder != 0 ){
      throw cet::exception("GEOM")
        << "Field map file does not hold an integral number of records: \n"
        << "Filename:  " << filename << "\n"
        << "Size:      " << info.st_size << "\n"
        << "Remainder: " << remainder << "\n";
    }

    // Compute number of records.
    int nrecords  = info.st_size / sizeof(DiskRecord);
    return nrecords;
  }

  void BFieldManagerMaker::loadUniformDS(){
    double bz = _config.getDouble("toyDS.bz", 0.);
    double scaleFactor(_config.getDouble("bfield.scaleFactor",1.0));

    _bfmgr->_dsUniformValue = CLHEP::Hep3Vector( 0., 0., bz*scaleFactor);

    double grad = _config.getDouble("toyDS.gradient", 0.);

    _bfmgr->_dsGradientValue = CLHEP::Hep3Vector( 0., 0., grad*scaleFactor);

  }

} // end namespace mu2e
