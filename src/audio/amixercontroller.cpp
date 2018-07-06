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
#include "amixercontroller.h"

AmixerController::AmixerController(umiClient* umiInstance)
                   :umi(umiInstance)
{}

AmixerController::~AmixerController() {}

bool AmixerController::onVolumeChanged()
{
    if ( (nullptr == umi) || umi->setOutputVolume(UMI_AUDIO_AMIXER, getVolume()) != UMI_ERROR_NONE)
    {
        LOG_ERROR(MSGID_CONFIG_VOLUME_ERROR, 0, "Failed set Amixer volume to %d", getVolume());
        return false;
    }

    LOG_DEBUG("Amixer volume changed to %d", getVolume());
    return true;
}

bool AmixerController::onMuteChanged()
{
    if( (nullptr == umi) || umi->setOutputMute(UMI_AUDIO_AMIXER, getMute()) != UMI_ERROR_NONE)
    {
        LOG_ERROR(MSGID_CONFIG_VOLUME_ERROR, 0, "Failed set Amixer mute to %d", getMute());
        return false;
    }
    LOG_DEBUG("Amixer mute changed to %d", getMute());
    return true;
}
