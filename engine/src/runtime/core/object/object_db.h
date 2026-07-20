#pragma once

#include "runtime/core/object/object.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class IEntityStore;

class ObjectDB {
 public:
  using ObjectVisitorFn = void (*)(Object* object);
  using ObjectVisitorUserFn = void (*)(Object* object, void* user);

  static void clear();
  static ObjectId create();
  static Object* get(ObjectId id);
  static void destroy(ObjectId id);
  /// First occupied Object bound to `entity_id`, or nullptr. MVP scan.
  static Object* findByEntityId(EntityId entity_id);

  // Visit every occupied Object (stable for later world Tick).
  static void forEach(ObjectVisitorFn fn);
  static void forEach(ObjectVisitorUserFn fn, void* user);

  static void setEntityStore(IEntityStore* store);
  static IEntityStore* getEntityStore();
};

}  // namespace Blunder
