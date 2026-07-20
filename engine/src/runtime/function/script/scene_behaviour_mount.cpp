#include "runtime/function/script/scene_behaviour_mount.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/script/dotnet_host.h"

#include <cstdio>

namespace Blunder {
namespace {

void appendJsonString(eastl::string& out, const eastl::string& value) {
  out.append("\"");
  for (size_t i = 0; i < value.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    switch (c) {
      case '"':
        out.append("\\\"");
        break;
      case '\\':
        out.append("\\\\");
        break;
      case '\n':
        out.append("\\n");
        break;
      case '\r':
        out.append("\\r");
        break;
      case '\t':
        out.append("\\t");
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out.append(buf);
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  out.append("\"");
}

void appendPropertyValue(eastl::string& out, const Variant& value) {
  char buffer[64];
  switch (value.getType()) {
    case VariantType::Bool:
      out.append(value.asBool() ? "true" : "false");
      break;
    case VariantType::Int:
      std::snprintf(buffer, sizeof(buffer), "%lld",
                    static_cast<long long>(value.asInt()));
      out.append(buffer);
      break;
    case VariantType::Float:
      std::snprintf(buffer, sizeof(buffer), "%.6g",
                    static_cast<double>(value.asFloat()));
      out.append(buffer);
      break;
    case VariantType::String:
      appendJsonString(out, value.asString());
      break;
    default:
      // Vec3/Quat/Nil not in MVP property bag — skip caller-side.
      out.append("null");
      break;
  }
}

eastl::string propertiesToJson(
    const eastl::vector<SceneBehaviourProperty>& properties) {
  eastl::string out = "{";
  bool first = true;
  for (const SceneBehaviourProperty& prop : properties) {
    if (prop.value.getType() != VariantType::Bool &&
        prop.value.getType() != VariantType::Int &&
        prop.value.getType() != VariantType::Float &&
        prop.value.getType() != VariantType::String) {
      continue;
    }
    if (!first) {
      out.append(",");
    }
    first = false;
    appendJsonString(out, prop.key);
    out.append(":");
    appendPropertyValue(out, prop.value);
  }
  out.append("}");
  return out;
}

const SceneBehaviourDeclaration* findDeclaration(
    const Scene& scene, const eastl::string& entity_name, BehaviourId id) {
  for (const SceneEntityDefinition& definition : scene.getEntities()) {
    if (definition.name != entity_name) {
      continue;
    }
    for (const SceneBehaviourDeclaration& decl : definition.behaviours) {
      if (decl.id == id) {
        return &decl;
      }
    }
  }
  return nullptr;
}

void mountObjectBehaviours(Object& object, DotNetHost& host,
                           const Scene* scene) {
  const eastl::string& entity_name = object.getName();
  for (size_t i = 0; i < object.getBehaviourCount(); ++i) {
    const BehaviourId id = object.getBehaviourIdAt(i);
    if (!isValidBehaviourId(id)) {
      continue;
    }
    if (object.getBehaviourScriptPeer(id) != nullptr) {
      continue;
    }
    const char* type_name = object.getBehaviourTypeName(id);
    if (type_name == nullptr || type_name[0] == '\0') {
      LOG_WARN("[SceneMount] skip Behaviour id={} with empty type on '{}'",
               static_cast<unsigned long long>(id), entity_name.c_str());
      continue;
    }

    BehaviourId mount_id = id;
    eastl::string err;
    if (!host.attachBehaviour(object.getId(), type_name, &mount_id, err)) {
      LOG_WARN("[SceneMount] AttachBehaviour failed type='{}' id={} on '{}': {}",
               type_name, static_cast<unsigned long long>(id),
               entity_name.c_str(), err.c_str());
      continue;
    }

    if (scene == nullptr) {
      continue;
    }
    const SceneBehaviourDeclaration* decl =
        findDeclaration(*scene, entity_name, id);
    if (decl == nullptr || decl->properties.empty()) {
      continue;
    }
    const eastl::string json = propertiesToJson(decl->properties);
    if (json == "{}") {
      continue;
    }
    if (!host.applyBehaviourProperties(object.getId(), mount_id, json.c_str(),
                                       err)) {
      LOG_WARN(
          "[SceneMount] ApplyBehaviourProperties failed type='{}' id={} on "
          "'{}': {}",
          type_name, static_cast<unsigned long long>(id), entity_name.c_str(),
          err.c_str());
    }
  }
}

}  // namespace

void mountSceneBehaviours(SceneInstance& instance, DotNetHost& host,
                          const Scene* scene) {
  if (!host.isRunning() || !host.hasGameAssembly()) {
    return;
  }

  instance.forEachEntity([&](EntityId entity_id, const Entity&) {
    Object* object = ObjectDB::findByEntityId(entity_id);
    if (object == nullptr || object->getBehaviourCount() == 0) {
      return;
    }
    mountObjectBehaviours(*object, host, scene);
  });
}

}  // namespace Blunder
