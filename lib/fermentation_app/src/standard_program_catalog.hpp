#pragma once

#include <array>
#include <optional>
#include <string>

#include "program_model.hpp"

namespace fermentation {

class FactoryProgramCatalog {
   public:
    static constexpr std::size_t kProgramCount = 4U;

    [[nodiscard]] static std::array<ProgramDocument, kProgramCount> programs();
    [[nodiscard]] static std::optional<ProgramDocument> find(
        const std::string& id);
    [[nodiscard]] static std::optional<ProgramDocument> makeUserCopy(
        const std::string& factoryId, std::string userId, std::string userName);
};

class ActiveProgramSelection {
   public:
    [[nodiscard]] bool select(const ProgramDocument& program);
    void clear();

    [[nodiscard]] const ProgramDocument* selected() const;
    [[nodiscard]] ProgramDocument* mutableSelected();

   private:
    std::optional<ProgramDocument> selected_;
};

}  // namespace fermentation
