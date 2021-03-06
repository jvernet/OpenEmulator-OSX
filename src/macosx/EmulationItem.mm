
/**
 * OpenEmulator
 * Mac OS X Emulation Item
 * (C) 2010-2012 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Implements an emulation item
 */

#import "EmulationItem.h"

#import "NSStringAdditions.h"

#import "OEEmulation.h"

#import "DeviceInterface.h"
#import "StorageInterface.h"

@implementation EmulationItem

- (EmulationItem *)getGroup:(NSString *)group
{
    for (EmulationItem *item in children)
    {
        if ([[item uid] compare:group] == NSOrderedSame)
            return item;
    }
    
    EmulationItem *item = [[EmulationItem alloc] initGroup:group];
    [children addObject:item];
    [item release];
    
    return item;
}

- (id)initRootWithDocument:(Document *)theDocument
{
    self = [super init];
    
    if (self)
    {
        type = EMULATIONITEM_ROOT;
        uid = [@"" copy];
        children = [[NSMutableArray alloc] init];
        document = theDocument;
        
        if (![NSThread isMainThread])
            [document lockEmulation];
        
        // Get info
        OEEmulation *emulation = (OEEmulation *)[theDocument emulation];
        OEIds deviceIds = emulation->getDeviceIds();
        
        OEPortInfos portInfos;
        portInfos = emulation->getPortInfos();
        
        // Create items connected on ports
        EmulationItem *systemGroupItem = [self getGroup:@"system"];
        
        for (OEPortInfos::iterator i = portInfos.begin();
             i != portInfos.end();
             i++)
        {
            OEPortInfo port = *i;
            string deviceId = OEGetDeviceId(port.ref);
            OEComponent *theComponent = emulation->getComponent(deviceId);
            
            EmulationItem *item;
            OEIds::iterator foundDeviceId;
            foundDeviceId = find(deviceIds.begin(), deviceIds.end(), deviceId);
            if (theComponent && (foundDeviceId != deviceIds.end()))
            {
                item = [[EmulationItem alloc] initDevice:[NSString stringWithCPPString:deviceId]
                                               component:theComponent
                                                portType:[NSString stringWithCPPString:port.type]
                                                  portId:[NSString stringWithCPPString:port.id]
                                                document:theDocument];
                
                deviceIds.erase(foundDeviceId);
            }
            else
                item = [[EmulationItem alloc] initPort:[NSString stringWithCPPString:port.id]
                                                 label:[NSString stringWithCPPString:port.label]
                                             imagePath:[NSString stringWithCPPString:port.image]
                                              portType:[NSString stringWithCPPString:port.type]
                                              document:theDocument];
            
            string group = port.group;
            if (group == "")
                group = "Unknown";
            EmulationItem *groupItem = [self getGroup:[NSString stringWithCPPString:group]];
            NSMutableArray *groupChildren = [groupItem children];
            [groupChildren addObject:item];
            
            [item release];
        }
        
        // Create items not connected on ports
        for (OEIds::iterator i = deviceIds.begin();
             i != deviceIds.end();
             i++)
        {
            string deviceId = *i;
            OEComponent *theComponent = emulation->getComponent(deviceId);
            
            EmulationItem *item;
            item = [[EmulationItem alloc] initDevice:[NSString stringWithCPPString:deviceId]
                                           component:theComponent
                                            portType:@""
                                              portId:@""
                                            document:theDocument];
            
            NSMutableArray *systemGroupChildren = [systemGroupItem children];
            [systemGroupChildren addObject:item];
            
            [item release];
        }
        
        if (![NSThread isMainThread])
            [document unlockEmulation];
    }
    
    return self;
}

- (id)initGroup:(NSString *)theUID
{
    if ((self = [super init]))
    {
        type = EMULATIONITEM_GROUP;
        uid = [theUID copy];
        children = [[NSMutableArray alloc] init];
        
        label = [[NSLocalizedString(theUID, @"Emulation Item Group Label.")
                  uppercaseString] retain];
    }
    
    return self;
}

- (id)initDevice:(NSString *)theUID
       component:(void *)theComponent
        portType:(NSString *)thePortType
          portId:(NSString *)thePortId
        document:(Document *)theDocument
{
    if ((self = [super init]))
    {
        type = EMULATIONITEM_DEVICE;
        uid = [theUID copy];
        children = [[NSMutableArray alloc] init];
        document = theDocument;
        
        device = theComponent;
        
        // Read device values
        string value;
        
        ((OEComponent *)device)->postMessage(NULL, DEVICE_GET_LABEL, &value);
        label = [[NSString stringWithCPPString:value] retain];
        NSString *resourcePath = [[NSUserDefaults standardUserDefaults] URLForKey:@"OEDefaultResourcesPath"].path;
        ((OEComponent *)device)->postMessage(NULL, DEVICE_GET_IMAGEPATH, &value);
        NSString *imagePath = [resourcePath stringByAppendingPathComponent:
                               [NSString stringWithCPPString:value]];
        image = [[NSImage alloc] initByReferencingFile:imagePath];
        
        ((OEComponent *)device)->postMessage(NULL, DEVICE_GET_LOCATIONLABEL, &value);
        locationLabel = [[NSString stringWithCPPString:value] retain];
        ((OEComponent *)device)->postMessage(NULL, DEVICE_GET_STATELABEL, &value);
        stateLabel = [[NSString stringWithCPPString:value] retain];
        
        // Read settings
        DeviceSettings settings;
        ((OEComponent *)device)->postMessage(NULL, DEVICE_GET_SETTINGS, &settings);
        [self initSettings:&settings];
        
        // Read canvases
        canvases = [[NSMutableArray alloc] init];
        OEComponents theCanvases;
        ((OEComponent *)device)->postMessage(NULL, DEVICE_GET_CANVASES, &theCanvases);
        for (int i = 0; i < theCanvases.size(); i++)
            [canvases addObject:[NSValue valueWithPointer:theCanvases.at(i)]];
        
        // Read storages
        storages = [[NSMutableArray alloc] init];
        OEComponents theStorages;
        ((OEComponent *)device)->postMessage(NULL, DEVICE_GET_STORAGES, &theStorages);
        for (int i = 0; i < theStorages.size(); i++)
        {
            OEComponent *theComponent = theStorages.at(i);
            [storages addObject:[NSValue valueWithPointer:theComponent]];
            
            ((OEComponent *)theComponent)->postMessage(NULL, STORAGE_GET_MOUNTPATH, &value);
            if (value.size())
            {
                NSString *storageUID;
                storageUID = [NSString stringWithFormat:@"%@.storage", uid];
                
                EmulationItem *storageItem;
                storageItem = [[EmulationItem alloc] initMount:storageUID
                                                     component:theComponent
                                                 locationLabel:locationLabel
                                                      document:theDocument];
                [children addObject:storageItem];
                [storageItem release];
            }
        }
        
        portType = [thePortType copy];
        portId = [thePortId copy];
    }
    
    return self;
}

- (id)initMount:(NSString *)theUID
      component:(void *)theComponent
  locationLabel:(NSString *)theLocationLabel
       document:(Document *)theDocument
{
    if ((self = [super init]))
    {
        type = EMULATIONITEM_MOUNT;
        uid = [theUID copy];
        children = [[NSMutableArray alloc] init];
        document = theDocument;
        
        string value;
        
        ((OEComponent *)theComponent)->postMessage(NULL, STORAGE_GET_MOUNTPATH, &value);
        label = [[[NSString stringWithCPPString:value] lastPathComponent] retain];
        image = [[NSImage imageNamed:@"DiskImage"] retain];
        
        locationLabel = [theLocationLabel copy];
        value = "";
        ((OEComponent *)theComponent)->postMessage(NULL, STORAGE_GET_FORMATLABEL, &value);
        stateLabel = [[NSString stringWithCPPString:value] retain];
        
        DeviceSettings settings;
        ((OEComponent *)theComponent)->postMessage(NULL, STORAGE_GET_SETTINGS, &settings);
        [self initSettings:&settings];
        
        storages = [[NSMutableArray alloc] init];
        [storages addObject:[NSValue valueWithPointer:theComponent]];
    }
    
    return self;
}

- (id)initPort:(NSString *)theUID
         label:(NSString *)theLabel
     imagePath:(NSString *)theImagePath
      portType:(NSString *)thePortType
      document:(Document *)theDocument;
{
    if ((self = [super init]))
    {
        type = EMULATIONITEM_AVAILABLEPORT;
        uid = [theUID copy];
        children = [[NSMutableArray alloc] init];
        document = theDocument;
        
        if ([theLabel length])
            theLabel = [@" " stringByAppendingString:theLabel];
        
        OEEmulation *emulation = (OEEmulation *)[theDocument emulation];
        string deviceId = OEGetDeviceId([uid cppString]);
        OEComponent *theDevice = emulation->getComponent(deviceId);
        if (theDevice)
        {
            string deviceLabel;
            theDevice->postMessage(NULL, DEVICE_GET_LABEL, &deviceLabel);
            string theLocationLabel;
            theDevice->postMessage(NULL, DEVICE_GET_LOCATIONLABEL, &theLocationLabel);
            if (theLocationLabel == "")
                locationLabel = [[[NSString stringWithCPPString:deviceLabel]stringByAppendingFormat:@"%@",
                                  theLabel] retain];
            else
                locationLabel = [[[NSString stringWithCPPString:theLocationLabel] stringByAppendingFormat:@"%@",
                                  theLabel] retain];
        }
        NSString *resourcePath = [[NSUserDefaults standardUserDefaults] URLForKey:@"OEDefaultResourcesPath"].path;
        NSString *imagePath = [resourcePath stringByAppendingPathComponent:theImagePath];
        image = [[NSImage alloc] initByReferencingFile:imagePath];
        
        label = [thePortType copy];
        stateLabel = @"";
        
        portType = [thePortType copy];
        portId = [theUID copy];
    }
    
    return self;
}

- (void)dealloc
{
    [uid release];
    [children release];
    
    [label release];
    [image release];
    
    [locationLabel release];
    [stateLabel release];
    
    [settingsComponent release];
    [settingsName release];
    [settingsLabel release];
    [settingsType release];
    [settingsOptions release];
    [settingsOptionKeys release];
    
    [canvases release];
    [storages release];
    
    [portType release];
    [portId release];
    
    [super dealloc];
}

- (void)initSettings:(void *)theSettings
{
    settingsComponent = [[NSMutableArray alloc] init];
    settingsName = [[NSMutableArray alloc] init];
    settingsLabel = [[NSMutableArray alloc] init];
    settingsType = [[NSMutableArray alloc] init];
    settingsOptions = [[NSMutableArray alloc] init];
    settingsOptionKeys = [[NSMutableArray alloc] init];
    
    DeviceSettings *settings = (DeviceSettings *)theSettings;
    
    for (int i = 0; i < settings->size(); i++)
    {
        DeviceSetting setting = settings->at(i);
        
        [settingsComponent addObject:[NSValue valueWithPointer:setting.component]];
        [settingsName addObject:[NSString stringWithCPPString:setting.name]];
        [settingsLabel addObject:[NSString stringWithCPPString:setting.label]];
        [settingsType addObject:[NSString stringWithCPPString:setting.type]];
        
        NSArray *optionEntries = [[NSString stringWithCPPString:setting.options]
                                  componentsSeparatedByString:@","];
        
        NSMutableArray *options = [NSMutableArray array];
        NSMutableArray *optionKeys = [NSMutableArray array];
        
        for (int i = 0; i < [optionEntries count]; i++)
        {
            NSString *optionEntry = [optionEntries objectAtIndex:i];
            NSArray *optionComponents = [optionEntry componentsSeparatedByString:@"|"];
            
            NSUInteger lastN = [optionComponents count] - 1;
            [options addObject:[optionComponents objectAtIndex:lastN]];
            [optionKeys addObject:[optionComponents objectAtIndex:0]];
        }
        
        [settingsOptions addObject:options];
        [settingsOptionKeys addObject:optionKeys];
    }
}

- (BOOL)isGroup
{
    return (type == EMULATIONITEM_GROUP);
}

- (NSString *)uid
{
    return [[uid copy] autorelease];
}

- (NSMutableArray *)children
{
    return children;
}

- (NSString *)label
{
    return label;
}

- (NSImage *)image
{
    return image;
}

- (NSString *)locationLabel
{
    return locationLabel;
}

- (NSString *)stateLabel
{
    return stateLabel;
}

- (void *)device
{
    return device;
}

- (NSInteger)numberOfSettings
{
    return [settingsComponent count];
}

- (NSString *)labelForSettingAtIndex:(NSInteger)index
{
    return [settingsLabel objectAtIndex:index];
}

- (NSString *)typeForSettingAtIndex:(NSInteger)index
{
    return [settingsType objectAtIndex:index];
}

- (NSArray *)optionsForSettingAtIndex:(NSInteger)index
{
    return [settingsOptions objectAtIndex:index];
}

- (void)setValue:(NSString *)value forSettingAtIndex:(NSInteger)index;
{
    OEComponent *settingComponent = (OEComponent *) [[settingsComponent objectAtIndex:index]
                                                     pointerValue];
    NSString *settingName = [settingsName objectAtIndex:index];
    NSString *settingType = [settingsType objectAtIndex:index];
    if ([settingType compare:@"select"] == NSOrderedSame)
    {
        NSArray *settingOptionKeys = [settingsOptionKeys objectAtIndex:index];
        value = [settingOptionKeys objectAtIndex:[value integerValue]];
    }
    
    [document lockEmulation];
    
    if (settingComponent &&
        settingComponent->setValue([settingName cppString], [value cppString]))
        settingComponent->update();
    
    [document unlockEmulation];
}

- (NSString *)valueForSettingAtIndex:(NSInteger)index
{
    OEComponent *settingComponent = (OEComponent *) [[settingsComponent objectAtIndex:index]
                                                     pointerValue];
    NSString *settingName = [settingsName objectAtIndex:index];
    NSString *value = @"";
    
    [document lockEmulation];
    
    if (settingComponent)
    {
        string theValue;
        settingComponent->getValue([settingName cppString], theValue);
        value = [NSString stringWithCPPString:theValue];
    }
    
    [document unlockEmulation];
    
    NSString *settingType = [settingsType objectAtIndex:index];
    if ([settingType compare:@"select"] == NSOrderedSame)
    {
        NSArray *optionKeys = [settingsOptionKeys objectAtIndex:index];
        value = [NSString stringWithFormat:@"%d", [optionKeys indexOfObject:value]];
    }
    
    return value;
}

- (BOOL)isRemovable
{
    return (type == EMULATIONITEM_DEVICE) && ([locationLabel length] != 0);
}

- (void)remove
{
    OEEmulation *emulation = (OEEmulation *)[document emulation];
    
    [document lockEmulation];
    
    emulation->removeDevice([uid cppString]);
    
    [document unlockEmulation];
}

- (BOOL)hasCanvases
{
    if (canvases)
        return [canvases count];
    
    return NO;
}

- (void)showCanvases
{
    if (canvases)
        for (int i = 0; i < [canvases count]; i++)
            [document showCanvas:[canvases objectAtIndex:i]];
}

- (BOOL)hasStorages
{
    if ((type == EMULATIONITEM_DEVICE) && [storages count])
        return YES;
    
    return NO;
}

- (BOOL)mount:(NSString *)path
{
    if (!storages)
        return NO;
    
    [document lockEmulation];
    
    string value = [path cppString];
    BOOL success = NO;
    for (int i = 0; i < [storages count]; i++)
    {
        OEComponent *component = (OEComponent *)[[storages objectAtIndex:i]
                                                 pointerValue];
        
        if (component->postMessage(NULL, STORAGE_IS_AVAILABLE, NULL))
        {
            success = component->postMessage(NULL, STORAGE_MOUNT, &value);
            
            if (success)
                break;
        }
    }
    
    if (!success)
    {
        for (int i = 0; i < [storages count]; i++)
        {
            OEComponent *component = (OEComponent *)[[storages objectAtIndex:i]
                                                     pointerValue];
            
            success = component->postMessage(NULL, STORAGE_MOUNT, &value);
            
            if (success)
                break;
        }
    }
    
    [document unlockEmulation];
    
    return success;
}

- (BOOL)canMount:(NSString *)path
{
    if (!storages)
        return NO;
    
    [document lockEmulation];
    
    string value = [path cppString];
    BOOL success = NO;
    for (int i = 0; i < [storages count]; i++)
    {
        OEComponent *component = (OEComponent *)[[storages objectAtIndex:i]
                                                 pointerValue];
        
        success = component->postMessage(NULL, STORAGE_CAN_MOUNT, &value);
        
        if (success)
            break;
    }
    
    [document unlockEmulation];
    
    return success;
}

- (BOOL)isMount
{
    return (type == EMULATIONITEM_MOUNT);
}

- (void)showInFinder
{
    if (!storages)
        return;
    
    string value;
    for (int i = 0; i < [storages count]; i++)
    {
        OEComponent *component = (OEComponent *)[[storages objectAtIndex:i]
                                                 pointerValue];
        
        [document lockEmulation];
        
        component->postMessage(NULL, STORAGE_GET_MOUNTPATH, &value);
        
        [document unlockEmulation];
        
        [[NSWorkspace sharedWorkspace] selectFile:[NSString stringWithCPPString:value]
                         inFileViewerRootedAtPath:@""];
    }
}

- (BOOL)isLocked
{
    if (!storages)
        return NO;
    
    [document lockEmulation];
    
    BOOL success = NO;
    for (int i = 0; i < [storages count]; i++)
    {
        OEComponent *component = (OEComponent *)[[storages objectAtIndex:i]
                                                 pointerValue];
        
        success |= component->postMessage(NULL, STORAGE_IS_LOCKED, NULL);
    }
    
    [document unlockEmulation];
    
    return success;
}

- (BOOL)unmount
{
    bool success = true;
    
    if (!storages)
        return success;
    
    [document lockEmulation];
    
    for (int i = 0; i < [storages count]; i++)
    {
        OEComponent *component = (OEComponent *)[[storages objectAtIndex:i]
                                                 pointerValue];
        
        if (!component->postMessage(NULL, STORAGE_UNMOUNT, NULL))
            success = false;
    }
    
    [document unlockEmulation];
    
    return success;
}

- (BOOL)isPort
{
    return (type == EMULATIONITEM_AVAILABLEPORT);
}

- (BOOL)addOEDocument:(NSString *)thePath
{
    OEDocument oeDocument;
    oeDocument.open([thePath cppString]);
    if (!oeDocument.isOpen())
        return NO;
    
    OEConnectorInfos connectorInfos = oeDocument.getFreeConnectorInfos();
    
    oeDocument.close();
    
    if (connectorInfos.size() == 1)
    {
        OEConnectorInfos::iterator i = connectorInfos.begin();
        
        map<string, string> idMap;
        idMap[[portId cppString]] = i->id;
        
        [document captureNewCanvases:YES];
        
        OEEmulation *emulation = (OEEmulation *)[document emulation];
        
        [document lockEmulation];
        
        if (type == EMULATIONITEM_DEVICE)
            emulation->removeDevice([uid cppString]);
        
        bool result = emulation->addDocument([thePath cppString], idMap);
        
        [document unlockEmulation];
        
        [document showNewCanvases];
        
        [document captureNewCanvases:NO];
        
        return result;
    }
    
    return NO;
}

- (NSString *)portType
{
    return portType;
}

@end
