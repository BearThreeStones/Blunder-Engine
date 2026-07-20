#pragma once

#include "runtime/core/object/object.h"

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

  // Visit every occupied Object (stable for later world Tick).
  static void forEach(ObjectVisitorFn fn);
  static void forEach(ObjectVisitorUserFn fn, void* user);

  static void setEntityStore(IEntityStore* store);
  static IEntityStore* getEntityStore();
};

}  // namespace Blunder
