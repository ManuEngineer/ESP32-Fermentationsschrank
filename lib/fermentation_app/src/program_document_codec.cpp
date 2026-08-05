#include "program_document_codec.hpp"

#include "configuration_document_codec_internal.hpp"

namespace fermentation {

ConfigurationCodecStatus encodeProgramDocumentPayload(
    const ProgramDocument& document, std::string& out) {
    return configuration_codec_internal::encodeSingleProgramDocumentPayload(
        document, out);
}

ConfigurationDecodeResult<ProgramDocument> decodeProgramDocumentPayload(
    const std::string& payload) {
    return configuration_codec_internal::decodeSingleProgramDocumentPayload(
        payload);
}

}  // namespace fermentation
