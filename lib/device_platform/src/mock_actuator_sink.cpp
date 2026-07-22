#include "mock_actuator_sink.hpp"

namespace device_platform {

void MockActuatorSink::setHeating(bool enabled) {
    heating_ = enabled;
    record(ActuatorCommandKind::Heating, enabled);
    checkSimultaneousDirections();
}

void MockActuatorSink::setCooling(bool enabled) {
    cooling_ = enabled;
    record(ActuatorCommandKind::Cooling, enabled);
    checkSimultaneousDirections();
}

void MockActuatorSink::setInsideFan(bool enabled) {
    insideFan_ = enabled;
    record(ActuatorCommandKind::InsideFan, enabled);
}

void MockActuatorSink::setOutsideFan(bool enabled) {
    outsideFan_ = enabled;
    record(ActuatorCommandKind::OutsideFan, enabled);
}

void MockActuatorSink::setBuzzer(bool enabled) {
    buzzer_ = enabled;
    record(ActuatorCommandKind::Buzzer, enabled);
}

bool MockActuatorSink::heating() const { return heating_; }

bool MockActuatorSink::cooling() const { return cooling_; }

bool MockActuatorSink::insideFan() const { return insideFan_; }

bool MockActuatorSink::outsideFan() const { return outsideFan_; }

bool MockActuatorSink::buzzer() const { return buzzer_; }

const std::vector<ActuatorCommand>& MockActuatorSink::commandJournal() const {
    return journal_;
}

bool MockActuatorSink::simultaneousDirectionsObserved() const {
    return simultaneousDirectionsObserved_;
}

void MockActuatorSink::record(ActuatorCommandKind kind, bool enabled) {
    if (journal_.size() >= kMaxJournalEntries) {
        journal_.erase(journal_.begin());
    }
    journal_.push_back(ActuatorCommand{kind, enabled});
}

void MockActuatorSink::checkSimultaneousDirections() {
    if (heating_ && cooling_) {
        simultaneousDirectionsObserved_ = true;
    }
}

}  // namespace device_platform
