#pragma once

#include <cstddef>
#include <vector>

#include "binary_output_sink.hpp"

namespace device_platform_test_support {

struct BinaryOutputCommand {
    bool enabled;
};

// Mock fuer genau einen binaeren Ausgang. Haelt den aktuellen Zustand und
// journalisiert jeden Befehl (fest begrenzt, aelteste Eintraege werden
// verworfen). Welche physische Rolle der Ausgang hat, entscheidet die
// Anwendung ueber die Zuordnung der konkreten Instanz, nicht dieser Mock.
class MockBinaryOutputSink final : public device_platform::IBinaryOutputSink {
   public:
    static constexpr std::size_t kMaxJournalEntries = 256;

    void setEnabled(bool enabled) override;

    [[nodiscard]] bool enabled() const;
    [[nodiscard]] const std::vector<BinaryOutputCommand>& commandJournal()
        const;

   private:
    bool enabled_{false};
    std::vector<BinaryOutputCommand> journal_;
};

}  // namespace device_platform_test_support
