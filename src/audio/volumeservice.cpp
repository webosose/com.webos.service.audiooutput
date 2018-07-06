// Copyright (c) 2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <map>
#include "volumeservice.h"
#include  <umiclient.h>
#include "logging.h"

VolumeService::VolumeService(LS::Handle &handle,umiClient* umiInstance)
        : mService(&handle)
         ,mAmixer(umiInstance)
{
    LS_CREATE_CATEGORY_BEGIN(VolumeService, volume)
    LS_CATEGORY_METHOD(up)
    LS_CATEGORY_METHOD(down)
    LS_CATEGORY_METHOD(set)
    LS_CATEGORY_METHOD(getStatus)
    LS_CATEGORY_METHOD(muteSoundOut)
    LS_CREATE_CATEGORY_END

    try
    {
        mService->registerCategory("/audio/volume",LS_CATEGORY_TABLE_NAME(volume),nullptr,nullptr);
        mService->setCategoryData("/audio/volume", this);
    }
    catch (LS::Error &lunaError)
    {
        LOG_ERROR(MSGID_LS2_SUBSCRIBE_FAILED, 0 , "%s - VolumeService API's registration Failed.",
                  lunaError.what());
    }

    //Initialize outputs list
    mOutputs.emplace(std::piecewise_construct,
                     std::forward_as_tuple("alsa"),
                     std::forward_as_tuple("alsa", &mAmixer));

    //Apply initial volumes, all outputs unmuted
    for (auto& volFuncIter : mOutputs)
    {
        // Will be overrided by audiod set volume call
        volFuncIter.second.volumeController->init(false, umiInstance->getDefaultVolume());
        volFuncIter.second.userMute = false; //Will be overrided by audiod settings
    }
}

AudioOutput* VolumeService::findOutput(const std::string &soundOutputType)
{
    auto iter = mOutputs.find(soundOutputType);
    if (iter == mOutputs.end())
    {
        return nullptr;
    }

    return &(*iter).second;
}

bool VolumeService::set(LSMessage& message)
{
    std::string soundOutputType;
    int8_t volLevel;
    int parseError = 0;
    LS::Message request(&message);
    pbnjson::JValue requestObj;

    const std::string schema = STRICT_SCHEMA(PROPS_2(PROP(soundOutput,string),PROP(volume,integer))
                                             REQUIRED_2(soundOutput, volume));

    if (!LSUtils::parsePayload(request.getPayload(), requestObj, schema, &parseError))
    {
        LSUtils::respondWithError(request, errorSchemavalidation, API_ERROR_SCHEMA_VALIDATION);
        return true;
    }

    soundOutputType = requestObj["soundOutput"].asString();
    volLevel = requestObj["volume"].asNumber<int>();
    pbnjson::JValue responseObj = pbnjson::Object();

    if (volLevel > MAX_VOLUME || volLevel < MIN_VOLUME)
    {
        LSUtils::respondWithError(request, errorVolumeLimit, API_ERROR_VOLUME_LIMIT);
        return true;
    }

    AudioOutput* speaker = findOutput(soundOutputType);

    if (!speaker)
    {
        LSUtils::respondWithError(request, errorInvalidVolumeControl,
                                  API_ERROR_INVALID_VOLUME_CONTROL);
        return true;
    }

    if(!speaker->volumeController->setVolume(volLevel))
    {
        responseObj.put("returnValue", false);
        responseObj.put("errorText", errorHALError);
        responseObj.put("errorCode", API_ERROR_HAL_ERROR);
    }
    else
    {
        responseObj.put("returnValue", true);
        responseObj.put("soundOutput", soundOutputType);
        responseObj.put("volume", volLevel);
    }

    LSUtils::postToClient(request, responseObj);

    return true;
}

bool VolumeService::up(LSMessage& message)
{
    std::string soundOutputType;
    int parseError = 0;
    LS::Message request(&message);
    pbnjson::JValue requestObj;

    const std::string schema = STRICT_SCHEMA(PROPS_1(PROP(soundOutput, string))
                                             REQUIRED_1(soundOutput));

    if (!LSUtils::parsePayload(request.getPayload(), requestObj, schema, &parseError)) {
        LSUtils::respondWithError(request, errorSchemavalidation, API_ERROR_SCHEMA_VALIDATION);
        return true;
    }

    soundOutputType = requestObj["soundOutput"].asString();
    pbnjson::JValue responseObj = pbnjson::Object();

    AudioOutput* speaker = findOutput(soundOutputType);

    if (!speaker)
    {
        LSUtils::respondWithError(request, errorInvalidVolumeControl,
                                  API_ERROR_INVALID_VOLUME_CONTROL);
        return true;
    }

    auto curVolume = speaker->volumeController->getVolume();

    if (curVolume == MAX_VOLUME)
    {
        LSUtils::respondWithError(request, errorVolumeMaxMin, API_ERROR_VOLUME_LIMIT);
        return true;
    }

    if(!speaker->volumeController->setVolume(curVolume + 1))
    {
        responseObj.put("returnValue", false);
        responseObj.put("errorText", errorHALError);
        responseObj.put("errorCode", API_ERROR_HAL_ERROR);
    }
    else
    {
        responseObj.put("returnValue", true);
        responseObj.put("soundOutput", soundOutputType);
        responseObj.put("volume", (curVolume + 1));
    }

    LSUtils::postToClient(request, responseObj);

    return true;
}

bool VolumeService::down(LSMessage& message)
{
    std::string soundOutputType;
    int parseError = 0;
    LS::Message request(&message);
    pbnjson::JValue requestObj;

    const std::string schema = STRICT_SCHEMA(PROPS_1(PROP(soundOutput, string))
                                             REQUIRED_1(soundOutput));

    if (!LSUtils::parsePayload(request.getPayload(), requestObj, schema, &parseError))
    {
        LSUtils::respondWithError(request, errorSchemavalidation, API_ERROR_SCHEMA_VALIDATION);
        return true;
    }

    soundOutputType = requestObj["soundOutput"].asString();
    pbnjson::JValue responseObj = pbnjson::Object();

    AudioOutput* speaker = findOutput(soundOutputType);

    if (!speaker)
    {
        LSUtils::respondWithError(request, errorInvalidVolumeControl,
                                  API_ERROR_INVALID_VOLUME_CONTROL);
        return true;
    }

    auto curVolume = speaker->volumeController->getVolume();

    if (curVolume == MIN_VOLUME)
    {
        LSUtils::respondWithError(request, errorVolumeMaxMin, API_ERROR_VOLUME_LIMIT);
        return true;
    }

    if(!speaker->volumeController->setVolume(curVolume - 1))
    {
        responseObj.put("returnValue", false);
        responseObj.put("errorText", errorHALError);
        responseObj.put("errorCode", API_ERROR_HAL_ERROR);
    }
    else
    {
        responseObj.put("returnValue", true);
        responseObj.put("soundOutput", soundOutputType);
        responseObj.put("volume", (curVolume - 1));
    }

    LSUtils::postToClient(request, responseObj);

    return true;
}

bool VolumeService::muteSoundOut(LSMessage& message)
{
    std::string soundOutputType;
    bool muteFlag = false;
    int parseError = 0;

    LS::Message request(&message);
    pbnjson::JValue requestObj;

    const std::string schema = STRICT_SCHEMA(PROPS_2(PROP(soundOutput, string),PROP(mute, boolean))
                                             REQUIRED_2(soundOutput,mute));

    if (!LSUtils::parsePayload(request.getPayload(), requestObj, schema, &parseError))
    {
        LSUtils::respondWithError(request, errorSchemavalidation, API_ERROR_SCHEMA_VALIDATION);
        return true;
    }

    soundOutputType = requestObj["soundOutput"].asString();
    muteFlag = requestObj["mute"].asBool();
    pbnjson::JValue responseObj = pbnjson::Object();

    AudioOutput* speaker = findOutput(soundOutputType);

    if (!speaker)
    {
        LSUtils::respondWithError(request, errorInvalidVolumeControl,
                                  API_ERROR_INVALID_VOLUME_CONTROL);
        return true;
    }

    if(speaker->userMute != muteFlag && (!speaker->volumeController->setMute(muteFlag)))
    {
        responseObj.put("returnValue", false);
        responseObj.put("errorText", errorHALError);
        responseObj.put("errorCode", API_ERROR_HAL_ERROR);
    }
    else
    {
        speaker->userMute = muteFlag;
        responseObj.put("returnValue", true);
        responseObj.put("soundOutput", soundOutputType);
        responseObj.put("mute", muteFlag);
    }

    LSUtils::postToClient(request, responseObj);

    return true;
}

bool VolumeService::getStatus(LSMessage& message)
{
    LS::Message request(&message);
    pbnjson::JValue requestObj;
    int parseError = 0;

    const std::string schema = STRICT_SCHEMA(PROPS_0());

    if (!LSUtils::parsePayload(request.getPayload(), requestObj, schema, &parseError))
    {
        LSUtils::respondWithError(request, errorSchemavalidation, API_ERROR_SCHEMA_VALIDATION);
        return true;
    }

    pbnjson::JValue response = this->buildAudioStatus();

    LSUtils::postToClient(request, response);

    return true;
}

pbnjson::JValue VolumeService::buildAudioStatus()
{
    pbnjson::JArray status;
    pbnjson::JValue responseObj = pbnjson::Object();

    for (auto& volFuncIter : mOutputs)
    {
        AudioOutput& output = volFuncIter.second;
        status.append(buildAudioStatus( output));
    }
    responseObj.put("returnValue", true);
    responseObj.put("volumeStatus", status);

    return responseObj;
}

pbnjson::JValue VolumeService::buildAudioStatus(AudioOutput& output)
{
    pbnjson::JValue responseObj = pbnjson::Object();

    responseObj.put("soundOutput", output.name);
    responseObj.put("volume", output.volumeController->getVolume());
    responseObj.put("muted", output.volumeController->getMute());

    return responseObj;
}
