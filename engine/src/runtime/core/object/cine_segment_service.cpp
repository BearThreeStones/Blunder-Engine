#include "runtime/core/object/cine_segment_service.h"

namespace Blunder {

CineSegmentService::CineSegmentService() = default;

CineSegmentService::~CineSegmentService() = default;

bool CineSegmentService::enter() {
  m_in_cine = true;
  return true;
}

bool CineSegmentService::end() {
  if (!m_in_cine) {
    return false;
  }

  m_in_cine = false;
  return true;
}

bool CineSegmentService::isInCine() const { return m_in_cine; }

void CineSegmentService::resetForTests() { m_in_cine = false; }

CineSegmentService& cineSegmentService() {
  static CineSegmentService s_service;
  return s_service;
}

}  // namespace Blunder
