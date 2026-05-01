#pragma once

#include <EASTL/string.h>

namespace Blunder {

class Event;

/**
 * Layer - Base class for application layers.
 *
 * Layers provide a modular architecture for organizing distinct functional
 * components (UI, game logic, debugging tools). Each layer receives lifecycle
 * callbacks for attachment, updates, rendering, and event handling.
 *
 * Usage:
 *   class GameLayer : public Layer {
 *    public:
 *     GameLayer() : Layer("GameLayer") {}
 *     void onAttach() override { // init }
 *     void onUpdate(float delta_time) override { // update }
 *     void onEvent(Event& e) override { // handle events }
 *   };
 */
class Layer {
 public:
  Layer(const eastl::string& name = "Layer");
  virtual ~Layer() = default;

  /**
   * Called when the layer is added to the LayerStack.
   * Use for initialization, resource allocation.
   */
  virtual void onAttach() {}

  /**
   * Called when the layer is removed from the LayerStack.
   * Use for cleanup, resource deallocation.
   */
  virtual void onDetach() {}

  /**
   * Called every frame for game logic updates.
   * @param delta_time Time elapsed since last frame in seconds.
   */
  virtual void onUpdate(float delta_time) {}

  /**
   * Called when an event is dispatched to this layer.
   * Set event.handled = true to stop propagation.
   * @param event The event to handle.
   */
  virtual void onEvent(Event& event) {}

  /**
   * Get the layer's debug name.
   * @return Reference to the layer's name string.
   */
  const eastl::string& getName() const { return m_debug_name; }

 protected:
  eastl::string m_debug_name;
};

}  // namespace Blunder
