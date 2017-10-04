/*
 * NDFileFITS.h
 * Writes NDArrays to FITS files.
 * Luca Cavalli - OHB italia S.p.A.
 * September 29, 2017
 */

#include <epicsTypes.h>
#include <epicsMessageQueue.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsTime.h>
#include <iocsh.h>
#include <epicsExport.h>

#include <asynDriver.h>

#include "NDFileFITS.h"

static const char *driverName = "NDFileFITS";

/** Opens a FITS file.
  * \param[in] fileName The name of the file to open.
  * \param[in] openMode Mask defining how the file should be opened; bits are 
  *            NDFileModeRead, NDFileModeWrite, NDFileModeAppend, NDFileModeMultiple
  * \param[in] pArray A pointer to an NDArray; this is used to determine the array and attribute properties.
  */
asynStatus NDFileFITS::openFile(const char *fileName, NDFileOpenMode_t openMode, NDArray *pArray)
{
    static const char *functionName = "openFile";
    int bitpix = BYTE_IMG;
    int naxis = 0;
    long *naxes = NULL;
    int status = 0;

    /* We don't support reading yet */
    if (openMode & NDFileModeRead) return (asynError);

    /* We don't support opening an existing file for appending yet */
    if (openMode & NDFileModeAppend) return (asynError);

    /* Create empty FITS file */
    fits_create_file(&(this->pFits), fileName, &status);

    if (status > 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s error, fits_create_file failed. file: %s\n",
            driverName, functionName, fileName);
        return (asynError);
    }

    naxis = pArray->ndims;
    if (naxis == 0) return (asynError);

    /* FIXME, use std:: stuff */
    naxes = (long*) malloc(naxis * sizeof(long));
    if (naxes == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s error, naxes malloc failed. file: %s\n",
            driverName, functionName, fileName);
        return (asynError);
    }

    for (int i = 0; i < naxis; i++) {
        naxes[i] = pArray->dims[i].size;
    }

    switch (pArray->dataType) {
        case NDInt8:
            bitpix = SBYTE_IMG;
            break;
        case NDUInt8:
            bitpix = BYTE_IMG;
            break;
        case NDInt16:
            bitpix = SHORT_IMG;
            break;
        case NDUInt16:
            bitpix = USHORT_IMG;
            break;
        case NDInt32:
            bitpix = LONG_IMG;
            break;
        case NDUInt32:
            bitpix = ULONG_IMG;
            break;
        case NDFloat32:
            bitpix = FLOAT_IMG;
            break;
        case NDFloat64:
            bitpix = DOUBLE_IMG;
            break;
        default:
            return (asynError);
            break;
    }

    /* Create empty image */
    fits_create_img(this->pFits, bitpix, naxis, naxes, &status);

    free(naxes);

    return (asynSuccess);
}

/** Writes single NDArray to the FITS file.
  * \param[in] pArray Pointer to the NDArray to be written
  */
asynStatus NDFileFITS::writeFile(NDArray *pArray)
{
    static const char *functionName = "writeFile";
    int dataType = 0;
    unsigned long nElements = 1;
    int status = 0;

    for (int i = 0; i < pArray->ndims; i++) {
        nElements *= pArray->dims[i].size;
    }

    switch (pArray->dataType) {
        case NDInt8:
            dataType = TSBYTE;
            break;
        case NDUInt8:
            dataType = TBYTE;
            break;
        case NDInt16:
            dataType = TSHORT;
            break;
        case NDUInt16:
            dataType = TUSHORT;
            break;
        case NDInt32:
            dataType = TLONG;
            break;
        case NDUInt32:
            dataType = TULONG;
            break;
        case NDFloat32:
            dataType = TFLOAT;
            break;
        case NDFloat64:
            dataType = TDOUBLE;
            break;
        default:
            return (asynError);
            break;
    }

    /* Write the image */
    fits_write_img(this->pFits, dataType, 1, nElements, pArray->pData, &status);

    if (status > 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s error, fits_write_img failed.\n",
            driverName, functionName);
        return (asynError);
    }

    return (asynSuccess);
}

/** Reads single NDArray from a FITS file; NOT CURRENTLY IMPLEMENTED.
  * \param[in] pArray Pointer to the NDArray to be read
  */
asynStatus NDFileFITS::readFile(NDArray **pArray)
{
    static const char *functionName = "readFile";

    return (asynError);
}

/** Closes the FITS file. */
asynStatus NDFileFITS::closeFile()
{
    static const char *functionName = "closeFile";
    int status = 0;

    fits_close_file(this->pFits, &status);
    this->pFits = NULL;

    return (asynSuccess);
}

/** Constructor for NDFileFITS; all parameters are simply passed to NDPluginFile::NDPluginFile.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] queueSize The number of NDArrays that the input queue for this plugin can hold when 
  *            NDPluginDriverBlockingCallbacks=0.  Larger queues can decrease the number of dropped arrays,
  *            at the expense of more NDArray buffers being allocated from the underlying driver's NDArrayPool.
  * \param[in] blockingCallbacks Initial setting for the NDPluginDriverBlockingCallbacks flag.
  *            0=callbacks are queued and executed by the callback thread; 1 callbacks execute in the thread
  *            of the driver doing the callbacks.
  * \param[in] NDArrayPort Name of asyn port driver for initial source of NDArray callbacks.
  * \param[in] NDArrayAddr asyn port driver address for initial source of NDArray callbacks.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
NDFileFITS::NDFileFITS(const char *portName, int queueSize, int blockingCallbacks,
                       const char *NDArrayPort, int NDArrayAddr,
                       int priority, int stackSize)
    /* Invoke the base class constructor.
     * We allocate 2 NDArrays of unlimited size in the NDArray pool.
     * This driver can block (because writing a file can be slow), and it is not multi-device.  
     * Set autoconnect to 1.  priority and stacksize can be 0, which will use defaults. */
    : NDPluginFile(portName, queueSize, blockingCallbacks,
                   NDArrayPort, NDArrayAddr, 1,
                   2, 0, asynGenericPointerMask, asynGenericPointerMask, 
                   ASYN_CANBLOCK, 1, priority, stackSize, 1)
{
    /* Set the plugin type string */    
    setStringParam(NDPluginDriverPluginType, "NDFileFITS");
    this->supportsMultipleArrays = 0;
}

/* Configuration routine.  Called directly, or from the iocsh  */

extern "C" int NDFileFITSConfigure(const char *portName, int queueSize, int blockingCallbacks,
                                   const char *NDArrayPort, int NDArrayAddr,
                                   int priority, int stackSize)
{
    NDFileFITS *pPlugin = new NDFileFITS(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr,
                                         priority, stackSize);
    return (pPlugin->start());
}


/* EPICS iocsh shell commands */

static const iocshArg initArg0 = { "portName",iocshArgString};
static const iocshArg initArg1 = { "frame queue size",iocshArgInt};
static const iocshArg initArg2 = { "blocking callbacks",iocshArgInt};
static const iocshArg initArg3 = { "NDArray Port",iocshArgString};
static const iocshArg initArg4 = { "NDArray Addr",iocshArgInt};
static const iocshArg initArg5 = { "priority",iocshArgInt};
static const iocshArg initArg6 = { "stack size",iocshArgInt};
static const iocshArg * const initArgs[] = {&initArg0,
                                            &initArg1,
                                            &initArg2,
                                            &initArg3,
                                            &initArg4,
                                            &initArg5,
                                            &initArg6};
static const iocshFuncDef initFuncDef = {"NDFileFITSConfigure",7,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    NDFileFITSConfigure(args[0].sval, args[1].ival, args[2].ival, args[3].sval, args[4].ival, args[5].ival, args[6].ival);
}

extern "C" void NDFileFITSRegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
}

extern "C" {
    epicsExportRegistrar(NDFileFITSRegister);
}

