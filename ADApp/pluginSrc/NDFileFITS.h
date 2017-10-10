/*
 * NDFileFITS.h
 * Writes NDArrays to FITS files.
 * Luca Cavalli - OHB italia S.p.A.
 * September 29, 2017
 */

#ifndef DRV_NDFileFITS_H
#define DRV_NDFileFITS_H

#include <fitsio.h>

#include "NDPluginFile.h"

/** Writes NDArrays in the FITS file format.
    Flexible Image Transport System is a file format used in astronomy endorsed by NASA and the International Astronomical Union.
    Used for the transport, analysis, and archival storage of scientific data sets
    - Multi-dimensional arrays: 1D spectra, 2D images, 3D+ data cubes
    - Tables containing rows and columns of information
    - Header keywords provide descriptive information about the data
    https://fits.gsfc.nasa.gov/
    */

class epicsShareClass NDFileFITS : public NDPluginFile {
public:
    NDFileFITS(const char *portName, int queueSize, int blockingCallbacks,
               const char *NDArrayPort, int NDArrayAddr,
               int priority, int stackSize);

    /* The methods that this class implements */
    virtual asynStatus openFile(const char *fileName, NDFileOpenMode_t openMode, NDArray *pArray);
    virtual asynStatus readFile(NDArray **pArray);
    virtual asynStatus writeFile(NDArray *pArray);
    virtual asynStatus closeFile();

private:
    fitsfile *pFits = NULL;
    NDAttributeList *pFileAttributes = NULL;
};

#endif
