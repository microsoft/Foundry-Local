// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "telemetry/telemetry_event_properties_sanitizer.h"

#include "telemetry/telemetry_redaction.h"

#include <EventProperty.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace fl::TelemetryInternal {

namespace {

using ::Microsoft::Applications::Events::DataCategory;
using ::Microsoft::Applications::Events::DataCategory_PartB;
using ::Microsoft::Applications::Events::DataCategory_PartC;
using ::Microsoft::Applications::Events::EventProperties;
using ::Microsoft::Applications::Events::EventProperty;

void SanitizeProperties(EventProperties& event_properties, DataCategory category) {
  // 1DS exposes its category maps as const and its setter always targets PartC. The event is mutable here, so update
  // mapped values in place to preserve PartB/PartC placement and stable schema keys.
  auto& properties = const_cast<std::map<std::string, EventProperty>&>(event_properties.GetProperties(category));
  for (auto& [name, property] : properties) {
    const bool secret_property = telemetry_detail::IsSecretKey(name);
    if (property.type == EventProperty::TYPE_STRING) {
      const auto value = property.as_string == nullptr ? std::string_view{} : std::string_view(property.as_string);
      const auto sanitized_value = secret_property ? std::string{"[secret]"} : ScrubStringForTelemetry(value);
      property = EventProperty(sanitized_value, property.piiKind, property.dataCategory);
    } else if (property.type == EventProperty::TYPE_STRING_ARRAY) {
      std::vector<std::string> sanitized_values;
      if (property.as_stringArray != nullptr) {
        sanitized_values.reserve(property.as_stringArray->size());
        for (const auto& value : *property.as_stringArray) {
          sanitized_values.push_back(secret_property ? std::string{"[secret]"} : ScrubStringForTelemetry(value));
        }
      }

      property = EventProperty(sanitized_values, property.piiKind, property.dataCategory);
    }
  }
}

}  // namespace

void SanitizeEventProperties(EventProperties& event_properties) {
  SanitizeProperties(event_properties, DataCategory_PartC);
  SanitizeProperties(event_properties, DataCategory_PartB);
}

}  // namespace fl::TelemetryInternal
