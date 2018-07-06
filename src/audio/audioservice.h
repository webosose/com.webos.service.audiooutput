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

#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include <string>
#include <unordered_map>
#include <luna-service2/lunaservice.hpp>
#include "ivolumecontroller.h"
#include "volumeservice.h"
#include <umiclient.h>
#include "utils.h"

using namespace pbnjson;

class AudioConnection
{
public:
    AudioConnection(){};

    std::string source;
    std::string sink;
    std::string outputMode;
    bool muted = false;

    UMI_AUDIO_RESOURCE_T audioResourceId = UMI_AUDIO_RESOURCE_NO_CONNECTION;
};

class AudioService
{

public:
    AudioService(LS::Handle &handle, VolumeService& volumeService,
                 umiClient* umiInstance);
    ~AudioService();

    AudioService(const AudioService &) = delete;
    AudioService &operator=(const AudioService &) = delete;

    // Audio methods
    bool connect(LSMessage& message);
    bool disconnect(LSMessage& message);
    bool mute(LSMessage& message);
    bool getStatus(LSMessage& message);
    bool setSoundOut(LSMessage& message);

private:
    VolumeService& mVolumeService;

    std::vector<AudioConnection> mConnections;
    LS::Handle *mService;

    umiClient* umi = nullptr;

    JValue buildStatus();
    JValue buildAudioStatus(const AudioConnection& connection);

    bool doMuteAudio(AudioConnection& connection, bool muted);
    bool isValidSource(std::string& source);
    bool isValidSink(std::string& sink);

    void removeAudioConnection(const std::string& source, const std::string& sink);

    AudioConnection* findAudioConnection(const std::string& source, const std::string& sink);

    UMI_ERROR doDisconnectAudio(AudioConnection& connection);

    UMI_AUDIO_RESOURCE_T getResourceId(std::string& source, std::string& sink);
    UMI_AUDIO_SNDOUT_T getSoundOutResourceId(std::string& soundOut);

};
#endif
