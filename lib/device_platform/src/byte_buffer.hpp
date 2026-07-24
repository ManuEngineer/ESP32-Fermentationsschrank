#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace device_platform {

// Begrenzter binaersicherer Schreibpuffer ohne unkontrollierte Allokation:
// die Kapazitaet wird bei Konstruktion einmal festgelegt und beim ersten Mal
// vollstaendig reserviert. Ein Schreibversuch, der die Kapazitaet
// ueberschreiten wuerde, schlaegt fehl und laesst den Puffer unveraendert.
class ByteWriter {
   public:
    explicit ByteWriter(std::size_t capacity) : capacity_(capacity) {
        buffer_.reserve(capacity);
    }

    [[nodiscard]] bool writeBytes(const void* data, std::size_t length) {
        // Laenge 0 mit moeglicherweise nullptr `data` vor jeder
        // Pointerarithmetik/-kopie abfangen: ein Nullzeiger als Beginn eines
        // (auch leeren) Bereichs ist fuer `std::string::append` nicht
        // dokumentiert sicher.
        if (length == 0U) {
            return true;
        }
        if (length > capacity_ - buffer_.size()) {
            return false;
        }
        buffer_.append(static_cast<const char*>(data), length);
        return true;
    }

    [[nodiscard]] bool writeByte(uint8_t value) {
        return writeBytes(&value, 1U);
    }

    [[nodiscard]] const std::string& bytes() const { return buffer_; }
    [[nodiscard]] std::size_t size() const { return buffer_.size(); }
    [[nodiscard]] std::size_t capacity() const { return capacity_; }
    [[nodiscard]] std::size_t remaining() const {
        return capacity_ - buffer_.size();
    }

   private:
    std::size_t capacity_;
    std::string buffer_;
};

// Begrenzter binaersicherer Lesepuffer: liest ausschliesslich aus dem
// tatsaechlich vorhandenen Bereich. Ein Leseversuch ueber das Pufferende
// hinaus schlaegt fehl, ohne die Position zu veraendern.
class ByteReader {
   public:
    explicit ByteReader(const std::string& bytes) : bytes_(bytes) {}

    [[nodiscard]] bool readBytes(void* out, std::size_t length) {
        // Siehe ByteWriter::writeBytes: Laenge 0 mit moeglicherweise
        // nullptr `out` vor jeder Pointerarithmetik/-kopie abfangen.
        if (length == 0U) {
            return true;
        }
        if (length > bytes_.size() - position_) {
            return false;
        }
        std::char_traits<char>::copy(static_cast<char*>(out),
                                     bytes_.data() + position_, length);
        position_ += length;
        return true;
    }

    [[nodiscard]] bool readByte(uint8_t& out) {
        char value = 0;
        if (!readBytes(&value, 1U)) {
            return false;
        }
        out = static_cast<uint8_t>(value);
        return true;
    }

    [[nodiscard]] std::size_t position() const { return position_; }
    [[nodiscard]] std::size_t remaining() const {
        return bytes_.size() - position_;
    }
    [[nodiscard]] std::size_t totalSize() const { return bytes_.size(); }

   private:
    const std::string& bytes_;
    std::size_t position_{0U};
};

}  // namespace device_platform
