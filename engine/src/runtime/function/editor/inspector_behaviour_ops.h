#pragma once

#include <cstdlib>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/function/script/behaviour_type_catalog.h"

namespace Blunder {

struct InspectorBehaviourPropRowData final {
  eastl::string key;
  eastl::string kind;  // "bool" | "number" | "string" | "clip_name"
  bool bool_value{false};
  float number_value{0.0f};
  eastl::string string_value;
  bool missing_type{false};
  /// True when kind is clip_name, value non-empty, and name ∉ player map.
  bool clip_name_invalid{false};
};

struct InspectorBehaviourRowData final {
  BehaviourId behaviour_id{k_invalid_behaviour_id};
  eastl::string type_name;
  bool missing{false};
  eastl::vector<InspectorBehaviourPropRowData> props;
};

inline const char* behaviourCatalogKindName(BehaviourCatalogMember::Kind kind) {
  switch (kind) {
    case BehaviourCatalogMember::Kind::Bool:
      return "bool";
    case BehaviourCatalogMember::Kind::Number:
      return "number";
    case BehaviourCatalogMember::Kind::String:
      return "string";
    case BehaviourCatalogMember::Kind::ClipName:
      return "clip_name";
  }
  return "string";
}

inline const BehaviourCatalogType* findBehaviourCatalogType(
    const eastl::vector<BehaviourCatalogType>& catalog,
    const eastl::string& clr_name) {
  for (const BehaviourCatalogType& type : catalog) {
    if (type.clr_name == clr_name) {
      return &type;
    }
  }
  return nullptr;
}

inline const Variant* findBehaviourBagValue(
    const eastl::vector<SceneBehaviourProperty>* bag, const eastl::string& key) {
  if (bag == nullptr) {
    return nullptr;
  }
  for (const SceneBehaviourProperty& prop : *bag) {
    if (prop.key == key) {
      return &prop.value;
    }
  }
  return nullptr;
}

inline Variant variantFromInspectorCommit(const eastl::string& kind,
                                          const eastl::string& text_value,
                                          float number_value, bool bool_value) {
  if (kind == "bool") {
    return Variant(bool_value);
  }
  if (kind == "number") {
    if (!text_value.empty()) {
      char* end = nullptr;
      const float parsed = std::strtof(text_value.c_str(), &end);
      if (end != text_value.c_str()) {
        return Variant(parsed);
      }
    }
    return Variant(number_value);
  }
  // "string" and "clip_name" both persist as string bag values.
  if (!text_value.empty()) {
    return Variant(text_value);
  }
  return Variant(eastl::string{});
}

/// Logical names from co-located AnimationPlayer (empty if none).
inline void collectBehaviourClipNameChoices(
    const Object* object, eastl::vector<eastl::string>& out_names) {
  out_names.clear();
  if (object == nullptr || !object->hasAnimationPlayer()) {
    return;
  }
  const AnimationPlayer* player = object->getAnimationPlayer();
  if (player == nullptr) {
    return;
  }
  const eastl::vector<AnimationPlayer::ClipBinding> bindings =
      player->getClipBindings();
  out_names.reserve(bindings.size());
  for (const AnimationPlayer::ClipBinding& binding : bindings) {
    if (!binding.name.empty()) {
      out_names.push_back(binding.name);
    }
  }
}

/// Dropdown choices: leading empty clear option, then player logical names.
inline void buildBehaviourClipNameDropdownChoices(
    const Object* object, eastl::vector<eastl::string>& out_choices) {
  out_choices.clear();
  out_choices.push_back(eastl::string{});
  eastl::vector<eastl::string> names;
  collectBehaviourClipNameChoices(object, names);
  for (eastl::string& name : names) {
    out_choices.push_back(eastl::move(name));
  }
}

inline bool isBehaviourClipNameInvalid(
    const eastl::string& stored_name,
    const eastl::vector<eastl::string>& player_names) {
  if (stored_name.empty()) {
    return false;
  }
  for (const eastl::string& name : player_names) {
    if (name == stored_name) {
      return false;
    }
  }
  return true;
}

inline void fillInspectorBehaviourPropRow(
    InspectorBehaviourPropRowData& row, const BehaviourCatalogMember& member,
    const Variant* bag_value,
    const eastl::vector<eastl::string>* player_clip_names) {
  row.key = member.name;
  row.kind = behaviourCatalogKindName(member.kind);
  row.missing_type = false;
  row.clip_name_invalid = false;
  switch (member.kind) {
    case BehaviourCatalogMember::Kind::Bool:
      row.bool_value =
          bag_value != nullptr && bag_value->getType() == VariantType::Bool
              ? bag_value->asBool()
              : false;
      break;
    case BehaviourCatalogMember::Kind::Number:
      row.number_value =
          bag_value != nullptr &&
                  (bag_value->getType() == VariantType::Float ||
                   bag_value->getType() == VariantType::Int)
              ? bag_value->asFloat()
              : 0.0f;
      break;
    case BehaviourCatalogMember::Kind::String:
      row.string_value =
          bag_value != nullptr && bag_value->getType() == VariantType::String
              ? bag_value->asString()
              : eastl::string{};
      break;
    case BehaviourCatalogMember::Kind::ClipName:
      row.string_value =
          bag_value != nullptr && bag_value->getType() == VariantType::String
              ? bag_value->asString()
              : eastl::string{};
      if (player_clip_names != nullptr) {
        row.clip_name_invalid =
            isBehaviourClipNameInvalid(row.string_value, *player_clip_names);
      } else {
        row.clip_name_invalid = !row.string_value.empty();
      }
      break;
  }
}

inline void buildInspectorBehaviourRows(
    const Object* object, const eastl::vector<BehaviourCatalogType>& catalog,
    eastl::vector<InspectorBehaviourRowData>& out_rows) {
  out_rows.clear();
  if (object == nullptr) {
    return;
  }

  eastl::vector<eastl::string> player_clip_names;
  collectBehaviourClipNameChoices(object, player_clip_names);

  const size_t count = object->getBehaviourCount();
  out_rows.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const BehaviourId behaviour_id = object->getBehaviourIdAt(i);
    const char* type_name = object->getBehaviourTypeName(behaviour_id);
    if (type_name == nullptr) {
      continue;
    }

    InspectorBehaviourRowData row{};
    row.behaviour_id = behaviour_id;
    row.type_name = type_name;
    const BehaviourCatalogType* catalog_type =
        findBehaviourCatalogType(catalog, row.type_name);
    row.missing = catalog_type == nullptr;

    const eastl::vector<SceneBehaviourProperty>* bag =
        object->getBehaviourProperties(behaviour_id);
    if (catalog_type != nullptr) {
      row.props.reserve(catalog_type->members.size());
      for (const BehaviourCatalogMember& member : catalog_type->members) {
        InspectorBehaviourPropRowData prop_row{};
        fillInspectorBehaviourPropRow(prop_row, member,
                                      findBehaviourBagValue(bag, member.name),
                                      &player_clip_names);
        row.props.push_back(eastl::move(prop_row));
      }
    }
    out_rows.push_back(eastl::move(row));
  }
}

inline void buildBehaviourTypeChoices(
    const eastl::vector<BehaviourCatalogType>& catalog,
    eastl::vector<eastl::string>& out_choices) {
  out_choices.clear();
  out_choices.reserve(catalog.size());
  for (const BehaviourCatalogType& type : catalog) {
    out_choices.push_back(type.clr_name);
  }
}

}  // namespace Blunder
