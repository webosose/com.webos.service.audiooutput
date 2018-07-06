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


#ifndef VOLUME_SERVICE_H
#define VOLUME_SERVICE_H

#include <string>
#include <unordered_map>
#include <luna-service2/lunaservice.hpp>

#include "ivolumecontroller.h"
#include "amixercontroller.h"
#include "utils.h"

struct AudioOutput
{
    AudioOutput(const std::string& _name, IVolumeController* _volumeController)
            : name(_name)
            , userMute(true)
            , volumeController(_volumeController)
    {};

    std::string name;
    bool userMute;
    IVolumeController* volumeController;
};

class VolumeService final
{
public:
    VolumeService(LS::Handle &handle,umiClient* umiInstance);
    VolumeService(const VolumeService &) = delete;
    VolumeService &operator=(const VolumeService &) = delete;

    // Luna handlers
    bool set(LSMessage& message);
    bool up(LSMessage& message);
    bool down(LSMessage& message);
    bool muteSoundOut(LSMessage& message);
    bool getStatus(LSMessage& message);

    // Call after media streams are set up to unmute outputs.
    void unmuteOutputs();

    // Call after media streams are closed to mute outputs.
    void muteOutputs();

private:
    // Data members
    LS::Handle *mService;
    AmixerController mAmixer;

    std::unordered_map<std::string, AudioOutput> mOutputs;
    bool mOutputsMuted;

    pbnjson::JValue buildAudioStatus();
    pbnjson::JValue buildAudioStatus(AudioOutput& output);
    AudioOutput* findOutput(const std::string &soundOutputType);
};
#endif
