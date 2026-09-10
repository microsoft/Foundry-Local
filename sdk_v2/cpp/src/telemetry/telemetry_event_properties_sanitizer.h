// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <EventProperties.hpp>

namespace fl::TelemetryInternal {

// Enforces telemetry string privacy and size limits at the final EventProperties emission boundary.
void SanitizeEventProperties(::Microsoft::Applications::Events::EventProperties& event_properties);

}  // namespace fl::TelemetryInternal
