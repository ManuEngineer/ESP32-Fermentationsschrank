#pragma once

namespace device_platform {

// Anwendungsneutraler Port fuer den Netzwerkverbindungsstatus. Die Regelung
// ist netzwerkunabhaengig (siehe docs/ARCHITECTURE.md); dieser Port dient nur
// der Anzeige und Diagnose.
class INetworkStatus {
   public:
    INetworkStatus() = default;
    virtual ~INetworkStatus() = default;

    INetworkStatus(const INetworkStatus&) = delete;
    INetworkStatus& operator=(const INetworkStatus&) = delete;
    INetworkStatus(INetworkStatus&&) = delete;
    INetworkStatus& operator=(INetworkStatus&&) = delete;

    [[nodiscard]] virtual bool isConnected() const = 0;
};

}  // namespace device_platform
