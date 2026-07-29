#include "runtime/core/reflection/class_db.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

Variant get_animation_player_is_playing(const void* instance) {
  return Variant(
      static_cast<const AnimationPlayer*>(instance)->isPlaying());
}

Variant get_animation_player_is_looping(const void* instance) {
  return Variant(
      static_cast<const AnimationPlayer*>(instance)->isLooping());
}

void set_animation_player_is_looping(void* instance, const Variant& value) {
  static_cast<AnimationPlayer*>(instance)->setLoop(value.asBool());
}

Variant get_animation_player_playback_position(const void* instance) {
  return Variant(
      static_cast<const AnimationPlayer*>(instance)->getPlaybackPosition());
}

Variant get_animation_player_clip_length(const void* instance) {
  return Variant(static_cast<const AnimationPlayer*>(instance)->getClipLength());
}

}  // namespace

void register_animation_player_reflection() {
  ClassDB::registerClass("AnimationPlayer");
  ClassDB::addProperty("AnimationPlayer",
                       PropertyInfo{"is_playing", VariantType::Bool}, nullptr,
                       get_animation_player_is_playing);
  ClassDB::addProperty("AnimationPlayer",
                       PropertyInfo{"is_looping", VariantType::Bool},
                       set_animation_player_is_looping,
                       get_animation_player_is_looping);
  ClassDB::addProperty(
      "AnimationPlayer", PropertyInfo{"playback_position", VariantType::Float},
      nullptr, get_animation_player_playback_position);
  ClassDB::addProperty("AnimationPlayer",
                       PropertyInfo{"clip_length", VariantType::Float}, nullptr,
                       get_animation_player_clip_length);
}

}  // namespace Blunder
