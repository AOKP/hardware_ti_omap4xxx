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
 * File            : smapi.h
 * Last-Author     : Trusted Logic S.A.
 * Created         : March 15, 2003
 */

#ifndef __SMAPI_H__
#define __SMAPI_H__

#ifdef __cplusplus
extern "C" {
#endif


/*------------------------------------------------------------------------------
   Includes
------------------------------------------------------------------------------*/

#include "s_type.h"
#include "s_error.h"
/*------------------------------------------------------------------------------
   SMAPI Types
------------------------------------------------------------------------------*/


#ifdef SM_EXPORT_IMPLEMENTATION
#define SM_EXPORT S_DLL_EXPORT
#else
#define SM_EXPORT S_DLL_IMPORT
#endif

typedef struct SM_PROPERTY
{
   wchar_t* pName;
   wchar_t* pValue;
} SM_PROPERTY;

typedef struct SM_TIME_LIMIT
{
   uint32_t nHighTime;
   uint32_t nLowTime;
} SM_TIME_LIMIT;

/*------------------------------------------------------------------------------
   Constants
------------------------------------------------------------------------------*/


#define SM_API_VERSION  0x03000000

#define SM_CONTROL_MODE_USER               ( (uint32_t)0x00000002 )
#define SM_CONTROL_MODE_MANAGER            ( (uint32_t)0x00000008 )
#define SM_CONTROL_MODE_EXCLUSIVE_MANAGER  ( (uint32_t)0x00000010 )

#define SM_LOGIN_PUBLIC             ( (uint32_t)0x00000000 )
#define SM_LOGIN_OS_IDENTIFICATION  ( (uint32_t)0x00000005 )
#define SM_LOGIN_AUTHENTICATION                             ( (uint32_t)0x80000000 )
#define SM_LOGIN_AUTHENTICATION_FALLBACK_OS_IDENTIFICATION  ( (uint32_t)0x80000001 )
#define SM_LOGIN_PRIVILEGED  ( (uint32_t)0x80000002 )

#define SM_MEMORY_ACCESS_CLIENT_WRITE_SERVICE_READ  ( (uint32_t)0x00000001 )
#define SM_MEMORY_ACCESS_CLIENT_READ_SERVICE_WRITE  ( (uint32_t)0x00000002 )

#define SMX_MEMORY_ACCESS_DIRECT                    ( (uint32_t)0x80000000 )
#define SMX_MEMORY_ACCESS_DIRECT_FORCE              ( (uint32_t)0x40000000 )

#define SM_INFINITE_TIMEOUT  ( (uint32_t)0xFFFFFFFF )

#define SM_NULL_ELEMENT  ( (uint32_t)0xFFFFFFFF )

/*------------------------------------------------------------------------------
   Functions and Macros
------------------------------------------------------------------------------*/

SM_EXPORT SM_ERROR SMDeviceCreateContext(
      const wchar_t* pDeviceName,
      uint32_t       nReserved,
      SM_HANDLE*     phDevice);

SM_EXPORT SM_ERROR SMDeviceDeleteContext(
      SM_HANDLE hDevice);

SM_EXPORT void SMFree(
      SM_HANDLE hElement,
      void* pBuffer);

SM_EXPORT SM_ERROR SMStubGetTimeLimit(
      SM_HANDLE      hElement,
      uint32_t       nTimeout,
      SM_TIME_LIMIT* pTimeLimit);

SM_EXPORT SM_ERROR SMStubPrepareOpenOperation(
      SM_HANDLE            hDevice,
      uint32_t             nLoginType,
      const void*          pLoginInfo,
      const SM_UUID*       pidService,
      uint32_t             nControlMode,
      const SM_TIME_LIMIT* pTimeLimit,
      uint32_t             nReserved1,
      uint32_t             nReserved2,
      SM_HANDLE*           phClientSession,
      SM_HANDLE*           phParameterEncoder,
      SM_HANDLE*           phOperation);

SM_EXPORT SM_ERROR SMStubPrepareInvokeOperation(
      SM_HANDLE            hClientSession,
      uint32_t             nCommandIdentifier,
      const SM_TIME_LIMIT* pTimeLimit,
      uint32_t             nReserved1,
      uint32_t             nReserved2,
      SM_HANDLE*           phParameterEncoder,
      SM_HANDLE*           phOperation);

SM_EXPORT SM_ERROR SMStubPrepareCloseOperation(
      SM_HANDLE  hClientSession,
      uint32_t   nReserved1,
      uint32_t   nReserved2,
      SM_HANDLE* phParameterEncoder,
      SM_HANDLE* phOperation);

SM_EXPORT SM_ERROR SMStubPerformOperation(
      SM_HANDLE  hOperation,
      uint32_t   nReserved,
      SM_ERROR*  pnServiceErrorCode,
      SM_HANDLE* phAnswerDecoder);

SM_EXPORT SM_ERROR SMStubCancelOperation(
      SM_HANDLE hOperation);

SM_EXPORT SM_ERROR SMStubReleaseOperation(
      SM_HANDLE hOperation);

SM_EXPORT SM_ERROR SMStubAllocateSharedMemory(
      SM_HANDLE  hClientSession,
      uint32_t   nLength,
      uint32_t   nFlags,
      uint32_t   nReserved,
      void**     ppBlock,
      SM_HANDLE* phBlockHandle);

SM_EXPORT SM_ERROR SMStubRegisterSharedMemory(
      SM_HANDLE  hClientSession,
      void*      pBuffer,
      uint32_t   nBufferLength,
      uint32_t   nFlags,
      uint32_t   nReserved,
      SM_HANDLE* phBlockHandle);

SM_EXPORT SM_ERROR SMStubReleaseSharedMemory(
      SM_HANDLE hBlockHandle);

SM_EXPORT void SMStubEncoderWriteUint8(
      SM_HANDLE hEncoder,
      uint8_t   nValue);

SM_EXPORT void SMStubEncoderWriteUint16(
      SM_HANDLE hEncoder,
      uint16_t  nValue);

SM_EXPORT void SMStubEncoderWriteUint32(
      SM_HANDLE hEncoder,
      uint32_t  nValue);

SM_EXPORT void SMStubEncoderWriteBoolean(
      SM_HANDLE hEncoder,
      bool      nValue);

SM_EXPORT void SMStubEncoderWriteHandle(
      SM_HANDLE hEncoder,
      SM_HANDLE hValue);

SM_EXPORT void SMStubEncoderWriteString(
      SM_HANDLE      hEncoder,
      const wchar_t* pValue);

SM_EXPORT void SMStubEncoderWriteUint8Array(
      SM_HANDLE      hEncoder,
      uint32_t       nArrayLength,
      const uint8_t* pnArray);

SM_EXPORT void SMStubEncoderWriteUint16Array(
      SM_HANDLE       hEncoder,
      uint32_t        nArrayLength,
      const uint16_t* pnArray);

SM_EXPORT void SMStubEncoderWriteUint32Array(
      SM_HANDLE       hEncoder,
      uint32_t        nArrayLength,
      const uint32_t* pnArray);

SM_EXPORT void SMStubEncoderWriteHandleArray(
      SM_HANDLE        hEncoder,
      uint32_t         nArrayLength,
      const SM_HANDLE* pnArray);

SM_EXPORT void SMStubEncoderWriteMemoryReference(
      SM_HANDLE hEncoder,
      SM_HANDLE hBlock,
      uint32_t  nOffset,
      uint32_t  nLength,
      uint32_t  nFlags);

SM_EXPORT void SMStubEncoderOpenSequence(
      SM_HANDLE hEncoder);

SM_EXPORT void SMStubEncoderCloseSequence(
      SM_HANDLE hEncoder);

SM_EXPORT SM_ERROR SMStubDecoderGetError(
      SM_HANDLE hDecoder);

SM_EXPORT bool SMStubDecoderHasData(
      SM_HANDLE hDecoder);

SM_EXPORT uint8_t SMStubDecoderReadUint8(
      SM_HANDLE hDecoder);

SM_EXPORT uint16_t SMStubDecoderReadUint16(
      SM_HANDLE hDecoder);

SM_EXPORT uint32_t SMStubDecoderReadUint32(
      SM_HANDLE hDecoder);

SM_EXPORT bool SMStubDecoderReadBoolean(
      SM_HANDLE hDecoder);

SM_EXPORT SM_HANDLE SMStubDecoderReadHandle(
      SM_HANDLE hDecoder);

SM_EXPORT wchar_t* SMStubDecoderReadString(
      SM_HANDLE hDecoder);

SM_EXPORT uint8_t* SMStubDecoderReadUint8Array(
      SM_HANDLE hDecoder,
      uint32_t* pnArrayLength);

SM_EXPORT uint16_t* SMStubDecoderReadUint16Array(
      SM_HANDLE hDecoder,
      uint32_t* pnArrayLength);

SM_EXPORT uint32_t* SMStubDecoderReadUint32Array(
      SM_HANDLE hDecoder,
      uint32_t* pnArrayLength);

SM_EXPORT SM_HANDLE* SMStubDecoderReadHandleArray(
      SM_HANDLE hDecoder,
      uint32_t* pnArrayLength);

SM_EXPORT uint32_t SMStubDecoderReadArrayLength(
      SM_HANDLE hDecoder);

SM_EXPORT uint32_t SMStubDecoderCopyUint8Array(
      SM_HANDLE hDecoder,
      uint32_t  nIndex,
      uint32_t  nMaxLength,
      uint8_t*  pArray);

SM_EXPORT uint32_t SMStubDecoderCopyUint16Array(
      SM_HANDLE hDecoder,
      uint32_t  nIndex,
      uint32_t  nMaxLength,
      uint16_t* pArray);

SM_EXPORT uint32_t SMStubDecoderCopyUint32Array(
      SM_HANDLE hDecoder,
      uint32_t  nIndex,
      uint32_t  nMaxLength,
      uint32_t* pArray);

SM_EXPORT uint32_t SMStubDecoderCopyHandleArray(
      SM_HANDLE  hDecoder,
      uint32_t   nIndex,
      uint32_t   nMaxLength,
      SM_HANDLE* pArray);

SM_EXPORT void SMStubDecoderReadSequence(
      SM_HANDLE  hDecoder,
      SM_HANDLE* phSequenceDecoder);

SM_EXPORT void SMStubDecoderSkip(
      SM_HANDLE hDecoder);

SM_EXPORT SM_ERROR SMManagerOpen(
      SM_HANDLE   hDevice,
      uint32_t    nLoginType,
      const void* pLoginInfo,
      uint32_t    nControlMode,
      SM_HANDLE*  phServiceManager);

SM_EXPORT SM_ERROR SMManagerClose(
      SM_HANDLE hServiceManager);

SM_EXPORT SM_ERROR SMManagerGetAllServices(
      SM_HANDLE hServiceManager,
      SM_UUID**  ppServiceIdentifierList,
      uint32_t* pnListLength);

SM_EXPORT SM_ERROR SMManagerGetServiceProperty(
      SM_HANDLE      hServiceManager,
      const SM_UUID* pidService,
      wchar_t*       pPropertyName,
      wchar_t**      ppPropertyValue);

SM_EXPORT SM_ERROR SMManagerGetAllServiceProperties(
      SM_HANDLE      hServiceManager,
      const SM_UUID* pidService,
      SM_PROPERTY**  ppProperties,
      uint32_t*      pnPropertiesLength);

SM_EXPORT SM_ERROR SMManagerDownloadService(
      SM_HANDLE      hServiceManager,
      const uint8_t* pServiceCode,
      uint32_t       nServiceCodeSize,
      SM_UUID*       pidService);

SM_EXPORT SM_ERROR SMManagerRemoveService(
      SM_HANDLE      hServiceManager,
      const SM_UUID* pidService);

SM_EXPORT SM_ERROR SMGetImplementationProperty(
      SM_HANDLE       hDevice,
      const wchar_t*  pPropertyName,
      wchar_t**       ppPropertyValue);

SM_EXPORT SM_ERROR SMGetAllImplementationProperties(
      SM_HANDLE     hDevice,
      SM_PROPERTY** ppProperties,
      uint32_t*     pnPropertiesLength);

#include "smapi_ex.h"

#ifdef __cplusplus
}
#endif

#endif /* __SMAPI_H__ */
