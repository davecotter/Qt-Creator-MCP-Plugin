/**
 * Cross-Platform Menu Icon Support - Implementation
 * 
 * On macOS: Uses native AppKit APIs to set menu icons when Qt's methods fail.
 * On other platforms: Provides stub implementations (Qt handles icons natively).
 */

#include "macos_menu_icon.h"

#ifdef Q_OS_MACOS

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#include <QTimer>

/**
 * Find an NSMenuItem by title within the Tools menu or main menu bar.
 */
static NSMenuItem* findMenuItemByTitle(NSString *title)
{
    NSMenu *mainMenu = [NSApp mainMenu];
    if (!mainMenu)
        return nil;
    
    // Search in the Tools menu first (most likely location)
    for (NSMenuItem *item in [mainMenu itemArray]) {
        NSString *itemTitle = [item title];
        if ([itemTitle isEqualToString:@"Tools"] || 
            [itemTitle localizedCaseInsensitiveContainsString:@"Tools"]) {
            NSMenu *toolsMenu = [item submenu];
            if (toolsMenu) {
                for (NSMenuItem *subItem in [toolsMenu itemArray]) {
                    if ([[subItem title] localizedCaseInsensitiveContainsString:title]) {
                        return subItem;
                    }
                }
            }
            break;
        }
    }
    
    // Fallback: search top-level menu bar
    for (NSMenuItem *item in [mainMenu itemArray]) {
        if ([[item title] localizedCaseInsensitiveContainsString:title]) {
            return item;
        }
    }
    
    return nil;
}

/**
 * Convert a QIcon to an NSImage at standard menu icon size (16x16).
 */
static NSImage* qIconToNSImage(const QIcon &icon)
{
    if (icon.isNull())
        return nil;
    
    QPixmap pixmap = icon.pixmap(16, 16);
    if (pixmap.isNull())
        return nil;
    
    QImage image = pixmap.toImage();
    CGImageRef cgImage = image.toCGImage();
    if (!cgImage)
        return nil;
    
    NSImage *nsImage = [[NSImage alloc] initWithCGImage:cgImage size:NSMakeSize(16, 16)];
    CGImageRelease(cgImage);
    
    return nsImage;
}

/**
 * Check if the menu icon is visible using native APIs.
 */
static bool isMenuIconVisible(const QString &menuTitle)
{
    @autoreleasepool {
        NSString *nsTitle = menuTitle.toNSString();
        NSMenuItem *menuItem = findMenuItemByTitle(nsTitle);
        return (menuItem && [menuItem image] != nil);
    }
}

/**
 * Force a menu icon using native AppKit APIs.
 */
static void forceMenuIconNative(const QString &menuTitle, const QIcon &icon)
{
    @autoreleasepool {
        NSString *nsTitle = menuTitle.toNSString();
        NSMenuItem *menuItem = findMenuItemByTitle(nsTitle);
        
        if (!menuItem)
            return;
        
        NSImage *nsImage = qIconToNSImage(icon);
        if (!nsImage)
            return;
        
        [menuItem setImage:nsImage];
        
        // Enable state column on parent menu for better alignment
        NSMenu *parentMenu = [menuItem menu];
        if (parentMenu) {
            [parentMenu setShowsStateColumn:YES];
        }
    }
}

void setMenuIconWithFallback(QMenu *menu, const QIcon &icon, const QString &menuTitle)
{
    if (!menu || icon.isNull())
        return;
    
    // Qt's icon methods are called by the caller before this function.
    // Use a delayed check to see if Qt's method worked, then use native fallback.
    QTimer::singleShot(200, [menuTitle, icon]() {
        if (!isMenuIconVisible(menuTitle)) {
            forceMenuIconNative(menuTitle, icon);
        }
    });
}

#else

// Stub implementations for Windows/Linux - Qt handles icons natively
void setMenuIconWithFallback(QMenu *menu, const QIcon &icon, const QString &menuTitle)
{
    Q_UNUSED(menu);
    Q_UNUSED(icon);
    Q_UNUSED(menuTitle);
    // No-op: Qt's native icon support works on these platforms
}

#endif
