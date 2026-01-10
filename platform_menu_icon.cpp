/**
 * Cross-Platform Menu Icon Support - Non-macOS Implementation
 * 
 * On Windows/Linux: Qt handles menu icons natively, so this provides stub implementations.
 * On macOS: The macos_menu_icon.mm file provides the actual implementation.
 */

#include "macos_menu_icon.h"

#ifndef Q_OS_MACOS

// Stub implementations for Windows/Linux - Qt handles icons natively
void setMenuIconWithFallback(QMenu *menu, const QIcon &icon, const QString &menuTitle)
{
    Q_UNUSED(menu);
    Q_UNUSED(icon);
    Q_UNUSED(menuTitle);
    // No-op: Qt's native icon support works on these platforms
}

#endif
