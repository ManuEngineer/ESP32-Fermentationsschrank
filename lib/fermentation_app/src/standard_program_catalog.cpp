#include "standard_program_catalog.hpp"

#include <utility>

namespace fermentation {
namespace {

ProgramDocument makeFactoryProgram(std::string id, std::string name,
                                   bool preheat,
                                   SensorPreference sensorPreference,
                                   CompletionMode completionMode) {
    ProgramDefinition program;
    program.id = std::move(id);
    program.name = std::move(name);
    program.builtIn = true;
    program.factoryCatalogEntry = true;
    program.resettable = true;
    program.userDeletable = true;
    program.installed = true;
    program.enabled = true;
    program.preheat = preheat;
    program.sensorPreference = sensorPreference;
    program.productSensorFailure.policy =
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout;
    program.fermentationStages.push_back({std::nullopt, std::nullopt});
    program.completion.mode = completionMode;
    return {{kCurrentProgramSchemaVersion, kCurrentRequiredProgramFields},
            std::move(program)};
}

}  // namespace

std::array<ProgramDocument, FactoryProgramCatalog::kProgramCount>
FactoryProgramCatalog::programs() {
    return {{makeFactoryProgram("yogurt-mild", "Joghurt mild", true,
                                SensorPreference::ProductIfAvailableElseAir,
                                CompletionMode::CoolAndHoldUntilManualStop),
             makeFactoryProgram("yogurt-firm", "Joghurt stichfest", true,
                                SensorPreference::ProductIfAvailableElseAir,
                                CompletionMode::CoolAndHoldUntilManualStop),
             makeFactoryProgram("milk-kefir", "Milchkefir", false,
                                SensorPreference::AirProductOptional,
                                CompletionMode::CoolAndHoldUntilManualStop),
             makeFactoryProgram("water-kefir", "Wasserkefir", false,
                                SensorPreference::AirProductOptional,
                                CompletionMode::FinishWithoutCooling)}};
}

std::optional<ProgramDocument> FactoryProgramCatalog::find(
    const std::string& id) {
    for (auto& program : programs()) {
        if (program.program.id == id) {
            return program;
        }
    }
    return std::nullopt;
}

std::optional<ProgramDocument> FactoryProgramCatalog::makeUserCopy(
    const std::string& factoryId, std::string userId, std::string userName) {
    auto source = find(factoryId);
    if (!source.has_value() || userId.empty() || userName.empty()) {
        return std::nullopt;
    }

    ProgramDocument copy = std::move(*source);
    copy.program.id = std::move(userId);
    copy.program.name = std::move(userName);
    copy.program.builtIn = false;
    copy.program.factoryCatalogEntry = false;
    copy.program.resettable = false;
    copy.program.userDeletable = true;
    copy.program.installed = true;
    return copy;
}

bool ActiveProgramSelection::select(const ProgramDocument& program) {
    if (!validateProgram(program, ValidationPurpose::CatalogTemplate).valid()) {
        return false;
    }
    selected_ = program;
    return true;
}

void ActiveProgramSelection::clear() { selected_.reset(); }

const ProgramDocument* ActiveProgramSelection::selected() const {
    return selected_ ? &*selected_ : nullptr;
}

ProgramDocument* ActiveProgramSelection::mutableSelected() {
    return selected_ ? &*selected_ : nullptr;
}

}  // namespace fermentation
