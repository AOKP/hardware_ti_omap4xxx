/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
* @file OMX3A.cpp
*
* This file contains functionality for handling 3A configurations.
*
*/

#undef LOG_TAG

#define LOG_TAG "CameraHAL"

#include "CameraHal.h"
#include "OMXCameraAdapter.h"
#include "ErrorUtils.h"

namespace android {

status_t OMXCameraAdapter::setParameters3A(const CameraParameters &params,
                                           BaseCameraAdapter::AdapterState state)
{
    status_t ret = NO_ERROR;
    int mode = 0;
    const char *str = NULL;

    LOG_FUNCTION_NAME;

    str = params.get(TICameraParameters::KEY_EXPOSURE_MODE);
    mode = getLUTvalue_HALtoOMX( str, ExpLUT);
    if ( ( str != NULL ) && ( mParameters3A.Exposure != mode ) && !mFaceDetectionRunning)
        {
        mParameters3A.Exposure = mode;
        CAMHAL_LOGDB("Exposure mode %d", mode);
        if ( 0 <= mParameters3A.Exposure )
            {
            mPending3Asettings |= SetExpMode;
            }
        }

    str = params.get(CameraParameters::KEY_WHITE_BALANCE);
    mode = getLUTvalue_HALtoOMX( str, WBalLUT);
    if ((mFirstTimeInit || ((str != NULL) && (mode != mParameters3A.WhiteBallance))) &&
        !mFaceDetectionRunning)
        {
        mParameters3A.WhiteBallance = mode;
        CAMHAL_LOGDB("Whitebalance mode %d", mode);
        if ( 0 <= mParameters3A.WhiteBallance )
            {
            mPending3Asettings |= SetWhiteBallance;
            }
        }

    if ( 0 <= params.getInt(TICameraParameters::KEY_CONTRAST) )
        {
        if ( mFirstTimeInit ||
             ( (mParameters3A.Contrast  + CONTRAST_OFFSET) !=
                     params.getInt(TICameraParameters::KEY_CONTRAST)) )
            {
            mParameters3A.Contrast = params.getInt(TICameraParameters::KEY_CONTRAST) - CONTRAST_OFFSET;
            CAMHAL_LOGDB("Contrast %d", mParameters3A.Contrast);
            mPending3Asettings |= SetContrast;
            }
        }

    if ( 0 <= params.getInt(TICameraParameters::KEY_SHARPNESS) )
        {
        if ( mFirstTimeInit ||
             ((mParameters3A.Sharpness + SHARPNESS_OFFSET) !=
                     params.getInt(TICameraParameters::KEY_SHARPNESS)))
            {
            mParameters3A.Sharpness = params.getInt(TICameraParameters::KEY_SHARPNESS) - SHARPNESS_OFFSET;
            CAMHAL_LOGDB("Sharpness %d", mParameters3A.Sharpness);
            mPending3Asettings |= SetSharpness;
            }
        }

    if ( 0 <= params.getInt(TICameraParameters::KEY_SATURATION) )
        {
        if ( mFirstTimeInit ||
             ((mParameters3A.Saturation + SATURATION_OFFSET) !=
                     params.getInt(TICameraParameters::KEY_SATURATION)) )
            {
            mParameters3A.Saturation = params.getInt(TICameraParameters::KEY_SATURATION) - SATURATION_OFFSET;
            CAMHAL_LOGDB("Saturation %d", mParameters3A.Saturation);
            mPending3Asettings |= SetSaturation;
            }
        }

    if ( 0 <= params.getInt(TICameraParameters::KEY_BRIGHTNESS) )
        {
        if ( mFirstTimeInit ||
             (( mParameters3A.Brightness !=
                     ( unsigned int ) params.getInt(TICameraParameters::KEY_BRIGHTNESS))) )
            {
            mParameters3A.Brightness = (unsigned)params.getInt(TICameraParameters::KEY_BRIGHTNESS);
            CAMHAL_LOGDB("Brightness %d", mParameters3A.Brightness);
            mPending3Asettings |= SetBrightness;
            }
        }

    str = params.get(CameraParameters::KEY_ANTIBANDING);
    mode = getLUTvalue_HALtoOMX(str,FlickerLUT);
    if ( mFirstTimeInit || ( ( str != NULL ) && ( mParameters3A.Flicker != mode ) ))
        {
        mParameters3A.Flicker = mode;
        CAMHAL_LOGDB("Flicker %d", mParameters3A.Flicker);
        if ( 0 <= mParameters3A.Flicker )
            {
            mPending3Asettings |= SetFlicker;
            }
        }

    str = params.get(TICameraParameters::KEY_ISO);
    mode = getLUTvalue_HALtoOMX(str, IsoLUT);
    CAMHAL_LOGVB("ISO mode arrived in HAL : %s", str);
    if ( mFirstTimeInit || (  ( str != NULL ) && ( mParameters3A.ISO != mode )) )
        {
        mParameters3A.ISO = mode;
        CAMHAL_LOGDB("ISO %d", mParameters3A.ISO);
        if ( 0 <= mParameters3A.ISO )
            {
            mPending3Asettings |= SetISO;
            }
        }

    str = params.get(CameraParameters::KEY_FOCUS_MODE);
    mode = getLUTvalue_HALtoOMX(str, FocusLUT);
    if ( (mFirstTimeInit || ((str != NULL) && (mParameters3A.Focus != mode))) &&
         !mFaceDetectionRunning )
        {
        //Apply focus mode immediatly only if  CAF  or Inifinity are selected
        if ( ( mode == OMX_IMAGE_FocusControlAuto ) ||
             ( mode == OMX_IMAGE_FocusControlAutoInfinity ) )
            {
            mPending3Asettings |= SetFocus;
            mParameters3A.Focus = mode;
            }
        else if ( mParameters3A.Focus == OMX_IMAGE_FocusControlAuto )
            {
            //If we switch from CAF to something else, then disable CAF
            mPending3Asettings |= SetFocus;
            mParameters3A.Focus = OMX_IMAGE_FocusControlOff;
            }

        mParameters3A.Focus = mode;
        CAMHAL_LOGDB("Focus %x", mParameters3A.Focus);
        }

    str = params.get(CameraParameters::KEY_EXPOSURE_COMPENSATION);
    if ( mFirstTimeInit ||
          (( str != NULL ) &&
                  (mParameters3A.EVCompensation !=
                          params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION))))
        {
        CAMHAL_LOGDB("Setting EV Compensation to %d",
                     params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION));

        mParameters3A.EVCompensation = params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
        mPending3Asettings |= SetEVCompensation;
        }

    str = params.get(CameraParameters::KEY_SCENE_MODE);
    mode = getLUTvalue_HALtoOMX( str, SceneLUT);
    if (  mFirstTimeInit || (( str != NULL ) && ( mParameters3A.SceneMode != mode )) )
        {
        if ( 0 <= mode )
            {
            mParameters3A.SceneMode = mode;
            mPending3Asettings |= SetSceneMode;
            }
        else
            {
            mParameters3A.SceneMode = OMX_Manual;
            }

        CAMHAL_LOGDB("SceneMode %d", mParameters3A.SceneMode);
        }

    str = params.get(CameraParameters::KEY_FLASH_MODE);
    mode = getLUTvalue_HALtoOMX( str, FlashLUT);
    if (  mFirstTimeInit || (( str != NULL ) && ( mParameters3A.FlashMode != mode )) )
        {
        if ( 0 <= mode )
            {
            mParameters3A.FlashMode = mode;
            mPending3Asettings |= SetFlash;
            }
        else
            {
            mParameters3A.FlashMode = OMX_Manual;
            }
        }

    CAMHAL_LOGVB("Flash Setting %s", str);
    CAMHAL_LOGVB("FlashMode %d", mParameters3A.FlashMode);

    str = params.get(CameraParameters::KEY_EFFECT);
    mode = getLUTvalue_HALtoOMX( str, EffLUT);
    if (  mFirstTimeInit || (( str != NULL ) && ( mParameters3A.Effect != mode )) )
        {
        mParameters3A.Effect = mode;
        CAMHAL_LOGDB("Effect %d", mParameters3A.Effect);
        if ( 0 <= mParameters3A.Effect )
            {
            mPending3Asettings |= SetEffect;
            }
        }

    str = params.get(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED);
    if ( (str != NULL) && (!strcmp(str, "true")) )
      {
        OMX_BOOL lock = OMX_FALSE;
        str = params.get(CameraParameters::KEY_AUTO_EXPOSURE_LOCK);
        if ( (strcmp(str, "true")) == 0)
          {
            CAMHAL_LOGVA("Locking Exposure");
            lock = OMX_TRUE;
          }
        else
          {
            CAMHAL_LOGVA("UnLocking Exposure");
          }
        if (mParameters3A.ExposureLock != lock)
          {
            mParameters3A.ExposureLock = lock;
            CAMHAL_LOGDB("ExposureLock %d", lock);
            mPending3Asettings |= SetExpLock;
          }
      }

    str = params.get(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED);
    if ( (str != NULL) && (!strcmp(str, "true")) )
      {
        OMX_BOOL lock = OMX_FALSE;
        str = params.get(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK);
        if ( (strcmp(str, "true")) == 0)
          {
            CAMHAL_LOGVA("Locking WhiteBalance");
            lock = OMX_TRUE;
          }
        else
          {
            CAMHAL_LOGVA("UnLocking WhiteBalance");
          }
        if (mParameters3A.WhiteBalanceLock != lock)
          {
            mParameters3A.WhiteBalanceLock = lock;
            CAMHAL_LOGDB("WhiteBalanceLock %d", lock);
            mPending3Asettings |= SetWBLock;
          }
      }

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

int OMXCameraAdapter::getLUTvalue_HALtoOMX(const char * HalValue, LUTtype LUT)
{
    int LUTsize = LUT.size;
    if( HalValue )
        for(int i = 0; i < LUTsize; i++)
            if( 0 == strcmp(LUT.Table[i].userDefinition, HalValue) )
                return LUT.Table[i].omxDefinition;

    return -ENOENT;
}

const char* OMXCameraAdapter::getLUTvalue_OMXtoHAL(int OMXValue, LUTtype LUT)
{
    int LUTsize = LUT.size;
    for(int i = 0; i < LUTsize; i++)
        if( LUT.Table[i].omxDefinition == OMXValue )
            return LUT.Table[i].userDefinition;

    return NULL;
}

status_t OMXCameraAdapter::setExposureMode(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_EXPOSURECONTROLTYPE exp;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&exp, OMX_CONFIG_EXPOSURECONTROLTYPE);
    exp.nPortIndex = OMX_ALL;
    exp.eExposureControl = (OMX_EXPOSURECONTROLTYPE)Gen3A.Exposure;

    if ( EXPOSURE_FACE_PRIORITY == Gen3A.Exposure )
        {
        //Disable Region priority and enable Face priority
        setAlgoPriority(REGION_PRIORITY, EXPOSURE_ALGO, false);
        setAlgoPriority(FACE_PRIORITY, EXPOSURE_ALGO, true);

        //Then set the mode to auto
        exp.eExposureControl = OMX_ExposureControlAuto;
        }
    else
        {
        //Disable Face and Region priority
        setAlgoPriority(FACE_PRIORITY, EXPOSURE_ALGO, false);
        setAlgoPriority(REGION_PRIORITY, EXPOSURE_ALGO, false);
        }

    eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp,
                            OMX_IndexConfigCommonExposure,
                            &exp);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring exposure mode 0x%x", eError);
        }
    else
        {
        CAMHAL_LOGDA("Camera exposure mode configured successfully");
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setFlashMode(Gen3A_settings& Gen3A)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_PARAM_FLASHCONTROLTYPE flash;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&flash, OMX_IMAGE_PARAM_FLASHCONTROLTYPE);
    flash.nPortIndex = OMX_ALL;
    flash.eFlashControl = ( OMX_IMAGE_FLASHCONTROLTYPE ) Gen3A.FlashMode;

    CAMHAL_LOGDB("Configuring flash mode 0x%x", flash.eFlashControl);
    eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp,
                            (OMX_INDEXTYPE) OMX_IndexConfigFlashControl,
                            &flash);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring flash mode 0x%x", eError);
        }
    else
        {
        CAMHAL_LOGDA("Camera flash mode configured successfully");
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setFocusMode(Gen3A_settings& Gen3A)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE focus;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    //First Disable Face and Region priority
    ret |= setAlgoPriority(FACE_PRIORITY, FOCUS_ALGO, false);
    ret |= setAlgoPriority(REGION_PRIORITY, FOCUS_ALGO, false);

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&focus, OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE);
        focus.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

        focus.eFocusControl = (OMX_IMAGE_FOCUSCONTROLTYPE)Gen3A.Focus;

        CAMHAL_LOGDB("Configuring focus mode 0x%x", focus.eFocusControl);
        eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigFocusControl, &focus);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring focus mode 0x%x", eError);
            }
        else
            {
            CAMHAL_LOGDA("Camera focus mode configured successfully");
            }
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setScene(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_SCENEMODETYPE scene;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&scene, OMX_CONFIG_SCENEMODETYPE);
    scene.nPortIndex = OMX_ALL;
    scene.eSceneMode = ( OMX_SCENEMODETYPE ) Gen3A.SceneMode;

    CAMHAL_LOGDB("Configuring scene mode 0x%x", scene.eSceneMode);
    eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp,
                            ( OMX_INDEXTYPE ) OMX_TI_IndexConfigSceneMode,
                            &scene);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring scene mode 0x%x", eError);
        }
    else
        {
        CAMHAL_LOGDA("Camera scene configured successfully");
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setEVCompensation(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_EXPOSUREVALUETYPE expValues;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&expValues, OMX_CONFIG_EXPOSUREVALUETYPE);
    expValues.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,
                   OMX_IndexConfigCommonExposureValue,
                   &expValues);
    CAMHAL_LOGDB("old EV Compensation for OMX = 0x%x", (int)expValues.xEVCompensation);
    CAMHAL_LOGDB("EV Compensation for HAL = %d", Gen3A.EVCompensation);

    expValues.xEVCompensation = ( Gen3A.EVCompensation * ( 1 << Q16_OFFSET ) )  / 10;
    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                            OMX_IndexConfigCommonExposureValue,
                            &expValues);
    CAMHAL_LOGDB("new EV Compensation for OMX = 0x%x", (int)expValues.xEVCompensation);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring EV Compensation 0x%x error = 0x%x",
                     ( unsigned int ) expValues.xEVCompensation,
                     eError);
        }
    else
        {
        CAMHAL_LOGDB("EV Compensation 0x%x configured successfully",
                     ( unsigned int ) expValues.xEVCompensation);
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setWBMode(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_WHITEBALCONTROLTYPE wb;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&wb, OMX_CONFIG_WHITEBALCONTROLTYPE);
    wb.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
    wb.eWhiteBalControl = ( OMX_WHITEBALCONTROLTYPE ) Gen3A.WhiteBallance;

    if ( WB_FACE_PRIORITY == Gen3A.WhiteBallance )
        {
        //Disable Region priority and enable Face priority
        setAlgoPriority(REGION_PRIORITY, WHITE_BALANCE_ALGO, false);
        setAlgoPriority(FACE_PRIORITY, WHITE_BALANCE_ALGO, true);

        //Then set the mode to auto
        wb.eWhiteBalControl = OMX_WhiteBalControlAuto;
        }
    else
        {
        //Disable Face and Region priority
        setAlgoPriority(FACE_PRIORITY, WHITE_BALANCE_ALGO, false);
        setAlgoPriority(REGION_PRIORITY, WHITE_BALANCE_ALGO, false);
        }

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                         OMX_IndexConfigCommonWhiteBalance,
                         &wb);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring Whitebalance mode 0x%x error = 0x%x",
                     ( unsigned int ) wb.eWhiteBalControl,
                     eError);
        }
    else
        {
        CAMHAL_LOGDB("Whitebalance mode 0x%x configured successfully",
                     ( unsigned int ) wb.eWhiteBalControl);
        }

    LOG_FUNCTION_NAME_EXIT;

    return eError;
}

status_t OMXCameraAdapter::setFlicker(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_FLICKERCANCELTYPE flicker;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&flicker, OMX_CONFIG_FLICKERCANCELTYPE);
    flicker.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
    flicker.eFlickerCancel = (OMX_COMMONFLICKERCANCELTYPE)Gen3A.Flicker;

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                            (OMX_INDEXTYPE)OMX_IndexConfigFlickerCancel,
                            &flicker );
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring Flicker mode 0x%x error = 0x%x",
                     ( unsigned int ) flicker.eFlickerCancel,
                     eError);
        }
    else
        {
        CAMHAL_LOGDB("Flicker mode 0x%x configured successfully",
                     ( unsigned int ) flicker.eFlickerCancel);
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setBrightness(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_BRIGHTNESSTYPE brightness;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&brightness, OMX_CONFIG_BRIGHTNESSTYPE);
    brightness.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
    brightness.nBrightness = Gen3A.Brightness;

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                         OMX_IndexConfigCommonBrightness,
                         &brightness);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring Brightness 0x%x error = 0x%x",
                     ( unsigned int ) brightness.nBrightness,
                     eError);
        }
    else
        {
        CAMHAL_LOGDB("Brightness 0x%x configured successfully",
                     ( unsigned int ) brightness.nBrightness);
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setContrast(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_CONTRASTTYPE contrast;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&contrast, OMX_CONFIG_CONTRASTTYPE);
    contrast.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
    contrast.nContrast = Gen3A.Contrast;

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                         OMX_IndexConfigCommonContrast,
                         &contrast);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring Contrast 0x%x error = 0x%x",
                     ( unsigned int ) contrast.nContrast,
                     eError);
        }
    else
        {
        CAMHAL_LOGDB("Contrast 0x%x configured successfully",
                     ( unsigned int ) contrast.nContrast);
        }

    LOG_FUNCTION_NAME_EXIT;

    return eError;
}

status_t OMXCameraAdapter::setSharpness(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_CONFIG_PROCESSINGLEVELTYPE procSharpness;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&procSharpness, OMX_IMAGE_CONFIG_PROCESSINGLEVELTYPE);
    procSharpness.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
    procSharpness.nLevel = Gen3A.Sharpness;

    if( procSharpness.nLevel == 0 )
        {
        procSharpness.bAuto = OMX_TRUE;
        }
    else
        {
        procSharpness.bAuto = OMX_FALSE;
        }

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                         (OMX_INDEXTYPE)OMX_IndexConfigSharpeningLevel,
                         &procSharpness);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring Sharpness 0x%x error = 0x%x",
                     ( unsigned int ) procSharpness.nLevel,
                     eError);
        }
    else
        {
        CAMHAL_LOGDB("Sharpness 0x%x configured successfully",
                     ( unsigned int ) procSharpness.nLevel);
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setSaturation(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_SATURATIONTYPE saturation;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&saturation, OMX_CONFIG_SATURATIONTYPE);
    saturation.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
    saturation.nSaturation = Gen3A.Saturation;

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                         OMX_IndexConfigCommonSaturation,
                         &saturation);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring Saturation 0x%x error = 0x%x",
                     ( unsigned int ) saturation.nSaturation,
                     eError);
        }
    else
        {
        CAMHAL_LOGDB("Saturation 0x%x configured successfully",
                     ( unsigned int ) saturation.nSaturation);
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setISO(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_EXPOSUREVALUETYPE expValues;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&expValues, OMX_CONFIG_EXPOSUREVALUETYPE);
    expValues.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,
                   OMX_IndexConfigCommonExposureValue,
                   &expValues);

    if( 0 == Gen3A.ISO )
        {
        expValues.bAutoSensitivity = OMX_TRUE;
        }
    else
        {
        expValues.bAutoSensitivity = OMX_FALSE;
        expValues.nSensitivity = Gen3A.ISO;
        }

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                         OMX_IndexConfigCommonExposureValue,
                         &expValues);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring ISO 0x%x error = 0x%x",
                     ( unsigned int ) expValues.nSensitivity,
                     eError);
        }
    else
        {
        CAMHAL_LOGDB("ISO 0x%x configured successfully",
                     ( unsigned int ) expValues.nSensitivity);
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setEffect(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_IMAGEFILTERTYPE effect;

    LOG_FUNCTION_NAME;

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        return NO_INIT;
        }

    OMX_INIT_STRUCT_PTR (&effect, OMX_CONFIG_IMAGEFILTERTYPE);
    effect.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
    effect.eImageFilter = (OMX_IMAGEFILTERTYPE ) Gen3A.Effect;

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                         OMX_IndexConfigCommonImageFilter,
                         &effect);
    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while configuring Effect 0x%x error = 0x%x",
                     ( unsigned int )  effect.eImageFilter,
                     eError);
        }
    else
        {
        CAMHAL_LOGDB("Effect 0x%x configured successfully",
                     ( unsigned int )  effect.eImageFilter);
        }

    LOG_FUNCTION_NAME_EXIT;

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setWhiteBalanceLock(Gen3A_settings& Gen3A)
{
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_IMAGE_CONFIG_LOCKTYPE lock;

  LOG_FUNCTION_NAME

  if ( OMX_StateInvalid == mComponentState )
    {
      CAMHAL_LOGEA("OMX component is in invalid state");
      return NO_INIT;
    }

  OMX_INIT_STRUCT_PTR (&lock, OMX_IMAGE_CONFIG_LOCKTYPE);
  lock.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
  lock.bLock = Gen3A.WhiteBalanceLock;
  eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                          (OMX_INDEXTYPE)OMX_IndexConfigImageWhiteBalanceLock,
                          &lock);
  if ( OMX_ErrorNone != eError )
    {
      CAMHAL_LOGEB("Error while configuring WhiteBalance Lock error = 0x%x", eError);
    }
  else
    {
      CAMHAL_LOGDB("WhiteBalance Lock configured successfully %d ", lock.bLock);
    }
  LOG_FUNCTION_NAME_EXIT

  return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::setExposureLock(Gen3A_settings& Gen3A)
{
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_IMAGE_CONFIG_LOCKTYPE lock;

  LOG_FUNCTION_NAME

  if ( OMX_StateInvalid == mComponentState )
    {
      CAMHAL_LOGEA("OMX component is in invalid state");
      return NO_INIT;
    }

  OMX_INIT_STRUCT_PTR (&lock, OMX_IMAGE_CONFIG_LOCKTYPE);
  lock.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
  lock.bLock = Gen3A.ExposureLock;
  eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                          (OMX_INDEXTYPE)OMX_IndexConfigImageExposureLock,
                          &lock);
  if ( OMX_ErrorNone != eError )
    {
      CAMHAL_LOGEB("Error while configuring Exposure Lock error = 0x%x", eError);
    }
  else
    {
      CAMHAL_LOGDB("Exposure Lock configured successfully %d ", lock.bLock);
    }
  LOG_FUNCTION_NAME_EXIT

    return ErrorUtils::omxToAndroidError(eError);
}

status_t OMXCameraAdapter::set3ALock(OMX_BOOL toggle)
{
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_IMAGE_CONFIG_LOCKTYPE lock;
  const char *param[] = {"false", "true"};
  int index = -1;

  LOG_FUNCTION_NAME

  if ( OMX_StateInvalid == mComponentState )
    {
      CAMHAL_LOGEA("OMX component is in invalid state");
      return NO_INIT;
    }

  OMX_INIT_STRUCT_PTR (&lock, OMX_IMAGE_CONFIG_LOCKTYPE);
  lock.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

  mParameters3A.ExposureLock = toggle;
  mParameters3A.WhiteBalanceLock = toggle;

  eError = OMX_GetConfig( mCameraAdapterParameters.mHandleComp,
                          (OMX_INDEXTYPE)OMX_IndexConfigImageExposureLock,
                          &lock);
  if ( OMX_ErrorNone != eError )
    {
      CAMHAL_LOGEB("Error GetConfig Exposure Lock error = 0x%x", eError);
    }
  else
    {
      CAMHAL_LOGDA("Exposure Lock GetConfig successfull");
    }

  /* Apply locks only when not applied already */
  if ( lock.bLock  != toggle )
    {
      setExposureLock(mParameters3A);
    }

  eError = OMX_GetConfig( mCameraAdapterParameters.mHandleComp,
                          (OMX_INDEXTYPE)OMX_IndexConfigImageWhiteBalanceLock,
                          &lock);
  if ( OMX_ErrorNone != eError )
    {
      CAMHAL_LOGEB("Error GetConfig WhiteBalance Lock error = 0x%x", eError);
    }
  else
    {
      CAMHAL_LOGDA("WhiteBalance Lock GetConfig successfull");
    }

  /* Apply locks only when not applied already */
  if ( lock.bLock != toggle )
    {
      setWhiteBalanceLock(mParameters3A);
    }

  index = toggle ? 1 : 0;
  mParams.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK, param[index]);
  mParams.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, param[index]);

  return ErrorUtils::omxToAndroidError(eError);

}

status_t OMXCameraAdapter::apply3Asettings( Gen3A_settings& Gen3A )
{
    status_t ret = NO_ERROR;
    unsigned int currSett; // 32 bit
    int portIndex;

    /*
     * Scenes have a priority during the process
     * of applying 3A related parameters.
     * They can override pretty much all other 3A
     * settings and similarly get overridden when
     * for instance the focus mode gets switched.
     * There is only one exception to this rule,
     * the manual a.k.a. auto scene.
     */
    if ( ( SetSceneMode & mPending3Asettings ) )
        {
        mPending3Asettings &= ~SetSceneMode;
        return setScene(Gen3A);
        }
    else if ( OMX_Manual != Gen3A.SceneMode )
        {
        mPending3Asettings = 0;
        return NO_ERROR;
        }

    for( currSett = 1; currSett < E3aSettingMax; currSett <<= 1)
        {
        if( currSett & mPending3Asettings )
            {
            switch( currSett )
                {
                case SetEVCompensation:
                    {
                    ret |= setEVCompensation(Gen3A);
                    break;
                    }

                case SetWhiteBallance:
                    {
                    ret |= setWBMode(Gen3A);
                    break;
                    }

                case SetFlicker:
                    {
                    ret |= setFlicker(Gen3A);
                    break;
                    }

                case SetBrightness:
                    {
                    ret |= setBrightness(Gen3A);
                    break;
                    }

                case SetContrast:
                    {
                    ret |= setContrast(Gen3A);
                    break;
                    }

                case SetSharpness:
                    {
                    ret |= setSharpness(Gen3A);
                    break;
                    }

                case SetSaturation:
                    {
                    ret |= setSaturation(Gen3A);
                    break;
                    }

                case SetISO:
                    {
                    ret |= setISO(Gen3A);
                    break;
                    }

                case SetEffect:
                    {
                    ret |= setEffect(Gen3A);
                    break;
                    }

                case SetFocus:
                    {
                    ret |= setFocusMode(Gen3A);
                    break;
                    }

                case SetExpMode:
                    {
                    ret |= setExposureMode(Gen3A);
                    break;
                    }

                case SetFlash:
                    {
                    ret |= setFlashMode(Gen3A);
                    break;
                    }

                case SetExpLock:
                  {
                    ret |= setExposureLock(Gen3A);
                    break;
                  }

                case SetWBLock:
                  {
                    ret |= setWhiteBalanceLock(Gen3A);
                    break;
                  }

                default:
                    CAMHAL_LOGEB("this setting (0x%x) is still not supported in CameraAdapter ",
                                 currSett);
                    break;
                }
                mPending3Asettings &= ~currSett;
            }
        }
        return ret;
}

};
