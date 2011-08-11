#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "omx_proxy_common.h"
#include <timm_osal_interfaces.h>

#define COMPONENT_NAME "OMX.TI.DUCATI1.VIDEO.DECODER.secure"

OMX_ERRORTYPE OMX_ComponentInit(OMX_HANDLETYPE hComponent)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pHandle = NULL;
	PROXY_COMPONENT_PRIVATE *pComponentPrivate = NULL;
	pHandle = (OMX_COMPONENTTYPE *) hComponent;

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

	eError = OMX_ProxyViddecInit(hComponent);

#ifdef USE_ION
	pComponentPrivate->bUseIon = OMX_TRUE;
	pComponentPrivate->bMapIonBuffers = OMX_FALSE;
#endif
	EXIT:
		return eError;
}
