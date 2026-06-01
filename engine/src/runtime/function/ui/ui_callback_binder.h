#pragma once

#include "runtime/function/ui/ui_host.h"

namespace Blunder {

/// Binds Slint callbacks to UiHost with weak_ptr lifetime checks.
class UiCallbackBinder {
 public:
  template <typename Fn>
  static auto bind(const eastl::weak_ptr<UiHost>& host, Fn&& fn) {
    return [host, fn = Fn(fn)](auto&&... args) mutable {
      if (const auto locked = host.lock()) {
        if (!locked->isAcceptingCallbacks()) {
          return;
        }
        fn(*locked, static_cast<decltype(args)&&>(args)...);
      }
    };
  }
};

}  // namespace Blunder
