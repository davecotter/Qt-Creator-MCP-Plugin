/**
 * Cross-Platform Menu Icon Support
 * 
 * Provides menu icon support that works across platforms:
 * - Windows/Linux: Icons work natively in Qt, these functions are no-ops
 * - macOS: Qt may hide menu icons per Apple HIG; these functions provide
 *          native AppKit fallback to force icon display
 * 
 * Note: On macOS, menu alignment may be imperfect since the icon column
 * only appears when at least one item has an image and the menu style
 * supports images.
 */

#ifndef MACOS_MENU_ICON_H
#define MACOS_MENU_ICON_H

#include <QMenu>
#include <QIcon>
#include <QString>

/**
 * Set a menu icon with platform-appropriate fallback.
 * 
 * On Windows/Linux: Simply sets the icon via Qt (icons work by default)
 * On macOS: Tries Qt first, then uses native AppKit API as fallback
 * 
 * @param menu The QMenu to set the icon on
 * @param icon The QIcon to display
 * @param menuTitle The title of the menu (used to find it in native APIs)
 */
void setMenuIconWithFallback(QMenu *menu, const QIcon &icon, const QString &menuTitle);

#endif // MACOS_MENU_ICON_H
