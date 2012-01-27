/**
 * Copyright(c) 2011 Trusted Logic.   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Trusted Logic nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
* File            : ssdi.h
*
* Original-Author : Trusted Logic S.A.
*
* Created         : May 31, 2006
*/

/**
 * SSDI specification 3.0 header file.
 */

#ifndef __SSDI_H__
#define __SSDI_H__

#include "s_type.h"
#include "s_error.h"

#ifndef SSDI_EXPORT
#ifdef SMODULE_IMPLEMENTATION
#define SSDI_EXPORT S_DLL_EXPORT
#else
#define SSDI_EXPORT S_DLL_IMPORT
#endif
#endif

#ifndef SRVX_EXPORT
#define SRVX_EXPORT S_DLL_EXPORT
#endif

/*------------------------------------------------------------------------------
         SSDI Types
------------------------------------------------------------------------------*/

typedef struct S_PROPERTY
{
   char* pName;
   char* pValue ;
} S_PROPERTY;

typedef struct S_TIME_LIMIT
{
   uint32_t nTime1;
   uint32_t nTime2;
} S_TIME_LIMIT;

typedef struct S_CALENDAR
{
   int32_t nYear;
   int32_t nMonth;
   int32_t nDayOfWeek;
   int32_t nDay;
   int32_t nHour;
   int32_t nMinute;
   int32_t nSecond;
} S_CALENDAR;

typedef enum
{
   S_FILE_SEEK_SET = 0,
   S_FILE_SEEK_CUR,
   S_FILE_SEEK_END
} S_WHENCE;

typedef struct S_FILE_INFO
{
   char*    pName;
   uint32_t nSize;
   uint32_t nNameLength;
}S_FILE_INFO;

typedef struct
{
   void* pBuffer;
   uint32_t nSize;
}
S_PARAM_MEMREF;

typedef struct
{
   uint32_t a;
   uint32_t b;
}
S_PARAM_VALUE;

typedef union
{
   S_PARAM_MEMREF  memref;
   S_PARAM_VALUE   value;
}
S_PARAM;


/*------------------------------------------------------------------------------
         Constants
------------------------------------------------------------------------------*/
#define S_TIMEOUT_INFINITE                         0xFFFFFFFF

/* storage private to the service */
#define S_FILE_STORAGE_PRIVATE                     0x00000001

#ifdef SUPPORT_RPMB_PARTITION
/* storage on rpmb */
#define S_FILE_STORAGE_RPMB                        0x00000002
#endif

/* Cryptoki slotID constants */
#define S_CRYPTOKI_KEYSTORE_PRIVATE                0x00000001
#define S_CRYPTOKI_KEYSTORE_PRIVATE_USER           0x00004004

/* SFile constants */
#define S_FILE_FLAG_ACCESS_READ                    0x0001
#define S_FILE_FLAG_ACCESS_WRITE                   0x0002
#define S_FILE_FLAG_ACCESS_WRITE_META              0x0004

#define S_FILE_FLAG_SHARE_READ                     0x0010
#define S_FILE_FLAG_SHARE_WRITE                    0x0020

#define S_FILE_FLAG_CREATE                         0x0200
#define S_FILE_FLAG_EXCLUSIVE                      0x0400

#define S_FILE_NAME_MAX                            0x40
#define S_FILE_MAX_POSITION                        0xFFFFFFFF

/* SDate constants */
#define S_DATE_STATUS_NOT_SET                      0xFFFF5000
#define S_DATE_STATUS_NEEDS_RESET                  0xFFFF5001
#define S_DATE_STATUS_SET                          0x00000000

/* Login types */
#define S_LOGIN_PUBLIC                             0x00000000
#define S_LOGIN_USER                               0x00000001
#define S_LOGIN_GROUP                              0x00000002
#define S_LOGIN_APPLICATION                        0x00000004
#define S_LOGIN_APPLICATION_USER                   0x00000005
#define S_LOGIN_APPLICATION_GROUP                  0x00000006
#define S_LOGIN_AUTHENTICATION                     0x80000000
#define S_LOGIN_PRIVILEGED                         0x80000002
#define S_LOGIN_CLIENT_IS_SERVICE                  0xF0000000
#define S_LOGIN_SYSTEM                             0xF0000001

/* Parameter types */
#define S_PARAM_TYPE_NONE          0x0
#define S_PARAM_TYPE_VALUE_INPUT   0x1
#define S_PARAM_TYPE_VALUE_OUTPUT  0x2
#define S_PARAM_TYPE_VALUE_INOUT   0x3
#define S_PARAM_TYPE_MEMREF_INPUT  0x5
#define S_PARAM_TYPE_MEMREF_OUTPUT 0x6
#define S_PARAM_TYPE_MEMREF_INOUT  0x7

#define S_PARAM_TYPE_INPUT_FLAG    0x1
#define S_PARAM_TYPE_OUTPUT_FLAG   0x2
#define S_PARAM_TYPE_MEMREF_FLAG   0x4

#define S_PARAM_TYPES(t0,t1,t2,t3)  ((t0) | ((t1) << 4) | ((t2) << 8) | ((t3) << 12))
#define S_PARAM_TYPE_GET(t, i) (((t) >> (i*4)) & 0xF)

#define S_ORIGIN_API         1
#define S_ORIGIN_COMMS       2
#define S_ORIGIN_TEE         3
#define S_ORIGIN_TRUSTED_APP 4

/*------------------------------------------------------------------------------
         Implementation Functions
------------------------------------------------------------------------------*/

S_RESULT SSDI_EXPORT SImplementationGetAllProperties(
                                 S_PROPERTY** ppProperties,
                                 uint32_t* pnPropertiesCount);

S_RESULT SSDI_EXPORT SImplementationGetProperty(const char* pName,
                                                char** ppValue);

S_RESULT SSDI_EXPORT SImplementationGetPropertyAsInt(const char* pName,
                                                     uint32_t* pnValue);

S_RESULT SSDI_EXPORT SImplementationGetPropertyAsBool(const char* pName,
                                                      bool* pbValue);

/*------------------------------------------------------------------------------
         Service Functions
------------------------------------------------------------------------------*/

S_RESULT SSDI_EXPORT SServiceGetAllProperties(
                                 OUT S_PROPERTY** ppProperties,
                                 OUT uint32_t* pnPropertiesCount);

S_RESULT SSDI_EXPORT SServiceGetProperty (
                             IN const char* pName,
                             OUT char** ppValue);

S_RESULT SSDI_EXPORT SServiceGetPropertyAsInt (
                                  IN const char* pName,
                                  OUT uint32_t* pnValue);

S_RESULT SSDI_EXPORT SServiceGetPropertyAsBool(
                                  IN const char* pName,
                                  OUT bool* pbValue);

/*------------------------------------------------------------------------------
         Instance Functions
------------------------------------------------------------------------------*/

void SSDI_EXPORT SInstanceSetData(
                      void* pInstanceData);

void SSDI_EXPORT *SInstanceGetData(void);

/*------------------------------------------------------------------------------
         Session Functions
------------------------------------------------------------------------------*/

void SSDI_EXPORT SSessionGetClientID(
                         S_UUID* pClientID);

S_RESULT SSDI_EXPORT SSessionGetAllClientProperties(
                                    OUT uint32_t* pnPropertyCount,
                                    OUT S_PROPERTY** ppPropertyArray);

S_RESULT SSDI_EXPORT SSessionGetClientProperty(
                                  IN const char* pName,
                                  OUT char** ppValue);

S_RESULT SSDI_EXPORT SSessionGetClientPropertyAsInt (
                                        IN const char* pName,
                                        OUT uint32_t* pnValue);

S_RESULT SSDI_EXPORT SSessionGetClientPropertyAsBool (
                                         IN const char* pName,
                                         OUT bool* pnValue);

/*------------------------------------------------------------------------------
         Memory Management Functions
------------------------------------------------------------------------------*/

void SSDI_EXPORT *SMemAlloc(uint32_t size);

void SSDI_EXPORT *SMemRealloc(void* ptr, uint32_t newSize);

void SSDI_EXPORT SMemFree(void *ptr);

void SSDI_EXPORT *SMemMove(void *dest, const void *src, uint32_t n);

int32_t SSDI_EXPORT SMemCompare(const void *s1, const void *s2, uint32_t n);

void SSDI_EXPORT *SMemFill(void *s, uint32_t c, uint32_t n);

void SSDI_EXPORT *SMemAllocEx(uint32_t nPoolID, uint32_t nSize);

S_RESULT SMemDup(void *src, uint32_t n, void **dest);

/*------------------------------------------------------------------------------
         Trace & Debug Functions
------------------------------------------------------------------------------*/
void SSDI_EXPORT _SLogTrace(
               const char *message,
               ... /* arguments */);
void SSDI_EXPORT _SLogWarning(
               const char *message,
               ... /* arguments */);
void SSDI_EXPORT _SLogError(
               const char *message,
               ... /* arguments */);

#ifdef __SSDI_USE_TRACE_EX
#include "ssdi_trace_ex.h"
#else

#ifndef SSDI_NO_TRACE

#define SLogTrace _SLogTrace
#define SLogWarning _SLogWarning
#define SLogError _SLogError

#else /* defined(SSDI_NO_TRACE) */

/* Note that the following code depends on the compiler's supporting variadic macros */
#define SLogTrace(...)   do ; while(false)
#define SLogWarning(...) do ; while(false)
#define SLogError(...)   do ; while(false)

#endif /* !defined(SSDI_NO_TRACE) */

#endif /* __SSDI_USE_TRACE_EX */

void SSDI_EXPORT _SAssertionFailed(
      const char* pFileName,
      uint32_t nLine,
      const char* pExpression);

#ifdef SSDI_DEBUG
#define SAssert(test)                                     \
   do                                                       \
   {                                                        \
      if (!(test))                                          \
      {                                                     \
         _SAssertionFailed(__FILE__, __LINE__, #test);   \
      }                                                     \
   }                                                        \
   while (0)
#else /* !defined(SSDI_DEBUG) */
#define SAssert(test)
#endif /* defined(SSDI_DEBUG) */

#define S_VAR_NOT_USED(variable) do{(void)(variable);}while(0);

/*------------------------------------------------------------------------------
         Time Utility
------------------------------------------------------------------------------*/
void SSDI_EXPORT STimeGetLimit(
           uint32_t      nTimeout,
       OUT S_TIME_LIMIT* pTimeLimit );


/*------------------------------------------------------------------------------
         Thread Functions
------------------------------------------------------------------------------*/
S_RESULT SSDI_EXPORT SThreadCreate(
                      OUT S_HANDLE* phThread,
                      uint32_t      stackSize,
                      uint32_t     (*pEntryPoint)(void*),
                      IN void*      pThreadArg);

S_RESULT SSDI_EXPORT SThreadJoin(
                    S_HANDLE        hThread,
                    uint32_t*       pnExitCode,
                    const S_TIME_LIMIT*   pTimeLimit);

void SSDI_EXPORT SThreadYield(void);

S_RESULT SSDI_EXPORT SThreadSleep(const S_TIME_LIMIT* pTimeLimit);

void SSDI_EXPORT SThreadCancel(S_HANDLE hThread, uint32_t nReserved);

bool SSDI_EXPORT SThreadIsCancelled (void* pReserved);

void SSDI_EXPORT SThreadResetCancel(void);

void SSDI_EXPORT SThreadMaskCancellation ( bool bMask );

/*------------------------------------------------------------------------------
         Semaphore Functions
------------------------------------------------------------------------------*/

S_RESULT SSDI_EXPORT SSemaphoreCreate (
                          uint32_t  initialCount,
                          S_HANDLE* phSemaphore);

S_RESULT SSDI_EXPORT SSemaphoreAcquire(S_HANDLE hSemaphore, const S_TIME_LIMIT* pTimeLimit);

void SSDI_EXPORT SSemaphoreRelease(S_HANDLE hSemaphore);

/*------------------------------------------------------------------------------
         File System Functions
------------------------------------------------------------------------------*/

S_RESULT SSDI_EXPORT SFileOpen(
                  uint32_t  nStorageID,
                  const char     *pFilename,
                  uint32_t  nFlags,
                  uint32_t  nReserved,
                  S_HANDLE *phFile);

S_RESULT SSDI_EXPORT SFileRead(S_HANDLE         hFile,
                  uint8_t*         pBuffer,
                  uint32_t         nSize,
                  uint32_t*        pnCount);

S_RESULT SSDI_EXPORT SFileWrite (S_HANDLE         hFile,
                    const uint8_t*         pBuffer,
                    uint32_t         nSize);

S_RESULT SSDI_EXPORT SFileTruncate(S_HANDLE      hFile,
                      uint32_t       nSize);

S_RESULT SSDI_EXPORT SFileSeek(S_HANDLE     hFile,
                  int32_t      nOffset,
                  S_WHENCE     eWhence);

uint32_t SSDI_EXPORT SFileTell(S_HANDLE        hFile);

bool SSDI_EXPORT SFileEOF(S_HANDLE   hFile);

S_RESULT SSDI_EXPORT SFileCloseAndDelete(S_HANDLE hFile);

S_RESULT SSDI_EXPORT SFileRename(S_HANDLE hFile, const char* pNewFilename);

S_RESULT SSDI_EXPORT SFileGetSize(uint32_t     nStorageID,
                     const char*        pFilename,
                     uint32_t*    pnFileSize);

S_RESULT SSDI_EXPORT SFileEnumerationStart (
                               uint32_t  nStorageID,
                               const char*     pFilenamePattern,
                               uint32_t  nReserved1,
                               uint32_t  nReserved2,
                               S_HANDLE* phFileEnumeration);

S_RESULT SSDI_EXPORT SFileEnumerationGetNext (
                                 S_HANDLE           hFileEnumeration,
                                 OUT S_FILE_INFO**  ppFileInfo);

/*------------------------------------------------------------------------------
         Date Functions
------------------------------------------------------------------------------*/

S_RESULT SSDI_EXPORT SDateSet (
           int32_t  nSeconds,
           uint32_t nReserved);

S_RESULT SSDI_EXPORT SDateGet(
       OUT int32_t*  pnSeconds,
       OUT uint32_t* pnDateStatus,
       uint32_t      nReserved );

int32_t SSDI_EXPORT SClockGet(void);

S_RESULT SSDI_EXPORT SDateConvertSecondsToCalendar(
        IN int32_t     nSeconds,
        IN const S_CALENDAR* pOrigin,
       OUT S_CALENDAR* pDate );

S_RESULT SSDI_EXPORT SDateConvertCalendarToSeconds(
        IN const S_CALENDAR* pOrigin,
        IN const S_CALENDAR* pDate,
       OUT int32_t*    pnSeconds);

/*------------------------------------------------------------------------------
         Handle Functions
------------------------------------------------------------------------------*/
void SSDI_EXPORT SHandleClose ( S_HANDLE hHandle);

/*------------------------------------------------------------------------------
         Crypto API
------------------------------------------------------------------------------*/

#define PKCS11_EXPORT SSDI_EXPORT

#include "pkcs11.h"

/*------------------------------------------------------------------------------
         Cryptoki Update Shortcut
------------------------------------------------------------------------------*/

#define S_UPDATE_SHORTCUT_FLAG_AGGRESSIVE 0x00000001

CK_RV SSDI_EXPORT CV_ActivateUpdateShortcut2(
   CK_SESSION_HANDLE hCryptokiSession,
   uint32_t nCommandID,
   uint32_t nFlags,
   uint32_t nReserved);

void SSDI_EXPORT CV_DeactivateUpdateShortcut(
  CK_SESSION_HANDLE hCryptokiSession);


/*------------------------------------------------------------------------------
         Panic Function
------------------------------------------------------------------------------*/

void SSDI_EXPORT SPanic(uint32_t nReserved);

/*------------------------------------------------------------------------------
         SXControl functions
------------------------------------------------------------------------------*/
S_RESULT SSDI_EXPORT SXControlOpenClientSession (
           const S_UUID* pDestination,
           S_TIME_LIMIT* pDeadline,
           uint32_t nParamTypes,
           S_PARAM pParams[4],
           OUT S_HANDLE* phSessionHandle,
           uint32_t* pnReturnOrigin);

S_RESULT SSDI_EXPORT SXControlInvokeCommand (
           S_HANDLE hSessionHandle,
           S_TIME_LIMIT* pDeadline,
           uint32_t nCommandID,
           uint32_t nParamTypes,
           S_PARAM pParams[4],
           uint32_t* pnReturnOrigin);

/*------------------------------------------------------------------------------
         SRVX Entry Points
------------------------------------------------------------------------------*/

extern S_RESULT SRVX_EXPORT SRVXCreate(void);
extern void     SRVX_EXPORT SRVXDestroy(void);
extern S_RESULT SRVX_EXPORT SRVXOpenClientSession(uint32_t nParamTypes,
                                           IN OUT S_PARAM pParams[4],
                                           OUT void** ppSessionContext);
extern S_RESULT SRVX_EXPORT SRVXInvokeCommand(IN OUT void* pSessionContext,
                                       uint32_t nCommandID,
                                       uint32_t nParamTypes,
                                       IN OUT S_PARAM pParams[4]);
extern void SRVX_EXPORT SRVXCloseClientSession(IN OUT void* pSessionContext);

#include "ssdi_v2_compat.h"

#endif /* __SSDI_H__ */
