#pragma once

namespace Blunder {

/// Runtime CINE segment state: thin enter/exit hooks and in-CINE marking.
///
/// Segment end is authoritative via explicit end(); member AnimationPlayer
/// finished notifications do not alone clear the in-CINE mark (task 2.3+).
class CineSegmentService {
 public:
  CineSegmentService();
  ~CineSegmentService();

  CineSegmentService(const CineSegmentService&) = delete;
  CineSegmentService& operator=(const CineSegmentService&) = delete;

  /// Enter an active CINE segment and set the in-CINE mark.
  bool enter();

  /// Explicit segment end; clears the in-CINE mark when active.
  bool end();

  bool isInCine() const;

  /// Clears in-CINE state (unit tests).
  void resetForTests();

 private:
  bool m_in_cine{false};
};

CineSegmentService& cineSegmentService();

}  // namespace Blunder
