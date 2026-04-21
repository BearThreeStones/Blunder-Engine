#pragma once

#include <EASTL/vector.h>

namespace Blunder {

class Layer;

/**
 * LayerStack - Container managing application layers.
 *
 * Organizes layers in two sections:
 *   [Layer1][Layer2][...LayerN] | [Overlay1][Overlay2][...OverlayM]
 *                               ^
 *                     m_layer_insert_index
 *
 * - Layers: Inserted at the insert index (grow from left)
 * - Overlays: Pushed to the end (always rendered on top)
 *
 * Processing order:
 *   - Update/Render: Forward iteration (Layer1 -> OverlayM)
 *   - Events: Reverse iteration (OverlayM -> Layer1) until handled
 *
 * Memory ownership: LayerStack owns all pushed layers and deletes them
 * on destruction.
 */
class LayerStack {
 public:
  LayerStack() = default;
  ~LayerStack();

  /**
   * Push a layer onto the stack.
   * Layer is inserted before overlays.
   * @param layer Pointer to the layer (ownership transferred).
   */
  void pushLayer(Layer* layer);

  /**
   * Push an overlay onto the stack.
   * Overlays are always rendered on top of regular layers.
   * @param overlay Pointer to the overlay (ownership transferred).
   */
  void pushOverlay(Layer* overlay);

  /**
   * Remove a layer from the stack.
   * Calls onDetach() on the layer. Does NOT delete the layer.
   * @param layer Pointer to the layer to remove.
   */
  void popLayer(Layer* layer);

  /**
   * Remove an overlay from the stack.
   * Calls onDetach() on the overlay. Does NOT delete the overlay.
   * @param overlay Pointer to the overlay to remove.
   */
  void popOverlay(Layer* overlay);

  // Forward iterators (for update/render)
  eastl::vector<Layer*>::iterator begin() { return m_layers.begin(); }
  eastl::vector<Layer*>::iterator end() { return m_layers.end(); }

  // Reverse iterators (for event propagation)
  eastl::vector<Layer*>::reverse_iterator rbegin() { return m_layers.rbegin(); }
  eastl::vector<Layer*>::reverse_iterator rend() { return m_layers.rend(); }

  // Const iterators
  eastl::vector<Layer*>::const_iterator begin() const { return m_layers.begin(); }
  eastl::vector<Layer*>::const_iterator end() const { return m_layers.end(); }
  eastl::vector<Layer*>::const_reverse_iterator rbegin() const { return m_layers.rbegin(); }
  eastl::vector<Layer*>::const_reverse_iterator rend() const { return m_layers.rend(); }

 private:
  eastl::vector<Layer*> m_layers;
  unsigned int m_layer_insert_index = 0;
};

}  // namespace Blunder
