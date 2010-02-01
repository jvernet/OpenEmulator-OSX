
/**
 * OpenEmulator
 * Mac OS X Inspector Controller
 * (C) 2009 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Controls the inspector window.
 */

#import <Cocoa/Cocoa.h>

#import "DeviceChooserController.h"

@interface InspectorController : NSWindowController
{
	IBOutlet NSObjectController *fDocumentObjectController;
	
	IBOutlet NSMatrix *fTabMatrix;
	
	IBOutlet NSView *fEmulationView;
	IBOutlet NSView *fExpansionsView;
	IBOutlet NSView *fStorageView;
	IBOutlet NSView *fPeripheralsView;
	IBOutlet NSView *fAudioView;
	
	int selectedViewIndex;
	id inspectedDocument;
	
	IBOutlet id fDeviceChooserController;
}

- (void) activeDocumentDidChange;
- (id) inspectedDocument;
- (void) setInspectedDocument:(id) theDocument;

- (void) toggleInspector:(id) sender;

- (void) selectView:(id) sender;
- (void) setView:(int) viewIndex;

- (void) restoreWindowState:(id) sender;
- (void) storeWindowState:(id) sender;

- (void) addDevices:(id) sender;

@end