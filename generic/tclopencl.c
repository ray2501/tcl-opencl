#include <tcl.h>
#include <vectcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tclopencl.h"

#define NS "opencl"

typedef struct ThreadSpecificData {
    int initialized;
    Tcl_HashTable *cl_hashtblPtr;
    int platform_count;
    int device_count;
    int context_count;
    int queue_count;
    int program_count;
    int kernel_count;
    int buffer_count;
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

TCL_DECLARE_MUTEX(myMutex);

typedef struct clBufferInfo {
    cl_mem mem;
    int typevalue;
    size_t length;
} clBufferInfo;


void OPENCL_Thread_Exit(ClientData clientdata)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if(tsdPtr->cl_hashtblPtr) {
        Tcl_DeleteHashTable(tsdPtr->cl_hashtblPtr);
        ckfree(tsdPtr->cl_hashtblPtr);
    }
}


/*
 * For Buffer API
 */
static int BUFFER_FUNCTION(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    int choice;
    Tcl_HashEntry *hashEntryPtr;
    char *handle;
    clBufferInfo *bufferinfo = NULL;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    static const char *FUNC_strs[] = {
        "close",
        0
    };

    enum FUNC_enum {
        FUNC_CLOSE,
    };

    if( objc < 2 ){
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }

    if( Tcl_GetIndexFromObj(interp, objv[1], FUNC_strs, "option", 0, &choice) ){
        return TCL_ERROR;
    }

    handle = Tcl_GetStringFromObj(objv[0], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, handle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid buffer handle ",
                                    handle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    bufferinfo = (clBufferInfo *) Tcl_GetHashValue( hashEntryPtr );

    switch( (enum FUNC_enum)choice ){
        case FUNC_CLOSE: {
            if( objc != 2 ){
                Tcl_WrongNumArgs(interp, 2, objv, 0);
                return TCL_ERROR;
            }

            if (bufferinfo) {
                clReleaseMemObject( bufferinfo->mem );
                ckfree(bufferinfo);
            }
            bufferinfo = NULL;

            Tcl_MutexLock(&myMutex);
            if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
            Tcl_MutexUnlock(&myMutex);

            Tcl_DeleteCommand(interp, handle);
            Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

            break;
        }
    }

    return TCL_OK;
}


static int createBuffer(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    cl_int err = CL_SUCCESS;
    cl_context context;
    cl_mem mem;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    Tcl_HashEntry *newHashEntryPtr;
    Tcl_HashEntry *hashEntryPtr;
    Tcl_Obj *pResultStr = NULL;
    char handleName[16 + TCL_INTEGER_SPACE];
    int newvalue;
    char *chandle = NULL;
    char *flag = NULL;
    char *type = NULL;
    int typevalue = 11;
    int len = 0;
    size_t length= 0;
    clBufferInfo *bufferinfo;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
      tsdPtr->initialized = 1;
      tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
      Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    if (objc !=5) {
        Tcl_WrongNumArgs(interp, 1, objv, "context flags type length");
        return TCL_ERROR;
    }

    chandle = Tcl_GetStringFromObj(objv[1], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, chandle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid context handle ",
                                    chandle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    context = (cl_context) Tcl_GetHashValue( hashEntryPtr );

    flag = Tcl_GetStringFromObj(objv[2], &len);
    if( !flag || len < 1 ){
        return TCL_ERROR;
    }

    type = Tcl_GetStringFromObj(objv[3], &len);
    if( !type || len < 1 ){
        return TCL_ERROR;
    }

    if(Tcl_GetIntFromObj(interp, objv[4], (int *) &length) != TCL_OK) {
        return TCL_ERROR;
    }
    if (length <= 0) {
        return TCL_ERROR;
    }

    if(strcmp(flag, "r")==0) {
        flags = CL_MEM_READ_ONLY;
    } else if(strcmp(flag, "w")==0) {
        flags = CL_MEM_WRITE_ONLY;
    } else if(strcmp(flag, "rw")==0) {
        flags = CL_MEM_READ_WRITE;
    } else {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "Wrong cl_mem_flags", (char *)NULL );
        }
        return TCL_ERROR;
    }

    if(strcmp(type, "int")==0) {
        typevalue = 1;
        mem = clCreateBuffer(context, flags, length * sizeof(int), NULL, &err);
    } else if(strcmp(type, "long")==0) {
        typevalue = 8;
        mem = clCreateBuffer(context, flags, length * sizeof(long), NULL, &err);
    } else if(strcmp(type, "double")==0) {
        typevalue = 11;
        mem = clCreateBuffer(context, flags, length * sizeof(double), NULL, &err);
    } else {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "clCreateBuffer: wrong type",
                                    (char *)NULL );
        }
        return TCL_ERROR;
    }

    if (mem == NULL || err != CL_SUCCESS) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "clCreateBuffer failed",
                                    (char *)NULL );
        }
        return TCL_ERROR;
    }

    bufferinfo = (clBufferInfo *) ckalloc (sizeof(clBufferInfo));
    bufferinfo->mem = mem;
    bufferinfo->typevalue = typevalue;
    bufferinfo->length = length;

    Tcl_MutexLock(&myMutex);
    sprintf( handleName, "cl-buffer%d", tsdPtr->buffer_count++ );

    pResultStr = Tcl_NewStringObj( handleName, -1 );

    newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->cl_hashtblPtr, handleName, &newvalue);
    Tcl_SetHashValue(newHashEntryPtr, bufferinfo);
    Tcl_MutexUnlock(&myMutex);

    Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) BUFFER_FUNCTION,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_SetObjResult(interp, pResultStr);
    return TCL_OK;
}


/*
 * For Kernel API
 */
static int KERNEL_FUNCTION(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    int choice;
    Tcl_HashEntry *hashEntryPtr;
    char *handle;
    cl_kernel kernel;
    cl_int err = CL_SUCCESS;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    static const char *FUNC_strs[] = {
        "setKernelArg",
        "close",
        0
    };

    enum FUNC_enum {
        FUNC_SETKERNELARG,
        FUNC_CLOSE,
    };

    if( objc < 2 ){
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }

    if( Tcl_GetIndexFromObj(interp, objv[1], FUNC_strs, "option", 0, &choice) ){
        return TCL_ERROR;
    }

    handle = Tcl_GetStringFromObj(objv[0], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, handle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid kernel handle ",
                                    handle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    kernel = (cl_kernel) Tcl_GetHashValue( hashEntryPtr );

    switch( (enum FUNC_enum)choice ){
        case FUNC_SETKERNELARG: {
            int index = 0;
            char *type;

            if( objc != 5 ){
                Tcl_WrongNumArgs(interp, 2, objv, "index type value");
                return TCL_ERROR;
            }

            if(Tcl_GetIntFromObj(interp, objv[2], (int *) &index) != TCL_OK) {
                return TCL_ERROR;
            }
            if (index < 0) {
                return TCL_ERROR;
            }

            type = Tcl_GetStringFromObj(objv[3], 0);
            if(strcmp(type, "mem")==0) {
                char *bhandle = NULL;
                clBufferInfo *bufferinfo = NULL;

                bhandle = Tcl_GetStringFromObj(objv[4], 0);
                hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, bhandle );
                if( !hashEntryPtr ) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "invalid buffer handle ",
                                                bhandle, (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                bufferinfo = (clBufferInfo *) Tcl_GetHashValue( hashEntryPtr );

                err = clSetKernelArg(kernel, index, sizeof(cl_mem),
                                     (const void *) &(bufferinfo->mem));
                if (err != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clSetKernelArg failed",
                                                (char *)NULL );
                    }
                    return TCL_ERROR;
                }
            } else if(strcmp(type, "int")==0) {
                int value = 0;
                if(Tcl_GetIntFromObj(interp, objv[4], (int *) &value) != TCL_OK) {
                    return TCL_ERROR;
                }

                err = clSetKernelArg(kernel, index, sizeof(int), (const void *) &value);
                if (err != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clSetKernelArg failed",
                                                (char *)NULL );
                    }
                    return TCL_ERROR;
                }
            } else if(strcmp(type, "long")==0) {
                long value = 0;
                if(Tcl_GetLongFromObj(interp, objv[4], (long *) &value) != TCL_OK) {
                    return TCL_ERROR;
                }

                err = clSetKernelArg(kernel, index, sizeof(long), (const void *) &value);
                if (err != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clSetKernelArg failed",
                                                (char *)NULL );
                    }
                    return TCL_ERROR;
                }
            } else if(strcmp(type, "double")==0) {
                double value = 0.0;
                if(Tcl_GetDoubleFromObj(interp, objv[4], (double *) &value) != TCL_OK) {
                    return TCL_ERROR;
                }

                err = clSetKernelArg(kernel, index, sizeof(double), (const void *) &value);
                if (err != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clSetKernelArg failed",
                                                (char *)NULL );
                    }
                    return TCL_ERROR;
                }
            } else {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "setKernelArg: unknown type",
                                            (char *)NULL );
                }
                return TCL_ERROR;
            }

            break;
        }
        case FUNC_CLOSE: {
            if( objc != 2 ){
                Tcl_WrongNumArgs(interp, 2, objv, 0);
                return TCL_ERROR;
            }

            clReleaseKernel( kernel );

            Tcl_MutexLock(&myMutex);
            if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
            Tcl_MutexUnlock(&myMutex);

            Tcl_DeleteCommand(interp, handle);
            Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

            break;
        }
    }

    return TCL_OK;
}


static int createKernel(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    cl_int err = CL_SUCCESS;
    cl_program program;
    cl_kernel kernel;
    Tcl_HashEntry *newHashEntryPtr;
    Tcl_HashEntry *hashEntryPtr;
    Tcl_Obj *pResultStr = NULL;
    char handleName[16 + TCL_INTEGER_SPACE];
    int newvalue;
    char *chandle = NULL;
    char *kernel_name = NULL;
    int len = 0;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
      tsdPtr->initialized = 1;
      tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
      Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    if (objc !=3) {
        Tcl_WrongNumArgs(interp, 1, objv, "program kernel_name");
        return TCL_ERROR;
    }

    chandle = Tcl_GetStringFromObj(objv[1], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, chandle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid program handle ",
                                    chandle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    program = (cl_program) Tcl_GetHashValue( hashEntryPtr );

    kernel_name = Tcl_GetStringFromObj(objv[2], &len);
    if( !kernel_name || len < 1 ){
        return TCL_ERROR;
    }

    kernel = clCreateKernel(program, kernel_name, &err);
    if (kernel == NULL || err != CL_SUCCESS) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "clCreateKernel failed",
                                    (char *)NULL );
        }
        return TCL_ERROR;
    }

    Tcl_MutexLock(&myMutex);
    sprintf( handleName, "cl-kernel%d", tsdPtr->kernel_count++ );

    pResultStr = Tcl_NewStringObj( handleName, -1 );

    newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->cl_hashtblPtr, handleName, &newvalue);
    Tcl_SetHashValue(newHashEntryPtr, kernel);
    Tcl_MutexUnlock(&myMutex);

    Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) KERNEL_FUNCTION,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_SetObjResult(interp, pResultStr);
    return TCL_OK;
}


/*
 * For Program API
 */
static int PROGRAM_FUNCTION(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    int choice;
    Tcl_HashEntry *hashEntryPtr;
    char *handle;
    cl_program program;
    cl_int err = CL_SUCCESS;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    static const char *FUNC_strs[] = {
        "build",
        "info",
        "close",
        0
    };

    enum FUNC_enum {
        FUNC_BUILD,
        FUNC_INFO,
        FUNC_CLOSE,
    };

    if( objc < 2 ){
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }

    if( Tcl_GetIndexFromObj(interp, objv[1], FUNC_strs, "option", 0, &choice) ){
        return TCL_ERROR;
    }

    handle = Tcl_GetStringFromObj(objv[0], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, handle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid program handle ",
                                    handle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    program = (cl_program) Tcl_GetHashValue( hashEntryPtr );

    switch( (enum FUNC_enum)choice ){
        case FUNC_BUILD: {
            err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
            if (err != CL_SUCCESS) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "clBuildProgram failed",
                                            (char *)NULL );
                }

                return TCL_ERROR;
            }

            break;
        }
        case FUNC_INFO: {
            char *property = NULL;
            int len = 0;
            size_t binary_size = 0;

            if( objc != 3 ){
                Tcl_WrongNumArgs(interp, 2, objv, "property");
                return TCL_ERROR;
            }

            property = Tcl_GetStringFromObj(objv[2], &len);
            if (!property || len < 1) {
                return TCL_ERROR;
            }

            if(strcmp(property, "binary_sizes")==0) {
                err = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES,
                                       sizeof(size_t), &binary_size, NULL);
                if (err != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetProgramInfo failed",
                                                (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                Tcl_SetObjResult(interp, Tcl_NewLongObj( binary_size ));
            } else if(strcmp(property, "binaries")==0) {
                unsigned char *binary;

                err = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES,
                                       sizeof(size_t), &binary_size, NULL);
                if (err != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetProgramInfo failed",
                                                (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                if (binary_size==0) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "binary size is zero",
                                                (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                binary = (unsigned char *) ckalloc(binary_size * sizeof(unsigned char));
                clGetProgramInfo(program, CL_PROGRAM_BINARIES, binary_size, &binary, NULL);
                Tcl_SetObjResult(interp, Tcl_NewByteArrayObj( binary, binary_size));
                if (binary) ckfree(binary);
            } else {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "Wrong property", (char *)NULL );
                }

                return TCL_ERROR;
            }

            break;
        }
        case FUNC_CLOSE: {
            if( objc != 2 ){
                Tcl_WrongNumArgs(interp, 2, objv, 0);
                return TCL_ERROR;
            }

            clReleaseProgram( program );

            Tcl_MutexLock(&myMutex);
            if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
            Tcl_MutexUnlock(&myMutex);

            Tcl_DeleteCommand(interp, handle);
            Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

            break;
        }
    }

    return TCL_OK;
}


static int createProgramWithSource(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    cl_int err = CL_SUCCESS;
    cl_context context;
    cl_program program;
    Tcl_HashEntry *newHashEntryPtr;
    Tcl_HashEntry *hashEntryPtr;
    Tcl_Obj *pResultStr = NULL;
    char handleName[16 + TCL_INTEGER_SPACE];
    int newvalue;
    char *chandle = NULL;
    char *source = NULL;
    int len = 0;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
      tsdPtr->initialized = 1;
      tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
      Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    if (objc !=3) {
        Tcl_WrongNumArgs(interp, 1, objv, "context source");
        return TCL_ERROR;
    }

    chandle = Tcl_GetStringFromObj(objv[1], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, chandle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid context handle ",
                                    chandle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    context = (cl_context) Tcl_GetHashValue( hashEntryPtr );

    source = Tcl_GetStringFromObj(objv[2], &len);
    if( !source || len < 1 ){
        return TCL_ERROR;
    }

    program = clCreateProgramWithSource(context, 1, (const char **) &source, NULL, &err);
    if (program == NULL || err != CL_SUCCESS) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "clCreateProgramWithSource failed",
                                    (char *)NULL );
        }
        return TCL_ERROR;
    }

    Tcl_MutexLock(&myMutex);
    sprintf( handleName, "cl-program%d", tsdPtr->program_count++ );

    pResultStr = Tcl_NewStringObj( handleName, -1 );

    newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->cl_hashtblPtr, handleName, &newvalue);
    Tcl_SetHashValue(newHashEntryPtr, program);
    Tcl_MutexUnlock(&myMutex);

    Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) PROGRAM_FUNCTION,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_SetObjResult(interp, pResultStr);
    return TCL_OK;
}


static int createProgramWithBinary(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    cl_int err = CL_SUCCESS;
    cl_int status = CL_SUCCESS;
    cl_context context;
    cl_device_id device;
    cl_program program;
    Tcl_HashEntry *newHashEntryPtr;
    Tcl_HashEntry *hashEntryPtr;
    Tcl_Obj *pResultStr = NULL;
    char handleName[16 + TCL_INTEGER_SPACE];
    int newvalue;
    char *chandle = NULL;
    char *dhandle = NULL;
    unsigned char *binary = NULL;
    size_t len = 0;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
      tsdPtr->initialized = 1;
      tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
      Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    if (objc !=4) {
        Tcl_WrongNumArgs(interp, 1, objv, "context deviceID binary");
        return TCL_ERROR;
    }

    chandle = Tcl_GetStringFromObj(objv[1], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, chandle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid context handle ",
                                    chandle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    context = (cl_context) Tcl_GetHashValue( hashEntryPtr );

    dhandle = Tcl_GetStringFromObj(objv[2], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, dhandle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid deviceID handle ",
                                    dhandle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    device = (cl_device_id) Tcl_GetHashValue( hashEntryPtr );

    binary = Tcl_GetByteArrayFromObj(objv[3], (int *) &len);
    if( !binary || len < 1 ){
        return TCL_ERROR;
    }

    program = clCreateProgramWithBinary(context, 1, &device, &len,
                                        (const unsigned char **) &binary,
                                        &status, &err);
    if (program == NULL || status != CL_SUCCESS || err != CL_SUCCESS) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "clCreateProgramWithBinary failed",
                                    (char *)NULL );
        }
        return TCL_ERROR;
    }

    Tcl_MutexLock(&myMutex);
    sprintf( handleName, "cl-program%d", tsdPtr->program_count++ );

    pResultStr = Tcl_NewStringObj( handleName, -1 );

    newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->cl_hashtblPtr, handleName, &newvalue);
    Tcl_SetHashValue(newHashEntryPtr, program);
    Tcl_MutexUnlock(&myMutex);

    Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) PROGRAM_FUNCTION,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_SetObjResult(interp, pResultStr);
    return TCL_OK;
}


/*
 * For CommandQueue API
 */
static int QUEUE_FUNCTION(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    int choice;
    Tcl_HashEntry *hashEntryPtr;
    char *handle;
    cl_command_queue queue;
    cl_int err = CL_SUCCESS;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    static const char *FUNC_strs[] = {
        "enqueueWriteBuffer",
        "enqueueReadBuffer",
        "enqueueCopyBuffer",
        "enqueueNDRangeKernel",
        "flush",
        "finish",
        "close",
        0
    };

    enum FUNC_enum {
        FUNC_ENQUEUEWRITEBUFFER,
        FUNC_ENQUEUEREADBUFFER,
        FUNC_ENQUEUECOPYBUFFER,
        FUNC_ENQUEUENDRANGEKERNEL,
        FUNC_FLUSH,
        FUNC_FINISH,
        FUNC_CLOSE,
    };

    if( objc < 2 ){
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }

    if( Tcl_GetIndexFromObj(interp, objv[1], FUNC_strs, "option", 0, &choice) ){
        return TCL_ERROR;
    }

    handle = Tcl_GetStringFromObj(objv[0], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, handle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid command queue handle ",
                                    handle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    queue = (cl_command_queue) Tcl_GetHashValue( hashEntryPtr );

    switch( (enum FUNC_enum)choice ){
        case FUNC_ENQUEUEWRITEBUFFER: {
            char *bhandle = NULL;
            clBufferInfo *buffer = NULL;
            NumArrayInfo *arr_info = NULL;
            const void *arrptr = NULL;

            if( objc != 4 ){
                Tcl_WrongNumArgs(interp, 2, objv, "buffer numarray");
                return TCL_ERROR;
            }

            bhandle = Tcl_GetStringFromObj(objv[2], 0);
            hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, bhandle );
            if( !hashEntryPtr ) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "invalid buffer handle ",
                                            bhandle, (char *)NULL );
                }

                return TCL_ERROR;
            }

            buffer = (clBufferInfo *) Tcl_GetHashValue( hashEntryPtr );

            arr_info = NumArrayGetInfoFromObj(interp, objv[3]);
            if (!arr_info) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "Get NumArray info failed",
                                            (char *)NULL );
                }

                return TCL_ERROR;
            }

            if(ISEMPTYINFO(arr_info)) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "NumArray is empty",
                                            (char *)NULL );
                }

                return TCL_ERROR;
            }

            arrptr = (const void *) NumArrayGetPtrFromObj(interp, objv[3]);
            if (!arrptr) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "Get NumArray pointer failed",
                                            (char *)NULL );
                }

                return TCL_ERROR;
            }

            if (buffer->typevalue == 1 && arr_info->type == 1) {
                int *tmpbuffer = NULL;
                long int *tmpptr = (long int *) arrptr;
                int i = 0;

                tmpbuffer = (int *) malloc (buffer->length * sizeof(int));
                if(!tmpbuffer) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj,
                                                "Write: int malloc memory failed",
                                                (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                /*
                 * It has potential data lose issues, but I need covert it here.
                 */
                for (i = 0; i < buffer->length; i++) {
                    *(tmpbuffer + i) = (int) *(tmpptr + i);
                }

                err = clEnqueueWriteBuffer( queue, buffer->mem, CL_TRUE, 0,
                                            buffer->length * sizeof(int),
                                            tmpbuffer, 0, NULL, NULL );
                if(tmpbuffer) free(tmpbuffer);
                tmpbuffer = NULL;
            } else if (buffer->typevalue == 8 && arr_info->type == 1) {
                err = clEnqueueWriteBuffer( queue, buffer->mem, CL_TRUE, 0,
                                            buffer->length * sizeof(long),
                                            arrptr, 0, NULL, NULL );
            } else if (buffer->typevalue == 11 && arr_info->type == 11) {
                err = clEnqueueWriteBuffer( queue, buffer->mem, CL_TRUE, 0,
                                            buffer->length * sizeof(double),
                                            arrptr, 0, NULL, NULL );
            } else {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj,
                                            "clEnqueueWriteBuffer: wrong type",
                                            (char *)NULL );
                }

                return TCL_ERROR;
            }

            if (err != CL_SUCCESS) {
               if( interp ) {
                   Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                   Tcl_AppendStringsToObj( resultObj, "clEnqueueWriteBuffer failed",
                                           (char *)NULL );
               }

               return TCL_ERROR;
            }
            break;
        }
        case FUNC_ENQUEUEREADBUFFER: {
            char *bhandle = NULL;
            clBufferInfo *buffer = NULL;
            void *arrptr = NULL;
            Tcl_Obj *result = NULL;

            if( objc != 3 ){
                Tcl_WrongNumArgs(interp, 2, objv, "buffer");
                return TCL_ERROR;
            }

            bhandle = Tcl_GetStringFromObj(objv[2], 0);
            hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, bhandle );
            if( !hashEntryPtr ) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "invalid buffer handle ",
                                            bhandle, (char *)NULL );
                }

                return TCL_ERROR;
            }

            buffer = (clBufferInfo *) Tcl_GetHashValue( hashEntryPtr );
            if(buffer->typevalue == 1) {
                long int *tmpptr = NULL;
                int *tmpbuffer = NULL;
                int i = 0;
                tmpbuffer = (int *) malloc (buffer->length * sizeof(int));
                if(!tmpbuffer) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "Write: int malloc memory failed",
                                                (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                err = clEnqueueReadBuffer( queue, buffer->mem, CL_TRUE, 0,
                                           buffer->length * sizeof(int),
                                           tmpbuffer, 0, NULL, NULL );

                result = NumArrayNewVector(NumArray_Int, buffer->length);
                arrptr = (void *) NumArrayGetPtrFromObj(interp, result);
                tmpptr = (long int *) arrptr;

                for (i = 0; i < buffer->length; i++) {
                    *(tmpptr + i) = (long int) *(tmpbuffer + i);
                }

                if(tmpbuffer) free(tmpbuffer);
                tmpbuffer = NULL;
            } else if(buffer->typevalue == 8) {
                result = NumArrayNewVector(NumArray_Int, buffer->length);
                arrptr = (void *) NumArrayGetPtrFromObj(interp, result);
                err = clEnqueueReadBuffer( queue, buffer->mem, CL_TRUE, 0,
                                           buffer->length * sizeof(long),
                                           arrptr, 0, NULL, NULL );
            } else if(buffer->typevalue == 11) {
                result = NumArrayNewVector(NumArray_Float64, buffer->length);
                arrptr = (void *) NumArrayGetPtrFromObj(interp, result);
                err = clEnqueueReadBuffer( queue, buffer->mem, CL_TRUE, 0,
                                           buffer->length * sizeof(double),
                                           arrptr, 0, NULL, NULL );
            } else {
               if( interp ) {
                   Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                   Tcl_AppendStringsToObj( resultObj, "Read: wrong type", (char *)NULL );
               }

               return TCL_ERROR;
            }

            if (err != CL_SUCCESS) {
               if( interp ) {
                   Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                   Tcl_AppendStringsToObj( resultObj, "clEnqueueReadBuffer failed",
                                           (char *)NULL );
               }

               return TCL_ERROR;
            }

            Tcl_SetObjResult(interp, result);
            break;
        }
        case FUNC_ENQUEUECOPYBUFFER: {
            char *shandle = NULL;
            char *dhandle = NULL;
            clBufferInfo *buffer1 = NULL;
            clBufferInfo *buffer2 = NULL;

            if( objc != 4 ){
                Tcl_WrongNumArgs(interp, 2, objv, "srcBuffer dstBuffer");
                return TCL_ERROR;
            }

            shandle = Tcl_GetStringFromObj(objv[2], 0);
            hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, shandle );
            if( !hashEntryPtr ) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "invalid src buffer handle ",
                                            shandle, (char *)NULL );
                }

                return TCL_ERROR;
            }

            buffer1 = (clBufferInfo *) Tcl_GetHashValue( hashEntryPtr );

            dhandle = Tcl_GetStringFromObj(objv[3], 0);
            hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, dhandle );
            if( !hashEntryPtr ) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "invalid dst buffer handle ",
                                            dhandle, (char *)NULL );
                }

                return TCL_ERROR;
            }

            buffer2 = (clBufferInfo *) Tcl_GetHashValue( hashEntryPtr );

            if (buffer1->typevalue == 1 && buffer2->typevalue == 1) {
                err = clEnqueueCopyBuffer( queue, buffer1->mem, buffer2->mem,
                            0, 0, buffer1->length * sizeof(int),
                            0, NULL, NULL );
            } else if (buffer1->typevalue == 8 && buffer2->typevalue == 8) {
                err = clEnqueueCopyBuffer( queue, buffer1->mem, buffer2->mem,
                            0, 0, buffer1->length * sizeof(long),
                            0, NULL, NULL );
            } else if (buffer1->typevalue == 11 && buffer2->typevalue == 11) {
                err = clEnqueueCopyBuffer( queue, buffer1->mem, buffer2->mem,
                            0, 0, buffer1->length * sizeof(double),
                            0, NULL, NULL );
            } else {
               if( interp ) {
                   Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                   Tcl_AppendStringsToObj( resultObj, "Copy: wrong type", (char *)NULL );
               }

               return TCL_ERROR;
            }

            if (err != CL_SUCCESS) {
               if( interp ) {
                   Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                   Tcl_AppendStringsToObj( resultObj, "clEnqueueCopyBuffer failed",
                                           (char *)NULL );
               }

               return TCL_ERROR;
            }

            break;
        }
        case FUNC_ENQUEUENDRANGEKERNEL: {
            char *handle = NULL;
            cl_kernel kernel;
            size_t global_work_size = 0;
            size_t local_work_size = 0;

            if( objc != 5 ){
                Tcl_WrongNumArgs(interp, 2, objv,
                                 "kernel global_work_size local_work_size");
                return TCL_ERROR;
            }

            handle = Tcl_GetStringFromObj(objv[2], 0);
            hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, handle );
            if( !hashEntryPtr ) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "invalid kernel handle ",
                                            handle, (char *)NULL );
                }

                return TCL_ERROR;
            }

            kernel = (cl_kernel) Tcl_GetHashValue( hashEntryPtr );

            if(Tcl_GetLongFromObj(interp, objv[3], (long int *) &global_work_size) != TCL_OK) {
                return TCL_ERROR;
            }

            if(Tcl_GetLongFromObj(interp, objv[4], (long int *) &local_work_size) != TCL_OK) {
                return TCL_ERROR;
            }

             err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL,
                    &global_work_size, &local_work_size, 0, NULL, NULL);
             if (err != CL_SUCCESS) {
               if( interp ) {
                   Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                   Tcl_AppendStringsToObj( resultObj, "clEnqueueNDRangeKernel failed",
                                           (char *)NULL );
               }

               return TCL_ERROR;
            }

            break;
        }
        case FUNC_FLUSH: {
            if( objc != 2 ){
                Tcl_WrongNumArgs(interp, 2, objv, 0);
                return TCL_ERROR;
            }

            err = clFlush(queue);
            if (err != CL_SUCCESS) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "clFinish failed",
                                            (char *)NULL );
                }
                return TCL_ERROR;
            }
            break;
        }
        case FUNC_FINISH: {
            if( objc != 2 ){
                Tcl_WrongNumArgs(interp, 2, objv, 0);
                return TCL_ERROR;
            }

            err = clFinish(queue);
            if (err != CL_SUCCESS) {
                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "clFinish failed",
                                            (char *)NULL );
                }
                return TCL_ERROR;
            }
            break;
        }
        case FUNC_CLOSE: {
            if( objc != 2 ){
                Tcl_WrongNumArgs(interp, 2, objv, 0);
                return TCL_ERROR;
            }

            clReleaseCommandQueue( queue );

            Tcl_MutexLock(&myMutex);
            if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
            Tcl_MutexUnlock(&myMutex);

            Tcl_DeleteCommand(interp, handle);
            Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

            break;
        }
    }

    return TCL_OK;
}


static int createCommandQueue(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    cl_int err = CL_SUCCESS;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    Tcl_HashEntry *newHashEntryPtr;
    Tcl_HashEntry *hashEntryPtr;
    Tcl_Obj *pResultStr = NULL;
    char handleName[16 + TCL_INTEGER_SPACE];
    int newvalue;
    char *chandle = NULL;
    char *dhandle = NULL;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
      tsdPtr->initialized = 1;
      tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
      Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    if (objc !=3) {
        Tcl_WrongNumArgs(interp, 1, objv, "context deviceID");
        return TCL_ERROR;
    }

    chandle = Tcl_GetStringFromObj(objv[1], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, chandle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid context handle ",
                                    chandle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    context = (cl_context) Tcl_GetHashValue( hashEntryPtr );

    dhandle = Tcl_GetStringFromObj(objv[2], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, dhandle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid deviceID handle ",
                                    dhandle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    device = (cl_device_id) Tcl_GetHashValue( hashEntryPtr );

#if TCLOPENCL_CL_VERSION > 0x1020
    queue = clCreateCommandQueueWithProperties( context, device, 0, &err );
#else
    queue = clCreateCommandQueue( context, device, 0, &err );
#endif
    if (context == NULL || err != CL_SUCCESS) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "clCreateCommandQueue failed",
                                    (char *)NULL );
        }
        return TCL_ERROR;
    }

    Tcl_MutexLock(&myMutex);
    sprintf( handleName, "cl-cqueue%d", tsdPtr->queue_count++ );

    pResultStr = Tcl_NewStringObj( handleName, -1 );

    newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->cl_hashtblPtr, handleName, &newvalue);
    Tcl_SetHashValue(newHashEntryPtr, queue);
    Tcl_MutexUnlock(&myMutex);

    Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) QUEUE_FUNCTION,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_SetObjResult(interp, pResultStr);
    return TCL_OK;
}


/*
 * For context API
 */
static int CONTEXT_FUNCTION(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    int choice;
    Tcl_HashEntry *hashEntryPtr;
    char *handle;
    cl_context ctx;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    static const char *FUNC_strs[] = {
        "close",
        0
    };

    enum FUNC_enum {
        FUNC_CLOSE,
    };

    if( objc < 2 ){
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }

    if( Tcl_GetIndexFromObj(interp, objv[1], FUNC_strs, "option", 0, &choice) ){
        return TCL_ERROR;
    }

    handle = Tcl_GetStringFromObj(objv[0], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, handle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid context handle ",
                                    handle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    ctx = (cl_context) Tcl_GetHashValue( hashEntryPtr );

    switch( (enum FUNC_enum)choice ){
        case FUNC_CLOSE: {
            if( objc != 2 ){
                Tcl_WrongNumArgs(interp, 2, objv, 0);
                return TCL_ERROR;
            }

            clReleaseContext( ctx );

            Tcl_MutexLock(&myMutex);
            if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
            Tcl_MutexUnlock(&myMutex);

            Tcl_DeleteCommand(interp, handle);
            Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

            break;
        }
    }

    return TCL_OK;
}


static int createContext(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    cl_int err;
    cl_device_id device;
    cl_platform_id platform;
    cl_context context;
    Tcl_HashEntry *newHashEntryPtr;
    Tcl_HashEntry *hashEntryPtr;
    Tcl_Obj *pResultStr = NULL;
    char handleName[16 + TCL_INTEGER_SPACE];
    int newvalue;
    char *dhandle = NULL;
    char *phandle = NULL;
    cl_context_properties props[3] = { CL_CONTEXT_PLATFORM, 0, 0 };

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
      tsdPtr->initialized = 1;
      tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
      Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    if (objc !=3) {
        Tcl_WrongNumArgs(interp, 1, objv, "deviceID platformID");
        return TCL_ERROR;
    }

    dhandle = Tcl_GetStringFromObj(objv[1], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, dhandle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid deviceID handle ",
                                    dhandle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    device = (cl_device_id) Tcl_GetHashValue( hashEntryPtr );

    phandle = Tcl_GetStringFromObj(objv[2], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, phandle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid platformID handle ",
                                    phandle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    platform = (cl_platform_id) Tcl_GetHashValue( hashEntryPtr );

    props[1] = (cl_context_properties) platform;
    context = clCreateContext( props, 1, &device, NULL, NULL, &err );
    if (context == NULL || err != CL_SUCCESS) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "clCreateContext failed",
                                    (char *)NULL );
        }
        return TCL_ERROR;
    }

    Tcl_MutexLock(&myMutex);
    sprintf( handleName, "cl-context%d", tsdPtr->context_count++ );

    pResultStr = Tcl_NewStringObj( handleName, -1 );

    newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->cl_hashtblPtr, handleName, &newvalue);
    Tcl_SetHashValue(newHashEntryPtr, context);
    Tcl_MutexUnlock(&myMutex);

    Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) CONTEXT_FUNCTION,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_SetObjResult(interp, pResultStr);
    return TCL_OK;
}


/*
 * For device API
 */
static int DEVICE_FUNCTION(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    int choice;
    Tcl_HashEntry *hashEntryPtr;
    char *handle;
    cl_device_id device;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    static const char *FUNC_strs[] = {
        "info",
        "close",
        0
    };

    enum FUNC_enum {
        FUNC_INFO,
        FUNC_CLOSE,
    };

    if( objc < 2 ){
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }

    if( Tcl_GetIndexFromObj(interp, objv[1], FUNC_strs, "option", 0, &choice) ){
        return TCL_ERROR;
    }

    handle = Tcl_GetStringFromObj(objv[0], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, handle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid deviceID handle ",
                                    handle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    device = (cl_device_id) Tcl_GetHashValue( hashEntryPtr );

    switch( (enum FUNC_enum)choice ){
        case FUNC_INFO: {
            char *param = NULL;
            int len = 0;
            cl_platform_info param_name = CL_DEVICE_NAME;
            cl_int error;
            size_t size;
            Tcl_Obj *pResultStr = NULL;
            int type = 1;

            if( objc != 3 ){
                Tcl_WrongNumArgs(interp, 2, objv, "param_name");
                return TCL_ERROR;
            }

            param = Tcl_GetStringFromObj(objv[2], &len);
            if( !param || len < 1 ){
                return TCL_ERROR;
            }

            if(strcmp(param, "name")==0) {
                param_name = CL_DEVICE_NAME;
                type = 1;  // char []
            } else if(strcmp(param, "profile")==0) {
                param_name = CL_DEVICE_PROFILE;
                type = 1;
            } else if(strcmp(param, "vendor")==0) {
                param_name = CL_DEVICE_VENDOR;
                type = 1;
            } else if(strcmp(param, "device_version")==0) {
                param_name = CL_DEVICE_VERSION;
                type = 1;
            } else if(strcmp(param, "driver_version")==0) {
                param_name = CL_DRIVER_VERSION;
                type = 1;
            } else if(strcmp(param, "extensions")==0) {
                param_name = CL_DEVICE_EXTENSIONS;
                type = 1;
            } else if(strcmp(param, "avaiable")==0) {
                param_name = CL_DEVICE_AVAILABLE;
                type = 2;  // cl_bool
            } else if(strcmp(param, "compiler_avaiable")==0) {
                param_name = CL_DEVICE_COMPILER_AVAILABLE;
                type = 2;
            } else if(strcmp(param, "endian_little")==0) {
                param_name = CL_DEVICE_ENDIAN_LITTLE;
                type = 2;
            } else if(strcmp(param, "error_correction_support")==0) {
                param_name = CL_DEVICE_ERROR_CORRECTION_SUPPORT;
                type = 2;
            } else if(strcmp(param, "image_support")==0) {
                param_name = CL_DEVICE_IMAGE_SUPPORT;
                type = 2;
            } else if(strcmp(param, "device_type")==0) {
                param_name = CL_DEVICE_TYPE;
                type = 3;  // cl_device_type
            } else if(strcmp(param, "address_bits")==0) {
                param_name = CL_DEVICE_ADDRESS_BITS;
                type = 4;  // cl_uint
            } else if(strcmp(param, "global_mem_cacheline_size")==0) {
                param_name = CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE;
                type = 4;
            } else if(strcmp(param, "max_clock_frequency")==0) {
                param_name = CL_DEVICE_MAX_CLOCK_FREQUENCY;
                type = 4;
            } else if(strcmp(param, "max_compute_units")==0) {
                param_name = CL_DEVICE_MAX_COMPUTE_UNITS;
                type = 4;
            } else if(strcmp(param, "max_constant_args")==0) {
                param_name = CL_DEVICE_MAX_CONSTANT_ARGS;
                type = 4;
            } else if(strcmp(param, "max_read_image_args")==0) {
                param_name = CL_DEVICE_MAX_READ_IMAGE_ARGS;
                type = 4;
            } else if(strcmp(param, "max_samplers")==0) {
                param_name = CL_DEVICE_MAX_SAMPLERS;
                type = 4;
            } else if(strcmp(param, "max_work_item_dimensions")==0) {
                param_name = CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS;
                type = 4;
            } else if(strcmp(param, "max_write_image_args")==0) {
                param_name = CL_DEVICE_MAX_WRITE_IMAGE_ARGS;
                type = 4;
            } else if(strcmp(param, "mem_base_addr_align")==0) {
                param_name = CL_DEVICE_MEM_BASE_ADDR_ALIGN;
                type = 4;
            } else if(strcmp(param, "min_data_type_align_size")==0) {
                param_name = CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE;
                type = 4;
            } else if(strcmp(param, "preferred_vector_with_char")==0) {
                param_name = CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR;
                type = 4;
            } else if(strcmp(param, "preferred_vector_with_short")==0) {
                param_name = CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT;
                type = 4;
            } else if(strcmp(param, "preferred_vector_with_int")==0) {
                param_name = CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT;
                type = 4;
            } else if(strcmp(param, "preferred_vector_with_long")==0) {
                param_name = CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG;
                type = 4;
            } else if(strcmp(param, "preferred_vector_with_float")==0) {
                param_name = CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT;
                type = 4;
            } else if(strcmp(param, "preferred_vector_with_double")==0) {
                param_name = CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE;
                type = 4;
            } else if(strcmp(param, "vendor_id")==0) {
                param_name = CL_DEVICE_VENDOR_ID;
                type = 4;
            } else if(strcmp(param, "global_mem_cache_size")==0) {
                param_name = CL_DEVICE_GLOBAL_MEM_CACHE_SIZE;
                type = 5;  // cl_ulong
            } else if(strcmp(param, "global_mem_size")==0) {
                param_name = CL_DEVICE_GLOBAL_MEM_SIZE;
                type = 5;
            } else if(strcmp(param, "local_mem_size")==0) {
                param_name = CL_DEVICE_LOCAL_MEM_SIZE;
                type = 5;
            } else if(strcmp(param, "constant_buffer_size")==0) {
                param_name = CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE;
                type = 5;
            } else if(strcmp(param, "max_mem_alloc_size")==0) {
                param_name = CL_DEVICE_MAX_MEM_ALLOC_SIZE;
                type = 5;
            } else if(strcmp(param, "image2d_max_height")==0) {
                param_name = CL_DEVICE_IMAGE2D_MAX_HEIGHT;
                type = 6;  // size_t
            } else if(strcmp(param, "image2d_max_width")==0) {
                param_name = CL_DEVICE_IMAGE2D_MAX_WIDTH;
                type = 6;
            } else if(strcmp(param, "image3d_max_depth")==0) {
                param_name = CL_DEVICE_IMAGE3D_MAX_DEPTH;
                type = 6;
            } else if(strcmp(param, "image3d_max_height")==0) {
                param_name = CL_DEVICE_IMAGE3D_MAX_HEIGHT;
                type = 6;
            } else if(strcmp(param, "image3d_max_width")==0) {
                param_name = CL_DEVICE_IMAGE3D_MAX_WIDTH;
                type = 6;
            } else if(strcmp(param, "max_parameter_size")==0) {
                param_name = CL_DEVICE_MAX_PARAMETER_SIZE;
                type = 6;
            } else if(strcmp(param, "max_work_group_size")==0) {
                param_name = CL_DEVICE_MAX_WORK_GROUP_SIZE;
                type = 6;
            } else if(strcmp(param, "profiling_timer_resolution")==0) {
                param_name = CL_DEVICE_PROFILING_TIMER_RESOLUTION;
                type = 6;
            } else if(strcmp(param, "global_mem_cache_type")==0) {
                param_name = CL_DEVICE_GLOBAL_MEM_CACHE_TYPE;
                type = 7;  // cl_device_mem_cache_type
            } else if(strcmp(param, "local_mem_type")==0) {
                param_name = CL_DEVICE_LOCAL_MEM_TYPE;
                type = 8;  // cl_device_local_mem_type
            } else if(strcmp(param, "max_work_item_sizes")==0) {
                param_name = CL_DEVICE_MAX_WORK_ITEM_SIZES;
                type = 9;  // size_t[]
            } else {
                if( interp ) {
                   Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                   Tcl_AppendStringsToObj( resultObj, "Invalid parameter",
                                     (char *)NULL );
                }

                return TCL_ERROR;
            }

            error = clGetDeviceInfo(device, param_name, 0, NULL, &size);
            if (error != CL_SUCCESS) {
                if( interp ) {
                   Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                   Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed (get size)",
                                     (char *)NULL );
                }

                return TCL_ERROR;
            }

            if (type == 1) {
                char *buffer = NULL;

                buffer = (char *) ckalloc (sizeof(char) * (size + 1));
                error = clGetDeviceInfo(device, param_name, size + 1, buffer, NULL);
                if (error != CL_SUCCESS) {
                    if (buffer) ckfree(buffer);

                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed",
                                        (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                pResultStr = Tcl_NewStringObj( buffer, -1 );
                if (buffer) ckfree(buffer);
                Tcl_SetObjResult(interp, pResultStr);
            } else if (type == 2) {
                cl_bool value = CL_TRUE;

                error = clGetDeviceInfo(device, param_name, sizeof(value), &value, NULL);
                if (error != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed",
                                        (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                if (value==CL_TRUE) {
                    pResultStr = Tcl_NewIntObj( 1 );
                } else {
                    pResultStr = Tcl_NewIntObj( 0 );
                }
                Tcl_SetObjResult(interp, pResultStr);
            } else if (type == 3) {
                cl_device_type value = CL_DEVICE_TYPE_DEFAULT;

                error = clGetDeviceInfo(device, param_name, sizeof(value), &value, NULL);
                if (error != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed",
                                        (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                if (value==CL_DEVICE_TYPE_DEFAULT) {
                    pResultStr = Tcl_NewStringObj( "default", -1 );
                } else if (value==CL_DEVICE_TYPE_CPU) {
                    pResultStr = Tcl_NewStringObj( "cpu", -1 );
                } else if (value==CL_DEVICE_TYPE_GPU) {
                    pResultStr = Tcl_NewStringObj( "gpu", -1 );
                } else if (value==CL_DEVICE_TYPE_ACCELERATOR) {
                    pResultStr = Tcl_NewStringObj( "accelerator", -1 );
                }
                Tcl_SetObjResult(interp, pResultStr);
            } else if (type == 4) {
                cl_uint value = 0;

                error = clGetDeviceInfo(device, param_name, sizeof(value), &value, NULL);
                if (error != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed",
                                        (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                pResultStr = Tcl_NewIntObj( (int) value );
                Tcl_SetObjResult(interp, pResultStr);
            } else if (type == 5) {
                cl_ulong value = 0;

                error = clGetDeviceInfo(device, param_name, sizeof(value), &value, NULL);
                if (error != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed",
                                        (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                pResultStr = Tcl_NewLongObj( (int) value );
                Tcl_SetObjResult(interp, pResultStr);
            } else if (type == 6) {
                size_t value = 0;

                error = clGetDeviceInfo(device, param_name, sizeof(value), &value, NULL);
                if (error != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed",
                                        (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                pResultStr = Tcl_NewLongObj( (int) value );
                Tcl_SetObjResult(interp, pResultStr);
            } else if (type == 7) {
                cl_device_mem_cache_type value = CL_NONE;

                error = clGetDeviceInfo(device, param_name, sizeof(value), &value, NULL);
                if (error != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed",
                                        (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                if (value==CL_NONE) {
                    pResultStr = Tcl_NewStringObj( "none", -1 );
                } else if (value==CL_READ_ONLY_CACHE) {
                    pResultStr = Tcl_NewStringObj( "read_only_cache", -1 );
                } else if (value==CL_READ_WRITE_CACHE) {
                    pResultStr = Tcl_NewStringObj( "read_write_cache", -1 );
                }
                Tcl_SetObjResult(interp, pResultStr);
            } else if (type == 8) {
                cl_device_local_mem_type value = CL_GLOBAL;

                error = clGetDeviceInfo(device, param_name, sizeof(value), &value, NULL);
                if (error != CL_SUCCESS) {
                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed",
                                        (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                if (value==CL_GLOBAL) {
                    pResultStr = Tcl_NewStringObj( "global", -1 );
                } else if (value==CL_LOCAL) {
                    pResultStr = Tcl_NewStringObj( "local", -1 );
                }
                Tcl_SetObjResult(interp, pResultStr);
            } else if (type == 9) {
                size_t *buffer = NULL;
                int count = 0;

                buffer = (size_t *) ckalloc (sizeof(size_t) * size );
                error = clGetDeviceInfo(device, param_name, size, buffer, NULL);
                if (error != CL_SUCCESS) {
                    if (buffer) ckfree(buffer);

                    if( interp ) {
                        Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                        Tcl_AppendStringsToObj( resultObj, "clGetDeviceInfo failed",
                                        (char *)NULL );
                    }

                    return TCL_ERROR;
                }

                pResultStr = Tcl_NewListObj(0, NULL);
                for (count = 0; count < size; count++) {
                    Tcl_ListObjAppendElement(interp, pResultStr, Tcl_NewIntObj(buffer[count]));
                }

                if (buffer) ckfree(buffer);
                Tcl_SetObjResult(interp, pResultStr);
            }

            break;
        }
        case FUNC_CLOSE: {
            if( objc != 2 ){
                Tcl_WrongNumArgs(interp, 2, objv, 0);
                return TCL_ERROR;
            }

            Tcl_MutexLock(&myMutex);
            if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
            Tcl_MutexUnlock(&myMutex);

            Tcl_DeleteCommand(interp, handle);
            Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

            break;
        }
    }

    return TCL_OK;
}


static int getDeviceID(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    cl_int err;
    cl_platform_id platform;
    cl_device_id device;
    cl_device_type device_type = CL_DEVICE_TYPE_DEFAULT;
    cl_uint number;
    Tcl_HashEntry *newHashEntryPtr;
    char handleName[16 + TCL_INTEGER_SPACE];
    Tcl_Obj *pResultStr = NULL;
    int newvalue;
    Tcl_HashEntry *hashEntryPtr;
    char *handle = NULL;
    char *type = NULL;
    int len = 0;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
      tsdPtr->initialized = 1;
      tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
      Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    if (objc !=3 && objc != 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "platformID type ?number?");
        return TCL_ERROR;
    }

    handle = Tcl_GetStringFromObj(objv[1], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, handle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid platform handleID ",
                                    handle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    platform = (cl_platform_id) Tcl_GetHashValue( hashEntryPtr );

    type = Tcl_GetStringFromObj(objv[2], &len);
    if( !type || len < 1 ){
        return TCL_ERROR;
    }

    if(strcmp(type, "default")==0) {
        device_type = CL_DEVICE_TYPE_DEFAULT;
    } else if(strcmp(type, "cpu")==0) {
        device_type = CL_DEVICE_TYPE_CPU;
    } else if(strcmp(type, "gpu")==0) {
        device_type = CL_DEVICE_TYPE_GPU;
    } else if(strcmp(type, "accelerator")==0) {
        device_type = CL_DEVICE_TYPE_ACCELERATOR;
    } else if(strcmp(type, "all")==0) {
        device_type = CL_DEVICE_TYPE_ALL;
    } else {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "Invalid parameter",
                                (char *)NULL );
        }

        return TCL_ERROR;
    }

    if (objc == 3) {
        cl_uint num;

        err = clGetDeviceIDs( platform, device_type, 0, NULL, &num );
        if (err != CL_SUCCESS) {
            return TCL_ERROR;
        }

        Tcl_SetObjResult(interp, Tcl_NewIntObj( (int) num ));
    } else {
        if(Tcl_GetIntFromObj(interp, objv[3], (int *) &number) != TCL_OK) {
            return TCL_ERROR;
        }

        err = clGetDeviceIDs( platform, device_type, number, &device, NULL );
        if (err != CL_SUCCESS) {
            if( interp ) {
                Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                Tcl_AppendStringsToObj( resultObj, "clGetDeviceIDs failed",
                                        (char *)NULL );
            }
            return TCL_ERROR;
        }

        Tcl_MutexLock(&myMutex);
        sprintf( handleName, "cl-device%d", tsdPtr->device_count++ );

        pResultStr = Tcl_NewStringObj( handleName, -1 );

        newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->cl_hashtblPtr, handleName, &newvalue);
        Tcl_SetHashValue(newHashEntryPtr, device);
        Tcl_MutexUnlock(&myMutex);

        Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) DEVICE_FUNCTION,
            (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

        Tcl_SetObjResult(interp, pResultStr);
    }

    return TCL_OK;
}


/*
 * For platform API
 */
static int PLATFORM_FUNCTION(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    int choice;
    Tcl_HashEntry *hashEntryPtr;
    char *handle;
    cl_platform_id platform;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    static const char *FUNC_strs[] = {
        "info",
        "close",
        0
    };

    enum FUNC_enum {
        FUNC_INFO,
        FUNC_CLOSE,
    };

    if( objc < 2 ){
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }

    if( Tcl_GetIndexFromObj(interp, objv[1], FUNC_strs, "option", 0, &choice) ){
        return TCL_ERROR;
    }

    handle = Tcl_GetStringFromObj(objv[0], 0);
    hashEntryPtr = Tcl_FindHashEntry( tsdPtr->cl_hashtblPtr, handle );
    if( !hashEntryPtr ) {
        if( interp ) {
            Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
            Tcl_AppendStringsToObj( resultObj, "invalid platformID handle ",
                                    handle, (char *)NULL );
        }

        return TCL_ERROR;
    }

    platform = (cl_platform_id) Tcl_GetHashValue( hashEntryPtr );

    switch( (enum FUNC_enum)choice ){
        case FUNC_INFO: {
            char *param = NULL;
            char *buffer = NULL;
            int len = 0;
            cl_platform_info param_name = CL_PLATFORM_PROFILE;
            cl_int error;
            size_t size;
            Tcl_Obj *pResultStr = NULL;

            if( objc != 3 ){
                Tcl_WrongNumArgs(interp, 2, objv, "param_name");
                return TCL_ERROR;
            }

            param = Tcl_GetStringFromObj(objv[2], &len);
            if( !param || len < 1 ){
                return TCL_ERROR;
            }

            if(strcmp(param, "profile")==0) {
                param_name = CL_PLATFORM_PROFILE;
            } else if(strcmp(param, "version")==0) {
                param_name = CL_PLATFORM_VERSION;
            } else if(strcmp(param, "name")==0) {
                param_name = CL_PLATFORM_NAME;
            } else if(strcmp(param, "vendor")==0) {
                param_name = CL_PLATFORM_VENDOR;
            } else if(strcmp(param, "extensions")==0) {
                param_name = CL_PLATFORM_EXTENSIONS;
            }

            error = clGetPlatformInfo(platform, param_name, 0, NULL, &size);
            if (error != CL_SUCCESS) {
                if( interp ) {
                   Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                   Tcl_AppendStringsToObj( resultObj, "clGetPlatformInfo failed (get size)",
                                     (char *)NULL );
                }

                return TCL_ERROR;
            }

            buffer = (char *) ckalloc (sizeof(char) * (size + 1));
            error = clGetPlatformInfo(platform, param_name, size + 1, buffer, NULL);
            if (error != CL_SUCCESS) {
                if (buffer) ckfree(buffer);

                if( interp ) {
                    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
                    Tcl_AppendStringsToObj( resultObj, "clGetPlatformInfo failed",
                                    (char *)NULL );
                }

                return TCL_ERROR;
            }

            pResultStr = Tcl_NewStringObj( buffer, -1 );
            if (buffer) ckfree(buffer);
            Tcl_SetObjResult(interp, pResultStr);

            break;
        }
        case FUNC_CLOSE: {
            if( objc != 2 ){
                Tcl_WrongNumArgs(interp, 2, objv, 0);
                return TCL_ERROR;
            }

            Tcl_MutexLock(&myMutex);
            if( hashEntryPtr )  Tcl_DeleteHashEntry(hashEntryPtr);
            Tcl_MutexUnlock(&myMutex);

            Tcl_DeleteCommand(interp, handle);
            Tcl_SetObjResult(interp, Tcl_NewIntObj( 0 ));

            break;
        }
    }

    return TCL_OK;
}


static int getPlatformID(void *cd, Tcl_Interp *interp, int objc,Tcl_Obj *const*objv){
    cl_int err;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
      Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
      tsdPtr->initialized = 1;
      tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
      Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);
    }

    if (objc !=1 && objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "?number?");
        return TCL_ERROR;
    }

    if (objc == 1) {
        cl_uint num;

        err = clGetPlatformIDs( 0, NULL, &num );
        if (err != CL_SUCCESS) {
            return TCL_ERROR;
        }

        Tcl_SetObjResult(interp, Tcl_NewIntObj( (int) num ));
        return TCL_OK;
    } else {
        cl_platform_id platform;
        cl_uint number;
        Tcl_HashEntry *newHashEntryPtr;
        char handleName[16 + TCL_INTEGER_SPACE];
        Tcl_Obj *pResultStr = NULL;
        int newvalue;

        if(Tcl_GetIntFromObj(interp, objv[1], (int *) &number) != TCL_OK) {
            return TCL_ERROR;
        }

        err = clGetPlatformIDs( number, &platform, NULL );
        if (err != CL_SUCCESS) {
            return TCL_ERROR;
        }

        Tcl_MutexLock(&myMutex);
        sprintf( handleName, "cl-platform%d", tsdPtr->platform_count++ );

        pResultStr = Tcl_NewStringObj( handleName, -1 );

        newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->cl_hashtblPtr, handleName, &newvalue);
        Tcl_SetHashValue(newHashEntryPtr, platform);
        Tcl_MutexUnlock(&myMutex);

        Tcl_CreateObjCommand(interp, handleName, (Tcl_ObjCmdProc *) PLATFORM_FUNCTION,
            (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

        Tcl_SetObjResult(interp, pResultStr);
    }

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Opencl_Init --
 *
 * Results:
 *  A standard Tcl result
 *
 * Side effects:
 *  The Opencl package is created.
 *
 *----------------------------------------------------------------------
 */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */
DLLEXPORT int
Opencl_Init(Tcl_Interp *interp)
{
    Tcl_Namespace *nsPtr; /* pointer to hold our own new namespace */

    if (Tcl_InitStubs(interp, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }

    if (Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION) != TCL_OK) {
        return TCL_ERROR;
    }

    nsPtr = Tcl_CreateNamespace(interp, NS, NULL, NULL);
    if (nsPtr == NULL) {
        return TCL_ERROR;
    }

    /*
     *  Tcl_GetThreadData handles the auto-initialization of all data in
     *  the ThreadSpecificData to NULL at first time.
     */
    Tcl_MutexLock(&myMutex);
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->cl_hashtblPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->cl_hashtblPtr, TCL_STRING_KEYS);

        tsdPtr->platform_count = 0;
    }
    Tcl_MutexUnlock(&myMutex);

    /* Add a thread exit handler to delete hash table */
    Tcl_CreateThreadExitHandler(OPENCL_Thread_Exit, (ClientData)NULL);

    Tcl_CreateObjCommand(interp, NS "::getPlatformID",
        (Tcl_ObjCmdProc *) getPlatformID,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, NS "::getDeviceID",
        (Tcl_ObjCmdProc *) getDeviceID,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, NS "::createContext",
        (Tcl_ObjCmdProc *) createContext,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, NS "::createCommandQueue",
        (Tcl_ObjCmdProc *) createCommandQueue,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, NS "::createProgramWithSource",
        (Tcl_ObjCmdProc *) createProgramWithSource,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, NS "::createProgramWithBinary",
        (Tcl_ObjCmdProc *) createProgramWithBinary,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, NS "::createKernel",
        (Tcl_ObjCmdProc *) createKernel,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_CreateObjCommand(interp, NS "::createBuffer",
        (Tcl_ObjCmdProc *) createBuffer,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}
#ifdef __cplusplus
}
#endif  /* __cplusplus */
