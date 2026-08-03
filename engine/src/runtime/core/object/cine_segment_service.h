#pragma once

namespace Blunder {

/// Runtime CINE segment state: thin enter/exit hooks and in-CINE marking.
///
/// Pose alignment (Object TRS snap/restore), gameplay state-machine transitions,
/// and control handoff on Enter/End remain **C# Behaviour** responsibilities —
/// this service does not auto-snap transforms or drive DogWalk state machines
/// (CONTEXT **CINE**; [ADR 0023](../../../docs/adr/0023-animation-sync-group.md);
/// acceptance-checklist P4).
///
/// Segment end is authoritative via explicit end(); member AnimationPlayer
/// finished notifications may assist authors but do not alone clear the
/// in-CINE mark.
class CineSegmentService {
 public:
  CineSegmentService();
  ~CineSegmentService();

  CineSegmentService(const CineSegmentService&) = delete;
  CineSegmentService& operator=(const CineSegmentService&) = delete;

  /// Enter an active CINE segment and set the in-CINE mark.
  /// When @p suppress_gameplay_input is true, gameplay Move/Jump sampling is
  /// suppressed until explicit end().
  bool enter(bool suppress_gameplay_input = false);

  /// Explicit segment end; clears the in-CINE mark and input suppression.
  bool end();

  bool isInCine() const;

  /// True while in-CINE with gameplay input suppression enabled at enter.
  bool isGameplayInputSuppressed() const;

  /// Clears in-CINE state (unit tests).
  void resetForTests();

 private:
  bool m_in_cine{false};
  bool m_suppress_gameplay_input{false};
};

CineSegmentService& cineSegmentService();

}  // namespace Blunder
