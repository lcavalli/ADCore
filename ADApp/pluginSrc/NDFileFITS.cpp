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

#define STRING_BUFFER_SIZE 81

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
    int naxis = 0;
    long *naxes = NULL;
    int status = 0;
    int numAttributes = 0;
    NDAttribute *pAttribute = NULL;

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

    naxes = (long *) malloc(naxis * sizeof(long));
    if (naxes == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s error, naxes malloc failed. file: %s\n",
            driverName, functionName, fileName);
        return (asynError);
    }

    for (int i = 0; i < naxis; i++) {
        naxes[i] = pArray->dims[i].size;
    }

    /* Create empty image */
    switch (pArray->dataType) {
        case NDInt8:
            fits_create_img(this->pFits, SBYTE_IMG, naxis, naxes, &status);
            break;

        case NDUInt8:
            fits_create_img(this->pFits, BYTE_IMG, naxis, naxes, &status);
            break;

        case NDInt16:
            fits_create_img(this->pFits, SHORT_IMG, naxis, naxes, &status);
            break;

        case NDUInt16:
            fits_create_img(this->pFits, USHORT_IMG, naxis, naxes, &status);
            break;

        case NDInt32:
            fits_create_img(this->pFits, LONG_IMG, naxis, naxes, &status);
            break;

        case NDUInt32:
            fits_create_img(this->pFits, ULONG_IMG, naxis, naxes, &status);
            break;

        case NDFloat32:
            fits_create_img(this->pFits, FLOAT_IMG, naxis, naxes, &status);
            break;

        case NDFloat64:
            fits_create_img(this->pFits, DOUBLE_IMG, naxis, naxes, &status);
            break;

        default:
            return (asynError);
            break;
    }

    free(naxes);

    /* Save attributes */
    if (pArray->pAttributeList != NULL) {

        numAttributes = pArray->pAttributeList->count();
        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s:%s pArray->pAttributeList->count(): %d\n",
            driverName, functionName, numAttributes);

        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s:%s Looping over attributes...\n",
            driverName, functionName);

        pAttribute = pArray->pAttributeList->next(NULL);
        while (pAttribute) {
            char attrString[STRING_BUFFER_SIZE] = {0};
            const char *attributeName = pAttribute->getName();
            const char *attributeDescription = pAttribute->getDescription();
            const char *attributeSource = pAttribute->getSource();

            asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
              "%s:%s : attribute: %s, source: %s\n",
              driverName, functionName, attributeName, attributeSource);

            NDAttrDataType_t attrDataType;
            size_t attrSize;
            NDAttrValue value;
            pAttribute->getValueInfo(&attrDataType, &attrSize);

            switch (attrDataType) {
                case NDAttrInt8:
                case NDAttrUInt8:
                    pAttribute->getValue(attrDataType, &value.i8);
                    fits_write_key(this->pFits, TBYTE, attributeName, &value.i8, attributeDescription, &status);
                    break;

                case NDAttrInt16:
                    pAttribute->getValue(attrDataType, &value.i16);
                    fits_write_key(this->pFits, TSHORT, attributeName, &value.i16, attributeDescription, &status);
                    break;

                case NDAttrUInt16:
                    pAttribute->getValue(attrDataType, &value.i16);
                    fits_write_key(this->pFits, TUSHORT, attributeName, &value.i16, attributeDescription, &status);
                    break;

                case NDAttrInt32:
                    pAttribute->getValue(attrDataType, &value.i32);
                    fits_write_key(this->pFits, TINT, attributeName, &value.i32, attributeDescription, &status);
                    break;

                case NDAttrUInt32:
                    pAttribute->getValue(attrDataType, &value.i32);
                    fits_write_key(this->pFits, TUINT, attributeName, &value.i32, attributeDescription, &status);
                    break;

                case NDAttrFloat32:
                    pAttribute->getValue(attrDataType, &value.f32);
                    fits_write_key(this->pFits, TFLOAT, attributeName, &value.f32, attributeDescription, &status);
                    break;

                case NDAttrFloat64:
                    pAttribute->getValue(attrDataType, &value.f64);
                    fits_write_key(this->pFits, TDOUBLE, attributeName, &value.f64, attributeDescription, &status);
                    break;

                case NDAttrString:
                    memset(attrString, 0, sizeof(attrString)-1);
                    pAttribute->getValue(attrDataType, attrString, sizeof(attrString)-1);
                    fits_write_key(this->pFits, TSTRING, attributeName, attrString, attributeDescription, &status);
                    break;

                case NDAttrUndefined:
                    break;

                default:
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                              "%s:%s error, unknown attrDataType=%d\n",
                              driverName, functionName, attrDataType);
                    return asynError;
                    break;
            }

            pAttribute = pArray->pAttributeList->next(pAttribute);
        }

        if (status > 0) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s error, fits_write_key failed. file: %s\n",
                driverName, functionName, fileName);
            return (asynError);
        }
    }

    return (asynSuccess);
}

/** Writes single NDArray to the FITS file.
  * \param[in] pArray Pointer to the NDArray to be written
  */
asynStatus NDFileFITS::writeFile(NDArray *pArray)
{
    static const char *functionName = "writeFile";
    unsigned long nElements = 1;
    int w = 0;
    int h = 0;
    int d = 0;
    int x = 0;
    int y = 0;
    int z = 0;
    int status = 0;
    void *pOut = NULL;

    for (int i = 0; i < pArray->ndims; i++) {
        nElements *= pArray->dims[i].size;
    }

    /* cfitsio handle array with FORTRAN style, we need to vertically flip it */
    if (pArray->ndims > 3) {
        /* Avoid transposing */
        w = 1;
        h = 1;
        d = 1;
    } else if (pArray->ndims == 3) {
        w = pArray->dims[0].size;
        h = pArray->dims[1].size;
        d = pArray->dims[2].size;
    } else if (pArray->ndims == 2) {
        w = pArray->dims[0].size;
        h = pArray->dims[1].size;
        d = 1;
    } else if (pArray->ndims == 1) {
        w = pArray->dims[0].size;
        h = 1;
        d = 1;
    }

    pOut = malloc(pArray->dataSize);
    if (pOut == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s error, pOut malloc failed.\n",
            driverName, functionName);
        return (asynError);
    }

    /* Write the image */
    switch (pArray->dataType) {
        case NDInt8:
            {
                char *pDst = (char *) pOut;
                char *pSrc = (char *) pArray->pData;

                for (z = 0; z < d; z++) {
                    for (x = 0; x < w; x++) {
                        for (y = 0; y < h; y++) {
                            pDst[z * (w * h) + (h - 1 - y) * w + x] = pSrc[z * (w * h) + y * w + x];
                        }
                    }
                }
            }

            fits_write_img(this->pFits, TSBYTE, 1, nElements, pOut, &status);
            break;

        case NDUInt8:
            {
                unsigned char *pDst = (unsigned char *) pOut;
                unsigned char *pSrc = (unsigned char *) pArray->pData;

                for (z = 0; z < d; z++) {
                    for (x = 0; x < w; x++) {
                        for (y = 0; y < h; y++) {
                            pDst[z * (w * h) + (h - 1 - y) * w + x] = pSrc[z * (w * h) + y * w + x];
                        }
                    }
                }
            }

            fits_write_img(this->pFits, TBYTE, 1, nElements, pOut, &status);
            break;

        case NDInt16:
            {
                short *pDst = (short *) pOut;
                short *pSrc = (short *) pArray->pData;

                for (z = 0; z < d; z++) {
                    for (x = 0; x < w; x++) {
                        for (y = 0; y < h; y++) {
                            pDst[z * (w * h) + (h - 1 - y) * w + x] = pSrc[z * (w * h) + y * w + x];
                        }
                    }
                }
            }

            fits_write_img(this->pFits, TSHORT, 1, nElements, pOut, &status);
            break;

        case NDUInt16:
            {
                unsigned short *pDst = (unsigned short *) pOut;
                unsigned short *pSrc = (unsigned short *) pArray->pData;

                for (z = 0; z < d; z++) {
                    for (x = 0; x < w; x++) {
                        for (y = 0; y < h; y++) {
                            pDst[z * (w * h) + (h - 1 - y) * w + x] = pSrc[z * (w * h) + y * w + x];
                        }
                    }
                }
            }

            fits_write_img(this->pFits, TUSHORT, 1, nElements, pOut, &status);
            break;

        case NDInt32:
            {
                int *pDst = (int *) pOut;
                int *pSrc = (int *) pArray->pData;

                for (z = 0; z < d; z++) {
                    for (x = 0; x < w; x++) {
                        for (y = 0; y < h; y++) {
                            pDst[z * (w * h) + (h - 1 - y) * w + x] = pSrc[z * (w * h) + y * w + x];
                        }
                    }
                }
            }

            fits_write_img(this->pFits, TINT, 1, nElements, pOut, &status);
            break;

        case NDUInt32:
            {
                unsigned int *pDst = (unsigned int *) pOut;
                unsigned int *pSrc = (unsigned int *) pArray->pData;

                for (z = 0; z < d; z++) {
                    for (x = 0; x < w; x++) {
                        for (y = 0; y < h; y++) {
                            pDst[z * (w * h) + (h - 1 - y) * w + x] = pSrc[z * (w * h) + y * w + x];
                        }
                    }
                }
            }

            fits_write_img(this->pFits, TUINT, 1, nElements, pOut, &status);
            break;

        case NDFloat32:
            {
                float *pDst = (float *) pOut;
                float *pSrc = (float *) pArray->pData;

                for (z = 0; z < d; z++) {
                    for (x = 0; x < w; x++) {
                        for (y = 0; y < h; y++) {
                            pDst[z * (w * h) + (h - 1 - y) * w + x] = pSrc[z * (w * h) + y * w + x];
                        }
                    }
                }
            }

            fits_write_img(this->pFits, TFLOAT, 1, nElements, pOut, &status);
            break;

        case NDFloat64:
            {
                double *pDst = (double *) pOut;
                double *pSrc = (double *) pArray->pData;

                for (z = 0; z < d; z++) {
                    for (x = 0; x < w; x++) {
                        for (y = 0; y < h; y++) {
                            pDst[z * (w * h) + (h - 1 - y) * w + x] = pSrc[z * (w * h) + y * w + x];
                        }
                    }
                }
            }

            fits_write_img(this->pFits, TDOUBLE, 1, nElements, pOut, &status);
            break;

        default:
            return (asynError);
            break;
    }

    free(pOut);

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

