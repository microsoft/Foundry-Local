// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "items/item.h"

#include <string>

namespace fl {

/// Plain text item.
struct TextItem : Item {
  std::string text;
  flTextItemType text_type = FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT;

  explicit TextItem(std::string text = {},
                    flTextItemType text_type = FOUNDRY_LOCAL_TEXT_ITEM_TYPE_DEFAULT)
      : Item(FOUNDRY_LOCAL_ITEM_TEXT), text(std::move(text)), text_type(text_type) {}
};

}  // namespace fl
