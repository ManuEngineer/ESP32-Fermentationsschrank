#pragma once

#include <cstdint>
#include <string>

namespace device_platform {

enum class NotificationSeverity : std::uint8_t {
    Info,
    Warning,
    Fault,
};

// Anwendungsneutraler Port fuer lokale Benutzerbenachrichtigungen (Display,
// Web, Summer). Dieser Port kennt keine anwendungsspezifischen Texte; die
// konkreten Meldungstexte liefert die Anwendung.
class IUserNotificationSink {
   public:
    IUserNotificationSink() = default;
    virtual ~IUserNotificationSink() = default;

    IUserNotificationSink(const IUserNotificationSink&) = delete;
    IUserNotificationSink& operator=(const IUserNotificationSink&) = delete;
    IUserNotificationSink(IUserNotificationSink&&) = delete;
    IUserNotificationSink& operator=(IUserNotificationSink&&) = delete;

    virtual void notify(NotificationSeverity severity,
                        const std::string& message) = 0;
};

}  // namespace device_platform
