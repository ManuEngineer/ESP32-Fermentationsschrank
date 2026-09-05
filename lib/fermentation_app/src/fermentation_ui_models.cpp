#include "fermentation_ui_models.hpp"

#include <limits>

namespace fermentation {

namespace {

bool equalEffectiveValues(const std::optional<EffectiveRunValues>& left,
                          const std::optional<EffectiveRunValues>& right) {
    if (left.has_value() != right.has_value()) return false;
    return !left.has_value() ||
           (left->targetTemperatureCelsius == right->targetTemperatureCelsius &&
            left->remainingDurationMinutes == right->remainingDurationMinutes);
}

bool equalRuntimeMessage(const RuntimeMessage& left,
                         const RuntimeMessage& right) {
    return left.id == right.id && left.code == right.code &&
           left.messageClass == right.messageClass &&
           left.priority == right.priority && left.trigger == right.trigger &&
           left.monotonicMillis == right.monotonicMillis &&
           left.active == right.active &&
           left.acknowledged == right.acknowledged &&
           left.resolved == right.resolved &&
           left.decisionRequired == right.decisionRequired &&
           left.acousticMuted == right.acousticMuted &&
           left.acousticIntent == right.acousticIntent &&
           left.runRevision == right.runRevision &&
           left.stateSequence == right.stateSequence &&
           left.faultRevision == right.faultRevision &&
           left.revision == right.revision;
}

bool equalQuality(const device_platform::SensorQualitySnapshot& left,
                  const device_platform::SensorQualitySnapshot& right) {
    return left.identity == right.identity && left.quality == right.quality &&
           left.rawCelsius == right.rawCelsius &&
           left.correctedCelsius == right.correctedCelsius &&
           left.filteredCelsius == right.filteredCelsius &&
           left.appliedOffset == right.appliedOffset &&
           left.lastAcceptedSampleAgeMs == right.lastAcceptedSampleAgeMs &&
           left.lastValidSampleAgeMs == right.lastValidSampleAgeMs &&
           left.lastFaultReason == right.lastFaultReason &&
           left.consecutiveInvalidCount == right.consecutiveInvalidCount &&
           left.recoveryProgressCount == right.recoveryProgressCount &&
           left.changeRateCelsiusPerSecond == right.changeRateCelsiusPerSecond;
}

bool equalPresentation(const PresentationState& left,
                       const PresentationState& right) {
    return left.faultCode == right.faultCode &&
           left.acknowledged == right.acknowledged &&
           left.resetCause == right.resetCause &&
           left.applicationAllocationFailure ==
               right.applicationAllocationFailure;
}

}  // namespace

bool equalFermentationUiSemanticSnapshot(
    const FermentationUiSnapshot& left,
    const FermentationUiSnapshot& right) noexcept {
    if (left.revisions.expectedStateSequence !=
            right.revisions.expectedStateSequence ||
        left.revisions.expectedRunRevision !=
            right.revisions.expectedRunRevision ||
        left.revisions.expectedMessageRevision !=
            right.revisions.expectedMessageRevision ||
        left.revisions.expectedFaultRevision !=
            right.revisions.expectedFaultRevision ||
        left.revisions.expectedRecoveryEpisodeRevision !=
            right.revisions.expectedRecoveryEpisodeRevision ||
        left.revisions.expectedUserConfigurationRevision !=
            right.revisions.expectedUserConfigurationRevision ||
        left.revisions.expectedProgramCatalogRevision !=
            right.revisions.expectedProgramCatalogRevision ||
        left.home.mode != right.home.mode ||
        left.home.processState != right.home.processState ||
        left.home.activeRunId != right.home.activeRunId ||
        !equalEffectiveValues(left.home.effectiveValues,
                              right.home.effectiveValues) ||
        left.home.primaryAction != right.home.primaryAction ||
        left.navigation.semanticActions != right.navigation.semanticActions ||
        left.temperatures.size() != right.temperatures.size() ||
        left.messages.size() != right.messages.size() ||
        left.recovery.mode != right.recovery.mode ||
        left.recovery.canonicalRecoveryDisposition !=
            right.recovery.canonicalRecoveryDisposition ||
        left.recovery.persistenceLoadStatus !=
            right.recovery.persistenceLoadStatus ||
        left.recovery.coordinatorState != right.recovery.coordinatorState ||
        !equalPresentation(left.status.presentation,
                           right.status.presentation) ||
        left.status.ready != right.status.ready ||
        left.service.available != right.service.available ||
        left.service.confirmationRequired !=
            right.service.confirmationRequired ||
        left.service.serviceAuthorizationRequired !=
            right.service.serviceAuthorizationRequired ||
        left.service.unavailableReason != right.service.unavailableReason) {
        return false;
    }
    for (std::size_t i = 0U; i < left.temperatures.size(); ++i) {
        if (left.temperatures[i].role != right.temperatures[i].role ||
            left.temperatures[i].valueCelsius !=
                right.temperatures[i].valueCelsius ||
            !equalQuality(left.temperatures[i].quality,
                          right.temperatures[i].quality)) {
            return false;
        }
    }
    for (std::size_t i = 0U; i < left.messages.size(); ++i) {
        if (!equalRuntimeMessage(left.messages[i].message,
                                 right.messages[i].message)) {
            return false;
        }
    }
    return true;
}

device_platform::UiRefreshRevision
FermentationUiRefreshRevisionTracker::publish(
    const FermentationUiSnapshot& snapshot) {
    if (!published_.has_value() ||
        !equalFermentationUiSemanticSnapshot(*published_, snapshot)) {
        if (revision_.value != std::numeric_limits<std::uint64_t>::max()) {
            ++revision_.value;
        }
        published_ = snapshot;
        published_->refreshRevision.reset();
    }
    return revision_;
}

device_platform::DeviceUiBuildCatalog makeFermentationR1DeviceUiBuildCatalog() {
    return {device_platform::BrandingId{"manuengineer"},
            {device_platform::BrandingId{"manuengineer"}},
            {device_platform::LocaleId{"de"}, device_platform::LocaleId{"en"},
             device_platform::LocaleId{"es"}},
            {device_platform::ThemeId{"manuengineer-dark"}},
            device_platform::LocaleId{"en"},
            device_platform::ThemeId{"manuengineer-dark"}};
}

std::vector<device_platform::ThemeDescriptor>
makeFermentationR1ThemeDescriptors() {
    using device_platform::ThemeDescriptor;
    using device_platform::ThemeId;
    using device_platform::ThemeToken;
    return {
        {ThemeId{"manuengineer-dark"},
         {ThemeToken::Canvas, ThemeToken::Surface, ThemeToken::PrimaryAction,
          ThemeToken::SecondaryAction, ThemeToken::TextPrimary,
          ThemeToken::TextSecondary, ThemeToken::StatusInformation,
          ThemeToken::StatusWarning, ThemeToken::StatusError,
          ThemeToken::Overlay, ThemeToken::OnCanvas, ThemeToken::OnSurface,
          ThemeToken::OnPrimaryAction, ThemeToken::OnSecondaryAction,
          ThemeToken::OnStatusInformation, ThemeToken::OnStatusWarning,
          ThemeToken::OnStatusError, ThemeToken::OnOverlay}}};
}

device_platform::ServiceSessionPolicy fermentationTouchServicePolicy() {
    return {10U * 60U * 1000U, std::nullopt};
}

device_platform::ServiceSessionPolicy fermentationWebServicePolicy() {
    return {5U * 60U * 1000U, 15U * 60U * 1000U};
}

}  // namespace fermentation
