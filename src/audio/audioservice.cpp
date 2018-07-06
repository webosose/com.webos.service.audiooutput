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

#include "logging.h"
#include "audioservice.h"

AudioService::AudioService(LS::Handle &handle,VolumeService& volumeService,
                           umiClient* umiInstance)
        : mVolumeService(volumeService)
        , mService(&handle)
        , umi(umiInstance)
{
    LS_CREATE_CATEGORY_BEGIN(AudioService, audio)
    LS_CATEGORY_METHOD(connect)
    LS_CATEGORY_METHOD(disconnect)
    LS_CATEGORY_METHOD(getStatus)
    LS_CATEGORY_METHOD(mute)
    LS_CATEGORY_METHOD(setSoundOut)
    LS_CREATE_CATEGORY_END

    try
    {
        mService->registerCategory("/audio", LS_CATEGORY_TABLE_NAME(audio), nullptr, nullptr);
        mService->setCategoryData("/audio", this);
    }
    catch (LS::Error &lunaError)
    {
        LOG_ERROR(MSGID_LS2_SUBSCRIBE_FAILED, 0 , "%s - AudioService API's registration Failed.",
                  lunaError.what());
    }
}

AudioService::~AudioService()
{
    for (auto& connection: mConnections)
    {
        doDisconnectAudio(connection);
    }
    mConnections.clear();
}

bool AudioService::isValidSource(std::string& source)
{
    if ( "AMIXER" == source)
        return true;
    else
        return false;
}

bool AudioService::isValidSink(std::string& sink)
{
    if ( "ALSA" == sink )
        return true;
    else
        return false;
}

UMI_AUDIO_RESOURCE_T AudioService::getResourceId(std::string& source, std::string& sink)
{
    if ("AMIXER" == source && "ALSA" == sink)
        return UMI_AUDIO_RESOURCE_MIXER0; // TODO - Mixer Inputs to be enhanced
    else
        return UMI_AUDIO_RESOURCE_NO_CONNECTION;
}

UMI_AUDIO_SNDOUT_T AudioService::getSoundOutResourceId(std::string& soundOut)
{
    //TODO - Soundoutputs to be enhanced

    if  ("alsa" == soundOut)
        return UMI_AUDIO_AMIXER;
    else
        return UMI_AUDIO_NO_OUTPUT;
}

bool AudioService::connect(LSMessage& message)
{
    LS::Message request(&message);
    pbnjson::JValue requestObj;
    int parseError = 0;

    std::string sinkName;
    std::string sourceName;

    UMI_AUDIO_RESOURCE_T audioResourceId;

    const std::string schema = STRICT_SCHEMA(PROPS_2(PROP(source, string),PROP(sink, string))
                                             REQUIRED_2(source, sink));

    if (!LSUtils::parsePayload(request.getPayload(), requestObj, schema, &parseError))
    {
        LSUtils::respondWithError(request, errorSchemavalidation, API_ERROR_SCHEMA_VALIDATION);
        return true;
    }

    sinkName = requestObj["sink"].asString();
    sourceName = requestObj["source"].asString();

    LOG_DEBUG("Audio connect request for source %s, sink %s",
               sourceName.c_str(), sinkName.c_str());

    if (!isValidSource(sourceName) || !isValidSink(sinkName))
    {
        LSUtils::respondWithError(request, errorInvalidParameters, API_ERROR_INVALID_PARAMETERS);
        return true;
    }

    audioResourceId = getResourceId(sourceName, sinkName);

    if (UMI_AUDIO_RESOURCE_NO_CONNECTION == audioResourceId)
    {
        LSUtils::respondWithError(request, errorConnectionNotPossible,
                                  API_ERROR_CONNECTION_NOT_POSSIBLE);
        return true;
    }

    AudioConnection* connection = findAudioConnection(sourceName, sinkName);
    if (!connection)
    {
        connection = &(*mConnections.emplace(mConnections.end(), AudioConnection()));
        connection->sink = sinkName;
        connection->source = sourceName;
        connection->audioResourceId = audioResourceId;
    }

    UMI_ERROR success = (nullptr != umi) ? umi->connectInput(connection->audioResourceId):
                                           UMI_ERROR_FAIL;

    pbnjson::JValue responseObj = pbnjson::Object();

    if (success == UMI_ERROR_NONE)
    {
        LOG_DEBUG("Audio connect success");
        responseObj.put("returnValue", true);
        responseObj.put("source", sourceName);
        responseObj.put("sink", sinkName);
    }
    else
    {
        removeAudioConnection(sourceName, sinkName);
        responseObj.put("returnValue", false);
        responseObj.put("errorText", errorHALError);
        responseObj.put("errorCode", API_ERROR_HAL_ERROR);
    }
    LSUtils::postToClient(request, responseObj);

    return true;
}

bool AudioService::disconnect(LSMessage& message)
{
    LS::Message request(&message);
    pbnjson::JValue requestObj;
    int parseError = 0;

    std::string sinkName;
    std::string sourceName;

    const std::string schema = STRICT_SCHEMA(PROPS_2(PROP(sink, string), PROP(source, string))
                                             REQUIRED_2(source, sink));

    if (!LSUtils::parsePayload(request.getPayload(), requestObj, schema, &parseError))
    {
        LSUtils::respondWithError(request, errorSchemavalidation, API_ERROR_SCHEMA_VALIDATION);
        return true;
    }

    sinkName = requestObj["sink"].asString();
    sourceName = requestObj["source"].asString();

    LOG_DEBUG("Audio disconnect request for source %s, sink %s",
               sourceName.c_str(), sinkName.c_str());

    AudioConnection* connection = findAudioConnection(sourceName, sinkName);

    if (!connection)
    {
        LSUtils::respondWithError(request, errorAudioNotConnected, API_ERROR_AUDIO_NOT_CONNECTED);
        return true;
    }

    UMI_ERROR success = doDisconnectAudio(*connection);

    removeAudioConnection(sourceName, sinkName);

    pbnjson::JValue responseObj = pbnjson::Object();

    if (success == UMI_ERROR_NONE)
    {
        LOG_DEBUG("Audio disconnect with source %s and sink %s", sourceName.c_str(),
                  sinkName.c_str());
        responseObj.put("returnValue", true);
        responseObj.put("source", sourceName);
        responseObj.put("sink", sinkName);
    }
    else
    {
        responseObj.put("returnValue", false);
        responseObj.put("errorText", errorHALError);
        responseObj.put("errorCode", API_ERROR_HAL_ERROR);
    }
    LSUtils::postToClient(request, responseObj);
    return true;
}

bool AudioService::setSoundOut(LSMessage& message)
{
    LS::Message request(&message);
    pbnjson::JValue requestObj;
    int parseError = 0;

    std::string soundOut;

    const std::string schema = STRICT_SCHEMA(PROPS_1(PROP(soundOut, string)) REQUIRED_1(soundOut));

    if (!LSUtils::parsePayload(request.getPayload(), requestObj, schema, &parseError))
    {
        LSUtils::respondWithError(request, errorSchemavalidation, API_ERROR_SCHEMA_VALIDATION);
        return true;
    }

    soundOut = requestObj["soundOut"].asString();

    LOG_DEBUG("Audio setSoundOut request for soundOut %s",soundOut.c_str());

    UMI_AUDIO_SNDOUT_T soundOutResourceId = getSoundOutResourceId(soundOut);

    if (UMI_AUDIO_NO_OUTPUT == soundOutResourceId)
    {
        LSUtils::respondWithError(request, errorNotImplemented, API_ERROR_NOT_IMPLEMENTED);
        return true;
    }

    pbnjson::JValue responseObj = pbnjson::Object();

    if ( (nullptr != umi) && umi->setSoundOutput(soundOutResourceId) == UMI_ERROR_NONE)
    {
        LOG_DEBUG("Audio routing to soundOut %s  is success", soundOut.c_str());

        for (AudioConnection& connection: mConnections)
            connection.outputMode = soundOut;

        responseObj.put("returnValue", true);
        responseObj.put("soundOut", soundOut);
    }
    else
    {
        responseObj.put("returnValue", false);
        responseObj.put("errorText", errorHALError);
        responseObj.put("errorCode", API_ERROR_HAL_ERROR);
    }

    LSUtils::postToClient(request, responseObj);

    return true;
}

bool AudioService::mute(LSMessage& message)
{
    LS::Message request(&message);
    pbnjson::JValue requestObj;
    int parseError = 0;

    std::string sinkName;
    std::string sourceName;
    bool muted = false;

    const std::string schema = STRICT_SCHEMA(PROPS_3(PROP(source, string),PROP(sink, string),
                                             PROP(mute, boolean)) REQUIRED_3(source, sink, mute));

    if (!LSUtils::parsePayload(request.getPayload(), requestObj, schema, &parseError))
    {
        LSUtils::respondWithError(request, errorSchemavalidation, API_ERROR_SCHEMA_VALIDATION);
        return true;
    }

    sinkName = requestObj["sink"].asString();
    sourceName = requestObj["source"].asString();
    muted = requestObj["mute"].asBool();

    LOG_DEBUG("Audio mute called for source %s, sink %s, mute %d",
               sourceName.c_str(), sinkName.c_str(), muted);

    AudioConnection* connection = findAudioConnection(sourceName, sinkName);

    if (!connection)
    {
        LSUtils::respondWithError(request, errorAudioNotConnected, API_ERROR_AUDIO_NOT_CONNECTED);
        return true;
    }

    pbnjson::JValue responseObj = pbnjson::Object();

    if (!doMuteAudio(*connection, muted))
    {
        responseObj.put("returnValue", false);
        responseObj.put("errorText", errorHALError);
        responseObj.put("errorCode", API_ERROR_HAL_ERROR);
    }
    else
    {
        responseObj.put("returnValue", true);
        responseObj.put("sink", sinkName);
        responseObj.put("source", sourceName);
        responseObj.put("mute", muted);
    }

    LSUtils::postToClient(request, responseObj);

    return true;
}

bool AudioService::getStatus(LSMessage& message)
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

    pbnjson::JValue response = this->buildStatus();

    LSUtils::postToClient(request, response);

    return true;
}

pbnjson::JValue AudioService::buildStatus()
{
    pbnjson::JArray array;
    pbnjson::JValue responseObj = pbnjson::Object();
    for (AudioConnection& connection: mConnections)
    {
        array.append(buildAudioStatus(connection));
    }
    responseObj.put("returnValue", true);
    responseObj.put("audio", array);
    return responseObj;
}

pbnjson::JValue AudioService::buildAudioStatus(const AudioConnection& c)
{
    pbnjson::JValue responseObj = pbnjson::Object();

    responseObj.put("sink", c.sink);
    responseObj.put("source", c.source);
    if (c.outputMode.empty())
      responseObj.put("outputMode", "null");
    else
      responseObj.put("outputMode", c.outputMode);
    responseObj.put("muted", c.muted);

    return responseObj;
}

AudioConnection* AudioService::findAudioConnection(const std::string& source,
                                                       const std::string& sink)
{
    for (auto iter = mConnections.begin(); iter != mConnections.end(); iter ++)
    {
        AudioConnection& connection = *iter;
        if (connection.source == source && connection.sink == sink)
        {
            return &connection;
        }
    }
    return nullptr;
}

void AudioService::removeAudioConnection(const std::string& source,
                                             const std::string& sink)
{
    for (auto iter = mConnections.begin(); iter != mConnections.end(); iter ++)
    {
        AudioConnection& connection = *iter;
        if (connection.source == source && connection.sink == sink)
        {
            mConnections.erase(iter);
            break;
        }
    }
}

UMI_ERROR AudioService::doDisconnectAudio(AudioConnection& connection)
{
    return ( (nullptr != umi) ?umi->disconnectInput(connection.audioResourceId):UMI_ERROR_FAIL);
}

bool AudioService::doMuteAudio(AudioConnection& connection, bool muted)
{
    if (connection.muted == muted)
    {
        return true;
    }

    if ( (nullptr == umi) || umi->setMute(connection.audioResourceId, muted)!=UMI_ERROR_NONE)
    {
        return false;
    }

    connection.muted = muted;
    return true;
}
