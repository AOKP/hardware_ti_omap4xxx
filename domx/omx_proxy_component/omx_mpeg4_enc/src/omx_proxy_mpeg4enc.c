/*
 * Copyright (c) 2010, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 *  @file  omx_proxy_videodecoder.c
 *         This file contains methods that provides the functionality for
 *         the OpenMAX1.1 DOMX Framework Tunnel Proxy component.
 *********************************************************************************************
 This is the proxy specific wrapper that passes the component name to the generic proxy init()
 The proxy wrapper also does some runtime/static time onfig on per proxy basis
 This is a thin wrapper that is called when componentiit() of the proxy is called
 static OMX_ERRORTYPE PROXY_Wrapper_init(OMX_HANDLETYPE hComponent, OMX_PTR pAppData);
 this layer gets called first whenever a proxy's get handle is called
 ************************************************************************************************
 *  @path WTSD_DucatiMMSW\omx\omx_il_1_x\omx_proxy_component\src
 *
 *  @rev 1.0
 */

/*==============================================================
 *! Revision History
 *! ============================
 *! 20-August-2010 Sarthak Aggarwal sarthak@ti.com: Initial Version
 *================================================================*/

/******************************************************************
 *   INCLUDE FILES
 ******************************************************************/
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "omx_proxy_common.h"
#include <timm_osal_interfaces.h>
#include "OMX_TI_IVCommon.h"
#include "OMX_TI_Video.h"
#include "OMX_TI_Index.h"

#include <MetadataBufferType.h>
#ifdef  ENABLE_GRALLOC_BUFFER
#include "native_handle.h"
#include <hal_public.h>
#include <VideoMetadata.h>
#endif

#define COMPONENT_NAME "OMX.TI.DUCATI1.VIDEO.MPEG4E"
/* needs to be specific for every configuration wrapper */

#define OMX_MPEG4E_INPUT_PORT 0
#define LINUX_PAGE_SIZE 4096

#ifdef ANDROID_QUIRK_CHANGE_PORT_VALUES

OMX_ERRORTYPE LOCAL_PROXY_MPEG4E_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct);

OMX_ERRORTYPE LOCAL_PROXY_MPEG4E_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct);

#endif

OMX_ERRORTYPE LOCAL_PROXY_MPEG4E_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE * pIndexType);

OMX_ERRORTYPE LOCAL_PROXY_MPEG4E_EmptyThisBuffer(OMX_HANDLETYPE hComponent,
    OMX_BUFFERHEADERTYPE * pBufferHdr);

extern OMX_ERRORTYPE RPC_UTIL_GetStride(OMX_COMPONENTTYPE * hRemoteComp,
    OMX_U32 nPortIndex, OMX_U32 * nStride);
extern OMX_ERRORTYPE RPC_UTIL_GetNumLines(OMX_COMPONENTTYPE * hComp,
    OMX_U32 nPortIndex, OMX_U32 * nNumOfLines);

OMX_ERRORTYPE OMX_ComponentInit(OMX_HANDLETYPE hComponent)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pHandle = NULL;
	PROXY_COMPONENT_PRIVATE *pComponentPrivate = NULL;
	pHandle = (OMX_COMPONENTTYPE *) hComponent;
        OMX_TI_PARAM_ENHANCEDPORTRECONFIG tParamStruct;
	DOMX_ENTER("");

	DOMX_DEBUG("Component name provided is %s", COMPONENT_NAME);

	pHandle->pComponentPrivate =
	    (PROXY_COMPONENT_PRIVATE *)
	    TIMM_OSAL_Malloc(sizeof(PROXY_COMPONENT_PRIVATE), TIMM_OSAL_TRUE,
	    0, TIMMOSAL_MEM_SEGMENT_INT);

	PROXY_assert(pHandle->pComponentPrivate != NULL,
	    OMX_ErrorInsufficientResources,
	    "ERROR IN ALLOCATING PROXY COMPONENT PRIVATE STRUCTURE");

	pComponentPrivate =
	    (PROXY_COMPONENT_PRIVATE *) pHandle->pComponentPrivate;

	TIMM_OSAL_Memset(pComponentPrivate, 0,
		sizeof(PROXY_COMPONENT_PRIVATE));

	pComponentPrivate->cCompName =
	    TIMM_OSAL_Malloc(MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8),
	    TIMM_OSAL_TRUE, 0, TIMMOSAL_MEM_SEGMENT_INT);

	PROXY_assert(pComponentPrivate->cCompName != NULL,
	    OMX_ErrorInsufficientResources,
	    " Error in Allocating space for proxy component table");

	// Copying component Name - this will be picked up in the proxy common
	PROXY_assert(strlen(COMPONENT_NAME) + 1 < MAX_COMPONENT_NAME_LENGTH,
	    OMX_ErrorInvalidComponentName,
	    "Length of component name is longer than the max allowed");
	TIMM_OSAL_Memcpy(pComponentPrivate->cCompName, COMPONENT_NAME,
	    strlen(COMPONENT_NAME) + 1);

	eError = OMX_ProxyCommonInit(hComponent);	// Calling Proxy Common Init()
#ifdef ANDROID_QUIRK_CHANGE_PORT_VALUES
	pHandle->SetParameter = LOCAL_PROXY_MPEG4E_SetParameter;
    pHandle->GetParameter = LOCAL_PROXY_MPEG4E_GetParameter;
#endif
	pComponentPrivate->IsLoadedState = OMX_TRUE;
	pHandle->EmptyThisBuffer = LOCAL_PROXY_MPEG4E_EmptyThisBuffer;
	pHandle->GetExtensionIndex = LOCAL_PROXY_MPEG4E_GetExtensionIndex;

      EXIT:
	if (eError != OMX_ErrorNone)
	{
		DOMX_DEBUG("Error in Initializing Proxy");
		if (pComponentPrivate->cCompName != NULL)
		{
			TIMM_OSAL_Free(pComponentPrivate->cCompName);
			pComponentPrivate->cCompName = NULL;
		}
		if (pComponentPrivate != NULL)
		{
			TIMM_OSAL_Free(pComponentPrivate);
			pComponentPrivate = NULL;
		}
	}
	return eError;
}

#ifdef  ANDROID_QUIRK_CHANGE_PORT_VALUES

/* ===========================================================================*/
/**
 * @name PROXY_MPEG4E_GetParameter()
 * @brief
 * @param void
 * @return OMX_ErrorNone = Successful
 * @sa TBD
 *
 */
/* ===========================================================================*/
OMX_ERRORTYPE LOCAL_PROXY_MPEG4E_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParamStruct)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	PROXY_COMPONENT_PRIVATE *pCompPrv = NULL;
	OMX_COMPONENTTYPE *hComp = (OMX_COMPONENTTYPE *) hComponent;
	OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pParamStruct;
	OMX_VIDEO_PARAM_PORTFORMATTYPE* pPortParam = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pParamStruct;

	PROXY_require((pParamStruct != NULL), OMX_ErrorBadParameter, NULL);
	PROXY_assert((hComp->pComponentPrivate != NULL),
	    OMX_ErrorBadParameter, NULL);

	pCompPrv = (PROXY_COMPONENT_PRIVATE *) hComp->pComponentPrivate;

	DOMX_ENTER
	    ("hComponent = %p, pCompPrv = %p, nParamIndex = %d, pParamStruct = %p",
	    hComponent, pCompPrv, nParamIndex, pParamStruct);

	eError = PROXY_GetParameter(hComponent,nParamIndex, pParamStruct);
	PROXY_assert(eError == OMX_ErrorNone,
		    eError," Error in Proxy GetParameter");

	if( nParamIndex == OMX_IndexParamPortDefinition)
	{
		if(pPortDef->format.video.eColorFormat == OMX_COLOR_FormatYUV420PackedSemiPlanar)
		{
			pPortDef->format.video.eColorFormat = OMX_TI_COLOR_FormatYUV420PackedSemiPlanar;
		}
	}
	else if ( nParamIndex == OMX_IndexParamVideoPortFormat)
	{
		if(pPortParam->eColorFormat == OMX_COLOR_FormatYUV420PackedSemiPlanar)
		{
			pPortParam->eColorFormat = OMX_TI_COLOR_FormatYUV420PackedSemiPlanar;
		}
        }

      EXIT:
	DOMX_EXIT("eError: %d", eError);
	return eError;
}

/* ===========================================================================*/
/**
 * @name PROXY_MPEG4E_SetParameter()
 * @brief
 * @param void
 * @return OMX_ErrorNone = Successful
 * @sa TBD
 *
 */
/* ===========================================================================*/
OMX_ERRORTYPE LOCAL_PROXY_MPEG4E_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex, OMX_IN OMX_PTR pParamStruct)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	PROXY_COMPONENT_PRIVATE *pCompPrv = NULL;
	OMX_COMPONENTTYPE *hComp = (OMX_COMPONENTTYPE *) hComponent;
	OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pParamStruct;
	OMX_VIDEO_PARAM_PORTFORMATTYPE* pPortParams = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pParamStruct;
	OMX_VIDEO_STOREMETADATAINBUFFERSPARAMS* pStoreMetaData = (OMX_VIDEO_STOREMETADATAINBUFFERSPARAMS *) pParamStruct;
	OMX_TI_PARAM_BUFFERPREANNOUNCE tParamSetNPA;
	OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

	PROXY_require((pParamStruct != NULL), OMX_ErrorBadParameter, NULL);
	PROXY_require((hComp->pComponentPrivate != NULL),
	    OMX_ErrorBadParameter, NULL);

	pCompPrv = (PROXY_COMPONENT_PRIVATE *) hComp->pComponentPrivate;
	DOMX_ENTER
	    ("hComponent = %p, pCompPrv = %p, nParamIndex = %d, pParamStruct = %p",
	    hComponent, pCompPrv, nParamIndex, pParamStruct);
	if(nParamIndex == OMX_IndexParamPortDefinition)
	{
		if(pPortDef->format.video.eColorFormat == OMX_TI_COLOR_FormatYUV420PackedSemiPlanar)
		{
			pPortDef->format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedSemiPlanar;
		}
	}
	else if(nParamIndex == OMX_IndexParamVideoPortFormat)
	{
		if(pPortParams->eColorFormat == OMX_TI_COLOR_FormatYUV420PackedSemiPlanar)
		{
			pPortParams->eColorFormat = OMX_COLOR_FormatYUV420PackedSemiPlanar;
		}
	}
	else if(nParamIndex == (OMX_INDEXTYPE) OMX_TI_IndexEncoderStoreMetadatInBuffers)
	{
		DOMX_DEBUG("Moving to Metadatamode");
	    if (pStoreMetaData->nPortIndex == OMX_MPEG4E_INPUT_PORT && pStoreMetaData->bStoreMetaData == OMX_TRUE)
	    {
		tParamSetNPA.nSize = sizeof(OMX_TI_PARAM_BUFFERPREANNOUNCE);
		tParamSetNPA.nVersion.s.nVersionMajor = OMX_VER_MAJOR;
		tParamSetNPA.nVersion.s.nVersionMinor = OMX_VER_MINOR;
		tParamSetNPA.nVersion.s.nRevision = 0x0;
		tParamSetNPA.nVersion.s.nStep = 0x0;
		tParamSetNPA.nPortIndex = OMX_MPEG4E_INPUT_PORT;
		tParamSetNPA.bEnabled = OMX_TRUE;
		//Call NPA on OMX encoder on ducati.
		PROXY_SetParameter(hComponent,OMX_TI_IndexParamBufferPreAnnouncement, &tParamSetNPA);
		pCompPrv->proxyPortBuffers[pStoreMetaData->nPortIndex].proxyBufferType = EncoderMetadataPointers;
		DOMX_DEBUG("Moving to Metadatamode done");

		/*Initializing Structure */
		sPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
		sPortDef.nVersion.s.nVersionMajor = OMX_VER_MAJOR;
		sPortDef.nVersion.s.nVersionMinor = OMX_VER_MINOR;
		sPortDef.nVersion.s.nRevision = 0x0;
		sPortDef.nVersion.s.nStep = 0x0;
		sPortDef.nPortIndex = OMX_MPEG4E_INPUT_PORT;

		eError = PROXY_GetParameter(hComponent,OMX_IndexParamPortDefinition, &sPortDef);
		PROXY_assert(eError == OMX_ErrorNone, eError," Error in Proxy GetParameter for Port Def");

		sPortDef.format.video.nStride = LINUX_PAGE_SIZE;

		eError = PROXY_SetParameter(hComponent,OMX_IndexParamPortDefinition, &sPortDef);

		PROXY_assert(eError == OMX_ErrorNone, eError," Error in Proxy SetParameter for Port Def");
	    }
	    goto EXIT;
	}

	eError = PROXY_SetParameter(hComponent, nParamIndex, pParamStruct);
	PROXY_assert(eError == OMX_ErrorNone,
		    eError," Error in Proxy SetParameter");

	EXIT:
	DOMX_EXIT("eError: %d", eError);
	return eError;
}

#endif


/* ===========================================================================*/
/**
 * @name PROXY_GetExtensionIndex()
 * @brief
 * @param void
 * @return OMX_ErrorNone = Successful
 * @sa TBD
 *
 */
/* ===========================================================================*/
OMX_ERRORTYPE LOCAL_PROXY_MPEG4E_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE * pIndexType)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	PROXY_COMPONENT_PRIVATE *pCompPrv = NULL;
	OMX_COMPONENTTYPE *hComp = hComponent;

	PROXY_require((hComp->pComponentPrivate != NULL),
	    OMX_ErrorBadParameter, NULL);
	PROXY_require(cParameterName != NULL, OMX_ErrorBadParameter, NULL);
	PROXY_require(pIndexType != NULL, OMX_ErrorBadParameter, NULL);

	pCompPrv = (PROXY_COMPONENT_PRIVATE *) hComp->pComponentPrivate;

	DOMX_ENTER("%s hComponent = %p, pCompPrv = %p, cParameterName = %s",
	    __FUNCTION__,hComponent, pCompPrv, cParameterName);

	// Check for NULL Parameters
	PROXY_require((cParameterName != NULL && pIndexType != NULL),
	    OMX_ErrorBadParameter, NULL);

	// Ensure that String length is not greater than Max allowed length
	PROXY_require(strlen(cParameterName) <= 127, OMX_ErrorBadParameter, NULL);

	if(strcmp(cParameterName, "OMX.google.android.index.storeMetaDataInBuffers") == 0)
	{
		// If Index type is 2D Buffer Allocated Dimension
		*pIndexType = (OMX_INDEXTYPE) OMX_TI_IndexEncoderStoreMetadatInBuffers;
		goto EXIT;
	}

        PROXY_GetExtensionIndex(hComponent, cParameterName, pIndexType);

      EXIT:
	DOMX_EXIT("%s eError: %d",__FUNCTION__, eError);
	return eError;
}

/* ===========================================================================*/
/**
 * @name PROXY_MPEG4E_EmptyThisBuffer()
 * @brief
 * @param void
 * @return OMX_ErrorNone = Successful
 * @sa TBD
 *
 */
/* ===========================================================================*/
OMX_ERRORTYPE LOCAL_PROXY_MPEG4E_EmptyThisBuffer(OMX_HANDLETYPE hComponent,
    OMX_BUFFERHEADERTYPE * pBufferHdr)
{

	OMX_ERRORTYPE eError = OMX_ErrorNone;
	PROXY_COMPONENT_PRIVATE *pCompPrv;
	OMX_COMPONENTTYPE *hComp = (OMX_COMPONENTTYPE *) hComponent;
	OMX_PTR pBufferOrig = NULL;
	OMX_U32 nStride = 0, nNumLines = 0;

	PROXY_require(pBufferHdr != NULL, OMX_ErrorBadParameter, NULL);
	PROXY_require(hComp->pComponentPrivate != NULL, OMX_ErrorBadParameter,
	    NULL);
	PROXY_CHK_VERSION(pBufferHdr, OMX_BUFFERHEADERTYPE);

	pCompPrv = (PROXY_COMPONENT_PRIVATE *) hComp->pComponentPrivate;

	DOMX_DEBUG
	    ("%s hComponent=%p, pCompPrv=%p, nFilledLen=%d, nOffset=%d, nFlags=%08x",
	    __FUNCTION__,hComponent, pCompPrv, pBufferHdr->nFilledLen,
	    pBufferHdr->nOffset, pBufferHdr->nFlags);

	if( pCompPrv->proxyPortBuffers[OMX_MPEG4E_INPUT_PORT].proxyBufferType == EncoderMetadataPointers)
	{
		OMX_U32 *pTempBuffer;
		OMX_U32 nMetadataBufferType;
		DOMX_DEBUG("Passing meta data to encoder");

		pBufferOrig = pBufferHdr->pBuffer;

		pTempBuffer = (OMX_U32 *) (pBufferHdr->pBuffer);
		nMetadataBufferType = *pTempBuffer;

		if(nMetadataBufferType == kMetadataBufferTypeCameraSource)
		{
#ifdef ENABLE_GRALLOC_BUFFER
			IMG_native_handle_t* pGrallocHandle;
			video_metadata_t* pVideoMetadataBuffer;
			DOMX_DEBUG("MetadataBufferType is kMetadataBufferTypeCameraSource");

			pVideoMetadataBuffer = (video_metadata_t*) ((OMX_U32 *)(pBufferHdr->pBuffer));
			pGrallocHandle = (IMG_native_handle_t*) (pVideoMetadataBuffer->handle);
			DOMX_DEBUG("Grallloc buffer recieved in metadata buffer 0x%x",pGrallocHandle );

			pBufferHdr->pBuffer = (OMX_U8 *)(pGrallocHandle->fd[0]);
			((OMX_TI_PLATFORMPRIVATE *) pBufferHdr->pPlatformPrivate)->
			    pAuxBuf1 = pGrallocHandle->fd[1];
			DOMX_DEBUG("%s Gralloc=0x%x, Y-fd=%d, UV-fd=%d", __FUNCTION__, pGrallocHandle,
			            pGrallocHandle->fd[0], pGrallocHandle->fd[1]);

			pBufferHdr->nOffset = pVideoMetadataBuffer->offset;
#endif
		}
		else if(nMetadataBufferType == kMetadataBufferTypeGrallocSource)
		{
#ifdef ENABLE_GRALLOC_BUFFER
			IMG_native_handle_t* pGrallocHandle;
			DOMX_DEBUG("MetadataBufferType is kMetadataBufferTypeGrallocSource");

			pTempBuffer++;
			pGrallocHandle = (IMG_native_handle_t*) pTempBuffer;
			DOMX_DEBUG("Grallloc buffer recieved in metadata buffer 0x%x",pGrallocHandle );

			pBufferHdr->pBuffer = (OMX_U8 *)(pGrallocHandle->fd[0]);
			((OMX_TI_PLATFORMPRIVATE *) pBufferHdr->pPlatformPrivate)->
			    pAuxBuf1 = pGrallocHandle->fd[1];
			DOMX_DEBUG("%s Gralloc=0x%x, Y-fd=%d, UV-fd=%d", __FUNCTION__, pGrallocHandle,
			            pGrallocHandle->fd[0], pGrallocHandle->fd[1]);
#endif
		}
		else
		{
			return OMX_ErrorBadParameter;
		}
	}

	PROXY_EmptyThisBuffer(hComponent, pBufferHdr);

	if( pCompPrv->proxyPortBuffers[pBufferHdr->nInputPortIndex].proxyBufferType == EncoderMetadataPointers)
		pBufferHdr->pBuffer = pBufferOrig;

	EXIT:
		return eError;
}
