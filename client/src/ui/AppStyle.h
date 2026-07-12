#pragma once
#include <QFile>
#include <QString>

// Application color system — centralized to prevent values from scattering across .cpp files.
namespace AppStyle {

    inline constexpr auto kColorPrimary   = "#07C160";  // Outgoing bubble bg
    inline constexpr auto kColorOnPrimary = "#FFFFFF";  // Outgoing bubble text
    inline constexpr auto kColorBubbleIn  = "#F0F0F0";  // Incoming bubble bg
    inline constexpr auto kColorChatBg    = "#EDEDED";  // Chat area background
    inline constexpr auto kColorPanelBg   = "#F7F7F7";  // Left-panel background
    inline constexpr auto kColorBorder    = "#E0E0E0";  // Dividers / borders
    inline constexpr auto kColorDanger    = "#E53E3E";  // Destructive action button

    // Load the compiled-in QSS resource.
    // Returns an empty string if the resource is not found (non-fatal).
    inline QString loadStyleSheet() {
        QFile qss(":/style.qss");
        if (!qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        return QString::fromUtf8(qss.readAll());
    }

} // namespace AppStyle
