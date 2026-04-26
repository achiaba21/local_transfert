// Sprint UX-3 : support drag & drop depuis Finder → fenêtre SFML.
// Objective-C++ avec ARC. Les méthodes NSDraggingDestination sont
// injectées dynamiquement dans la classe de la contentView SFML via
// l'Objective-C runtime — zéro subclassing, zéro modification de SFML.

#include "ltr/ui/drag_drop.hpp"

#include <objc/runtime.h>
#import <Cocoa/Cocoa.h>

#include "ltr/core/logger.hpp"

namespace ltr::ui {

namespace {

// Holder Objective-C qui porte les callbacks C++ + est associé à la
// view via objc_setAssociatedObject. Le holder est libéré
// automatiquement au dealloc de la view (retainé par l'association).
} // namespace
} // namespace ltr::ui

@interface LTRDragDelegate : NSObject
@property (nonatomic) ltr::ui::DragDropHandler::Callbacks cb;
@end

@implementation LTRDragDelegate
@end

namespace ltr::ui {
namespace {

// Clé unique pour l'associated object.
static const void* kDragDelegateKey = &kDragDelegateKey;

// IMP de `-(NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender`
static NSDragOperation ltr_draggingEntered(id self, SEL /*_cmd*/,
                                            id<NSDraggingInfo> /*info*/) {
    core::log_debug("[drag-drop/mac] draggingEntered");
    LTRDragDelegate* d =
        objc_getAssociatedObject(self, kDragDelegateKey);
    if (d && d.cb.onEnter) d.cb.onEnter();
    return NSDragOperationCopy;
}

// IMP de `-(void)draggingExited:(id<NSDraggingInfo>)sender`
static void ltr_draggingExited(id self, SEL /*_cmd*/,
                                id<NSDraggingInfo> /*info*/) {
    LTRDragDelegate* d =
        objc_getAssociatedObject(self, kDragDelegateKey);
    if (d && d.cb.onExit) d.cb.onExit();
}

// IMP de `-(BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender`
// Appelée après draggingEntered et avant performDragOperation. Doit
// retourner YES pour autoriser le drop.
static BOOL ltr_prepareForDragOperation(id /*self*/, SEL /*_cmd*/,
                                         id<NSDraggingInfo> /*info*/) {
    return YES;
}

// IMP de `-(BOOL)performDragOperation:(id<NSDraggingInfo>)sender`
static BOOL ltr_performDragOperation(id self, SEL /*_cmd*/,
                                      id<NSDraggingInfo> sender) {
    core::log_debug("[drag-drop/mac] performDragOperation");
    NSPasteboard* pb = [sender draggingPasteboard];
    std::vector<std::filesystem::path> paths;

    // Legacy NSFilenamesPboardType (toujours supporté sur macOS 14+).
    NSArray* files = [pb propertyListForType:NSFilenamesPboardType];
    if ([files isKindOfClass:[NSArray class]]) {
        for (NSString* f in files) {
            if ([f isKindOfClass:[NSString class]]) {
                paths.emplace_back([f UTF8String]);
            }
        }
    }
    // Fallback moderne : NSURL file://
    if (paths.empty()) {
        NSArray<NSURL*>* urls =
            [pb readObjectsForClasses:@[[NSURL class]] options:nil];
        for (NSURL* u in urls) {
            if ([u isFileURL]) {
                paths.emplace_back([[u path] UTF8String]);
            }
        }
    }

    LTRDragDelegate* d =
        objc_getAssociatedObject(self, kDragDelegateKey);
    if (d && d.cb.onDrop && !paths.empty()) {
        d.cb.onDrop(std::move(paths));
    }
    return YES;
}

} // namespace

struct DragDropHandler::Impl {
    NSView* view = nil;  // __weak implicite : ARC désactivé pour ce struct
};

DragDropHandler::DragDropHandler() : impl_(std::make_unique<Impl>()) {}
DragDropHandler::~DragDropHandler() { detach(); }

bool DragDropHandler::attach(sf::WindowHandle handle, Callbacks cb) {
    @autoreleasepool {
        NSWindow* win = (__bridge NSWindow*)handle;
        if (!win) {
            core::log_error("[drag-drop/mac] handle null");
            return false;
        }
        NSView* view = [win contentView];
        if (!view) {
            core::log_error("[drag-drop/mac] contentView null");
            return false;
        }

        // Autoriser les drops de fichiers (legacy + moderne).
        [view registerForDraggedTypes:@[NSFilenamesPboardType,
                                         (NSString*)NSPasteboardTypeFileURL]];

        core::log_info(std::string("[drag-drop/mac] contentView class = ")
                       + class_getName([view class]));

        // class_replaceMethod : installe notre IMP que la méthode existe
        // déjà (héritée de NSView) ou pas. `class_addMethod` simple
        // n'aurait pas marché car NSView a des stubs NSDraggingDestination
        // qui retournent NSDragOperationNone → notre guard aurait skippé.
        //
        // Type encodings (ARM64 / x86_64) :
        //   "Q@:@" → NSUInteger (Q = unsigned long long) / id / SEL / id
        //   "v@:@" → void
        //   "c@:@" → BOOL
        Class cls = [view class];
        class_replaceMethod(cls, @selector(draggingEntered:),
                            (IMP)ltr_draggingEntered, "Q@:@");
        class_replaceMethod(cls, @selector(draggingExited:),
                            (IMP)ltr_draggingExited, "v@:@");
        class_replaceMethod(cls, @selector(performDragOperation:),
                            (IMP)ltr_performDragOperation, "c@:@");
        class_replaceMethod(cls, @selector(prepareForDragOperation:),
                            (IMP)ltr_prepareForDragOperation, "c@:@");

        // Attacher les callbacks via associated object (retainé).
        LTRDragDelegate* delegate = [LTRDragDelegate new];
        delegate.cb = cb;
        objc_setAssociatedObject(view, kDragDelegateKey, delegate,
                                  OBJC_ASSOCIATION_RETAIN_NONATOMIC);

        impl_->view = view;
        core::log_info("[drag-drop/mac] attached");
        return true;
    }
}

void DragDropHandler::detach() {
    if (!impl_ || !impl_->view) return;
    @autoreleasepool {
        // Retirer le delegate (les méthodes restent ajoutées à la classe
        // mais sans callbacks associés, elles sont no-op).
        objc_setAssociatedObject(impl_->view, kDragDelegateKey, nil,
                                  OBJC_ASSOCIATION_ASSIGN);
        [impl_->view unregisterDraggedTypes];
        impl_->view = nil;
    }
}

} // namespace ltr::ui
