// A line's right-click menu on a Mac (menu.h): an NSMenu, as a text field's, with each item's
// Command key beside it.

#include "app/menu.h"

#import <AppKit/AppKit.h>

// What an item chosen tells: its tag is the Edit.
@interface TwoPStartEditMenu : NSObject
@property(nonatomic, assign) NSInteger chosen;
@end

@implementation TwoPStartEditMenu
- (void)choose:(NSMenuItem*)item {
    self.chosen = item.tag;
}
@end

namespace menu {

Edit edit(SDL_Window* window, const Can& can) {
    (void)window;
    TwoPStartEditMenu* target = [[TwoPStartEditMenu alloc] init];
    target.chosen = static_cast<NSInteger>(Edit::None);
    NSMenu* m = [[NSMenu alloc] initWithTitle:@""];
    m.autoenablesItems = NO;
    const auto add = [&](NSString* words, NSString* key, Edit e, bool on) {
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:words action:@selector(choose:) keyEquivalent:key];
        item.target = target;
        item.tag = static_cast<NSInteger>(e);
        item.enabled = on;
        [m addItem:item];
    };
    add(@"Cut", @"x", Edit::Cut, can.cut);
    add(@"Copy", @"c", Edit::Copy, can.copy);
    add(@"Paste", @"v", Edit::Paste, can.paste);
    [m addItem:[NSMenuItem separatorItem]];
    add(@"Select All", @"a", Edit::SelectAll, can.selectAll);
    // At the pointer, in the screen's points. It returns once the menu has closed.
    [m popUpMenuPositioningItem:nil atLocation:[NSEvent mouseLocation] inView:nil];
    return static_cast<Edit>(target.chosen);
}

}  // namespace menu
