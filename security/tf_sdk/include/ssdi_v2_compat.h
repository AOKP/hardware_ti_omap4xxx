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
* File            : ssdi_v2_compat.h
*
* Original-Author : Trusted Logic S.A.
*
* Created         : July 08, 2010
*/

/**
 * This header file contains the definitions for the legacy
 * SSDI-V2 types and functions
 */

#ifndef __SSDI_V2_COMPAT_H__
#define __SSDI_V2_COMPAT_H__

#include "s_type.h"
#include "s_error.h"
#include "ssdi.h"

/* SSPI entry points must not be exported.
   SRVX entry points defined in the ssdi_v2_compat library are exported. */
#define SSPI_EXPORT

/*------------------------------------------------------------------------------
         Constants
------------------------------------------------------------------------------*/
#define S_SHARED_MEMORY_ACCESS_READ                0x01
#define S_SHARED_MEMORY_ACCESS_WRITE               0x02

#define S_NULL_ELEMENT                             0xFFFFFFFF

#define S_SHARED_MEMORY_ACCESS_READ                0x01
#define S_SHARED_MEMORY_ACCESS_WRITE               0x02

/* SControl constants */
#define S_CONTROL_MODE_USER                        0x00000002
#define S_CONTROL_MODE_MANAGER                     0x00000008
#define S_CONTROL_MODE_EXCLUSIVE_MANAGER           0x00000010

/* Shared memory access */
#define S_MEMORY_ACCESS_CLIENT_WRITE_SERVICE_READ  0x00000001
#define S_MEMORY_ACCESS_CLIENT_READ_SERVICE_WRITE  0x00000002

/* Login types */
#define S_LOGIN_OS_IDENTIFICATION                  S_LOGIN_APPLICATION_USER

/*------------------------------------------------------------------------------
         Decoder Functions
------------------------------------------------------------------------------*/

S_RESULT SDecoderGetError(S_HANDLE hDecoder);

bool SDecoderHasData(S_HANDLE hDecoder);

uint8_t SDecoderReadUint8(S_HANDLE hDecoder);

uint16_t SDecoderReadUint16(S_HANDLE hDecoder);

uint32_t SDecoderReadUint32(S_HANDLE hDecoder);

char *SDecoderReadString(S_HANDLE hDecoder);

bool SDecoderReadBoolean(S_HANDLE hDecoder);

uint8_t *SDecoderReadUint8Array(
                               S_HANDLE hDecoder,
                               OUT uint32_t* pnArrayLength);

uint16_t *SDecoderReadUint16Array(
                                 S_HANDLE hDecoder,
                                 OUT uint32_t* pnArrayLength);

uint32_t *SDecoderReadUint32Array(
                                 S_HANDLE hDecoder,
                                 OUT uint32_t* pnArrayLength);

uint32_t SDecoderReadArrayLength(S_HANDLE hDecoder);

uint32_t SDecoderCopyUint8Array(
                                IN  S_HANDLE hDecoder,
                                IN  uint32_t nIndex,
                                IN  uint32_t nMaxLength,
                                OUT uint8_t* pArray);

uint32_t SDecoderCopyUint16Array(
                                 IN S_HANDLE hDecoder,
                                 IN uint32_t nIndex,
                                 IN uint32_t nMaxLength,
                                 OUT uint16_t* pArray);

uint32_t SDecoderCopyUint32Array(
                                 IN S_HANDLE hDecoder,
                                 IN uint32_t nIndex,
                                 IN uint32_t nMaxLength,
                                 OUT uint32_t* pArray);

void SDecoderOpenSequence(S_HANDLE hDecoder);

void SDecoderCloseSequence(S_HANDLE hDecoder);

void SDecoderSkip(S_HANDLE hDecoder);

uint8_t *SDecoderReadMemoryReference(
                                     S_HANDLE hDecoder,
                                     uint32_t nFlags,
                                     OUT uint32_t* pnSize);
void SDecoderReadUUID(
                        IN  S_HANDLE hDecoder,
                        OUT S_UUID*  pUUID);

/*------------------------------------------------------------------------------
         Encoder Functions
------------------------------------------------------------------------------*/

void SEncoderWriteUint8(
                        IN S_HANDLE hEncoder,
                        IN uint8_t value);

void SEncoderWriteUint16(
                         IN S_HANDLE hEncoder,
                         IN   uint16_t value);

void SEncoderWriteUint32(IN S_HANDLE hEncoder,
                                     IN uint32_t value);

void SEncoderWriteBoolean(IN S_HANDLE hEncoder,
                                      IN  bool value);

void SEncoderWriteString(IN S_HANDLE hEncoder,
                                     IN const char* value);

void SEncoderWriteUint8Array(IN S_HANDLE hEncoder,
                                         IN  uint32_t nArrayLength,
                                         IN const uint8_t* pnArray);

void SEncoderWriteUint16Array(IN S_HANDLE hEncoder,
                              IN uint32_t nArrayLength,
                              IN const uint16_t* pnArray);

void SEncoderWriteUint32Array(IN S_HANDLE hEncoder,
                              IN uint32_t nArrayLength,
                              IN const uint32_t* pnArray);

void SEncoderWriteBooleanArray(IN S_HANDLE hEncoder,
                               IN   uint32_t nArrayLength,
                               IN const bool*    pnArray);

void SEncoderWriteStringArray(IN S_HANDLE hEncoder,
                             IN uint32_t nArrayLength,
                             IN const char** pnArray);

void SEncoderWriteMemoryReference(
                              S_HANDLE hEncoder,
                              S_HANDLE hBlock,
                              uint32_t nOffset,
                              uint32_t nLength,
                              uint32_t nFlags);

void SEncoderOpenSequence( S_HANDLE hEncoder );

void SEncoderCloseSequence( S_HANDLE hEncoder );

S_RESULT SEncoderGetError(S_HANDLE hEncoder);

void SEncoderReset( S_HANDLE hEncoder );

void SEncoderWriteUUID(
                  IN S_HANDLE hEncoder,
                  IN const S_UUID*  pUUID);

/*------------------------------------------------------------------------------
         Service Control Functions
------------------------------------------------------------------------------*/

S_RESULT SControlPrepareOpenOperation(
        IN const S_UUID*       pIdService,
           uint32_t      nControlMode,
        IN const S_TIME_LIMIT* pTimeLimit,
           uint32_t      nEncoderBufferSize,
           uint32_t      nDecoderBufferSize,
       OUT S_HANDLE*     phClientSession,
       OUT S_HANDLE*     phParameterEncoder,
       OUT S_HANDLE*     phOperation );

S_RESULT SControlPrepareInvokeOperation(
           S_HANDLE      hClientSession,
           uint32_t      nCommandIdentifier,
        IN const S_TIME_LIMIT* pTimeLimit,
           uint32_t      nEncoderBufferSize,
           uint32_t      nDecoderBufferSize,
       OUT S_HANDLE*     phParameterEncoder,
       OUT S_HANDLE*     phOperation );

S_RESULT SControlPrepareCloseOperation(
           S_HANDLE  hClientSession,
           uint32_t  nEncoderBufferSize,
           uint32_t  nDecoderBufferSize,
       OUT S_HANDLE* phParameterEncoder,
       OUT S_HANDLE* phOperation );

S_RESULT SControlPerformOperation(
           S_HANDLE  hOperation,
           uint32_t  nReserved,
       OUT S_RESULT* pnServiceErrorCode,
       OUT S_HANDLE* phAnswerDecoder );

S_RESULT SControlCancelOperation( S_HANDLE hOperation );

S_RESULT SControlAllocateSharedMemory(
           S_HANDLE  hClientSession,
           uint32_t  nLength,
           uint32_t  nFlags,
           uint32_t  nReserved,
       OUT void**    ppBlock,
       OUT S_HANDLE* phBlockHandle);

S_RESULT SControlRegisterSharedMemory(
           S_HANDLE  hClientSession,
        IN const void*     pBuffer,
           uint32_t  nBufferLength,
           uint32_t  nFlags,
           uint32_t  nReserved,
       OUT S_HANDLE* phBlockHandle );

/*------------------------------------------------------------------------------
         Service Manager Functions
------------------------------------------------------------------------------*/

S_RESULT SManagerOpen(
                           uint32_t nControlMode,
                           S_HANDLE* phServiceManager);

S_RESULT SManagerGetAllServices(
                           S_HANDLE hServiceManager,
                           S_UUID** ppServiceIdentifierList,
                           uint32_t* pnListLength);

S_RESULT SManagerGetServiceProperty(
                           S_HANDLE hServiceManager,
                           const S_UUID* pidService,
                           const char* pPropertyName,
                           char** ppPropertyValue);

S_RESULT SManagerGetAllServiceProperties(
                           S_HANDLE hServiceManager,
                           const S_UUID* pidService,
                           S_PROPERTY** ppProperties,
                           uint32_t* pnPropertiesLength);

S_RESULT SManagerDownloadService(
                           S_HANDLE hServiceManager,
                           const uint8_t* pServiceCode,
                           uint32_t nServiceCodeSize,
                           S_UUID* pidService);

S_RESULT SManagerRemoveService(
                           S_HANDLE hServiceManager,
                           const S_UUID* pidService);

/*------------------------------------------------------------------------------
         SSPI Entry Points
------------------------------------------------------------------------------*/

S_RESULT SSPICreate(void);
void     SSPIDestroy(void);
S_RESULT SSPIOpenClientSession(S_HANDLE hDecoder,
                                           S_HANDLE hEncoder,
                                           OUT void** ppSessionContext);
S_RESULT SSPIInvokeCommand(IN OUT void* pSessionContext,
                                       uint32_t nCommandID,
                                       S_HANDLE hDecoder,
                                       S_HANDLE hEncoder);
S_RESULT SSPICloseClientSession(IN OUT void* pSessionContext,
                                            uint32_t nCause,
                                            S_HANDLE hDecoder,
                                            S_HANDLE hEncoder);
#endif /* __SSDI_V2_COMPAT_H__ */
