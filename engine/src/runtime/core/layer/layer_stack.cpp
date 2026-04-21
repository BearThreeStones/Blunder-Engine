#include "runtime/core/layer/layer_stack.h"

#include "runtime/core/layer/layer.h"

#include <EASTL/algorithm.h>

namespace Blunder {

LayerStack::~LayerStack() {
  for (Layer* layer : m_layers) {
    layer->onDetach();
    delete layer;
  }
}

void LayerStack::pushLayer(Layer* layer) {
  m_layers.insert(m_layers.begin() + m_layer_insert_index, layer);
  m_layer_insert_index++;
}

void LayerStack::pushOverlay(Layer* overlay) {
  m_layers.push_back(overlay);
}

void LayerStack::popLayer(Layer* layer) {
  auto it = eastl::find(m_layers.begin(), m_layers.begin() + m_layer_insert_index, layer);
  if (it != m_layers.begin() + m_layer_insert_index) {
    layer->onDetach();
    m_layers.erase(it);
    m_layer_insert_index--;
  }
}

void LayerStack::popOverlay(Layer* overlay) {
  auto it = eastl::find(m_layers.begin() + m_layer_insert_index, m_layers.end(), overlay);
  if (it != m_layers.end()) {
    overlay->onDetach();
    m_layers.erase(it);
  }
}

}  // namespace Blunder
