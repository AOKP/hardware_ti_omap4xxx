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

#ifndef __SDDI_H__
#define __SDDI_H__

#include "ssdi.h"

#ifndef SDDI_EXPORT
#if defined(WIN32) || defined(__ARMCC_VERSION)
#ifdef SMODULE_IMPLEMENTATION
#define SDDI_EXPORT __declspec(dllexport)
#else
#define SDDI_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define SDDI_EXPORT __attribute__ ((visibility ("default")))
#else
#define SDDI_EXPORT
#endif
#endif   /* !SDDI_EXPORT */

#ifndef SDRV_EXPORT
#if defined(WIN32) || defined(_WIN32_WCE) || defined(__ARMCC_VERSION)
#define SDRV_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define SDRV_EXPORT __attribute__ ((visibility ("default")))
#else
#define SDRV_EXPORT
#endif
#endif   /* !SDRV_EXPORT */

#define S_CACHE_OPERATION_CLEAN                0x00000001
#define S_CACHE_OPERATION_INVALIDATE           0x00000002
#define S_CACHE_OPERATION_CLEAN_AND_INVALIDATE 0x00000003

void SDDI_EXPORT *SMemGetVirtual(uint32_t nSegmentID);

S_RESULT SDDI_EXPORT SMemGetPhysical(void* pVirtual, uint32_t* pnPhysical);

S_RESULT SDDI_EXPORT SMemFlush(
                   uint32_t  nSegmentID,
                   uint32_t  nOperation);

S_RESULT SDDI_EXPORT SMemFlushByAddress(
                   void* pStartAddress,
                   uint32_t nLength,
                   uint32_t  nOperation);

/*------------------------------------------------------------------------------
         Driver Common Entry Points
------------------------------------------------------------------------------*/

S_RESULT SDRV_EXPORT SDrvCreate(uint32_t nParam0, uint32_t nParam1);

void SDRV_EXPORT SDrvDestroy(void);

#endif /* #ifndef __SDDI_H__ */
