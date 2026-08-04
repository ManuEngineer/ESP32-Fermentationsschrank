#pragma once

#include <optional>
#include <string>

#include "configuration_document_codec.hpp"

namespace fermentation {

// The payload bytes are produced by the same internal ProgramDocument codec
// as ProgramCatalog. Run persistence consumes this narrow public helper and
// therefore cannot grow a second, divergent ProgramDocument wire format.
[[nodiscard]] ConfigurationCodecStatus encodeProgramDocumentPayload(
    const ProgramDocument& document, std::string& out);

[[nodiscard]] ConfigurationDecodeResult<ProgramDocument>
decodeProgramDocumentPayload(const std::string& payload);

}  // namespace fermentation
