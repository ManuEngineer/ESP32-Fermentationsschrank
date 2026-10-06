#pragma once

#include <optional>
#include <utility>

#include "fermentation_ui_models.hpp"

namespace fermentation {

// The single long-lived consumer copy of the presentation source (display
// locale, canonical time zone, program catalog) used by the local UI loop.
// It is invalidated only through the existing snapshot revisions
// (expectedUserConfigurationRevision / expectedProgramCatalogRevision) and is
// owned by the UI loop, not by FermentationApplication, so there is no second
// config owner. A revision is adopted only after the fill reported an explicit
// success; an unavailable fill or an undecidable revision keeps the previous
// copy and stored revisions unchanged so the next loop retries.
//
// HeaderNetwork does not consume the program catalog. Entering it evicts the
// copy (and its stored revisions) before the network-mode commit; leaving it
// refills through the same explicit fill contract.
class FermentationUiPresentationCache {
   public:
    // One call per loop step. `fill` returns
    // std::optional<FermentationUiPresentationSource>: a value only if the
    // configuration runtime granted its read lease.
    template <typename Fill>
    void update(bool networkPage,
                const FermentationUiExpectedRevisions& revisions, Fill&& fill) {
        if (networkPage) {
            evict();
            return;
        }
        if (!needsFill(revisions)) {
            return;
        }
        auto filled = std::forward<Fill>(fill)();
        if (!filled.has_value()) {
            return;
        }
        source_ = std::move(*filled);
        // Remembered separately so the HeaderNetwork eviction, which frees
        // the catalog copy, never reverts the language or time zone.
        lastLocale_ = source_->displayLocale;
        lastTimeZone_ = source_->canonicalTimeZoneId;
        adoptedRevisions_ =
            decidable(revisions)
                ? std::optional<Revisions>{Revisions{
                      *revisions.expectedUserConfigurationRevision,
                      *revisions.expectedProgramCatalogRevision}}
                : std::nullopt;
    }

    // Frees the copy and invalidates the stored revisions. The last filled
    // display locale and time zone are deliberately kept (see displayLocale()).
    void evict() noexcept {
        source_.reset();
        adoptedRevisions_.reset();
    }

    // The current copy, or safe defaults (English, empty catalog) while no
    // copy exists; matches the former per-loop default behavior.
    [[nodiscard]] const FermentationUiPresentationSource& get() const noexcept {
        return source_.has_value() ? *source_ : defaultSource_;
    }

    // Display locale / canonical time zone of the last successful fill. They
    // survive evict(), so the network page (which holds no copy) keeps drawing
    // in the language chosen by the user instead of a boot-time snapshot.
    // Before the first successful fill they are the safe defaults.
    [[nodiscard]] const device_platform::LocaleId& displayLocale()
        const noexcept {
        return lastLocale_.has_value() ? *lastLocale_
                                       : defaultSource_.displayLocale;
    }
    [[nodiscard]] const device_platform::TimeZoneId& canonicalTimeZoneId()
        const noexcept {
        return lastTimeZone_.has_value() ? *lastTimeZone_
                                         : defaultSource_.canonicalTimeZoneId;
    }

    // Catalog identity of the copy currently in use, for the renderer's
    // allocation-free render key: the revision adopted with the last
    // successful fill, or nullopt while no valid copy exists (never filled,
    // evicted on HeaderNetwork, or fill unavailable).
    [[nodiscard]] std::optional<ProgramCatalogRevision>
    adoptedProgramCatalogRevision() const noexcept {
        return adoptedRevisions_.has_value()
                   ? std::optional<ProgramCatalogRevision>{adoptedRevisions_
                                                               ->catalog}
                   : std::nullopt;
    }

    [[nodiscard]] bool hasCopy() const noexcept { return source_.has_value(); }
    [[nodiscard]] bool revisionsValid() const noexcept {
        return adoptedRevisions_.has_value();
    }

   private:
    struct Revisions {
        UserConfigurationRevision user;
        ProgramCatalogRevision catalog;
    };

    [[nodiscard]] static bool decidable(
        const FermentationUiExpectedRevisions& revisions) noexcept {
        return revisions.expectedUserConfigurationRevision.has_value() &&
               revisions.expectedProgramCatalogRevision.has_value();
    }

    [[nodiscard]] bool needsFill(
        const FermentationUiExpectedRevisions& revisions) const noexcept {
        if (!adoptedRevisions_.has_value()) {
            return true;
        }
        if (!decidable(revisions)) {
            return false;
        }
        return *revisions.expectedUserConfigurationRevision !=
                   adoptedRevisions_->user ||
               *revisions.expectedProgramCatalogRevision !=
                   adoptedRevisions_->catalog;
    }

    std::optional<FermentationUiPresentationSource> source_;
    std::optional<Revisions> adoptedRevisions_;
    std::optional<device_platform::LocaleId> lastLocale_;
    std::optional<device_platform::TimeZoneId> lastTimeZone_;
    FermentationUiPresentationSource defaultSource_;
};

}  // namespace fermentation
