#ifndef _OPENCL
#define _OPENCL

#include <tcl.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#if defined(CL_VERSION_3_0) 
#define TCLOPENCL_CL_VERSION 0x3000 
#elif defined(CL_VERSION_2_2) 
#define TCLOPENCL_CL_VERSION 0x2020 
#elif defined(CL_VERSION_2_1) 
#define TCLOPENCL_CL_VERSION 0x2010 
#elif defined(CL_VERSION_2_0) 
#define TCLOPENCL_CL_VERSION 0x2000 
#elif defined(CL_VERSION_1_2) 
#define TCLOPENCL_CL_VERSION 0x1020 
#elif defined(CL_VERSION_1_1) 
#define TCLOPENCL_CL_VERSION 0x1010 
#else 
#define TCLOPENCL_CL_VERSION 0x1000 
#endif 

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Only the _Init function is exported.
 */

extern DLLEXPORT int	Opencl_Init(Tcl_Interp * interp);

/*
 * end block for C++
 */

#ifdef __cplusplus
}
#endif

#endif /* _OPENCL */
