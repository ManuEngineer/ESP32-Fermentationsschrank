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
        adoptedRevisions_ =
            decidable(revisions)
                ? std::optional<Revisions>{Revisions{
                      *revisions.expectedUserConfigurationRevision,
                      *revisions.expectedProgramCatalogRevision}}
                : std::nullopt;
    }

    // Frees the copy and invalidates the stored revisions.
    void evict() noexcept {
        source_.reset();
        adoptedRevisions_.reset();
    }

    // The current copy, or safe defaults (English, empty catalog) while no
    // copy exists; matches the former per-loop default behavior.
    [[nodiscard]] const FermentationUiPresentationSource& get() const noexcept {
        return source_.has_value() ? *source_ : defaultSource_;
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
    FermentationUiPresentationSource defaultSource_;
};

}  // namespace fermentation
