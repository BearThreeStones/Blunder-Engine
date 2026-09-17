# MeshOptimizer via FetchContent (cook-time Meshlets only).
# Do not vendor under engine/3rdparty/.

include(FetchContent)

set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  meshoptimizer
  GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
  GIT_TAG v0.24
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(meshoptimizer)
