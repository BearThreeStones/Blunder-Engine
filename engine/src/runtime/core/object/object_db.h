#pragma once

#include "runtime/core/object/object.h"

namespace Blunder {

class IEntityStore;

class ObjectDB {
 public:
  static void clear();
  static ObjectId create();
  static Object* get(ObjectId id);
  static void destroy(ObjectId id);

  static void setEntityStore(IEntityStore* store);
  static IEntityStore* getEntityStore();
};

}  // namespace Blunder
