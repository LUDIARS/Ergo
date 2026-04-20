// observer_dummy.cpp
// Dummy plug implementation for Observer module.

#include "ergo/input/observer.h"
#include <memory>

namespace ergo::input::dummy {

std::unique_ptr<Observer> createDummyObserver() {
    return std::make_unique<Observer>(DeliveryPolicy::Immediate);
}

} // namespace ergo::input::dummy
