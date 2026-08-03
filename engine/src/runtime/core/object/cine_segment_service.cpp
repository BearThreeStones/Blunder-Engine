#include "runtime/core/object/cine_segment_service.h"

namespace Blunder {

CineSegmentService::CineSegmentService() = default;

CineSegmentService::~CineSegmentService() = default;

bool CineSegmentService::enter(bool suppress_gameplay_input) {
  m_in_cine = true;
  m_suppress_gameplay_input = suppress_gameplay_input;
  return true;
}

bool CineSegmentService::end() {
  if (!m_in_cine) {
    return false;
  }

  m_in_cine = false;
  m_suppress_gameplay_input = false;
  return true;
}

bool CineSegmentService::isInCine() const { return m_in_cine; }

bool CineSegmentService::isGameplayInputSuppressed() const {
  return m_in_cine && m_suppress_gameplay_input;
}

void CineSegmentService::resetForTests() {
  m_in_cine = false;
  m_suppress_gameplay_input = false;
}

CineSegmentService& cineSegmentService() {
  static CineSegmentService s_service;
  return s_service;
}

}  // namespace Blunder
