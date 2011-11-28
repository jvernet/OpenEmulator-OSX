
/**
 * libemulator
 * Apple II Video
 * (C) 2010-2011 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Generates Apple II video
 */

#include <math.h>

#include "AppleIIVideo.h"

#include "DeviceInterface.h"
#include "MemoryInterface.h"
#include "AppleIIInterface.h"

#define SCREEN_WIDTH        768
#define SCREEN_ORIGIN_X     104
#define NTSC_ORIGIN_Y       25
#define NTSC_HEIGHT         242
#define PAL_ORIGIN_Y        35
#define PAL_HEIGHT          312

#define VIDEO_WIDTH         40
#define VIDEO_HEIGHT        192
#define TERM_WIDTH          40
#define TERM_HEIGHT         24
#define BLOCK_WIDTH         14
#define BLOCK_HEIGHT        8

#define MAP_SIZE           0x100
#define MAP_SIZE_MASK      0xff
#define MAP_WIDTH          16
#define MAP_HEIGHT         8
#define FLASH_HALFCYCLE     14

AppleIIVideo::AppleIIVideo()
{
    device = NULL;
    controlBus = NULL;
    floatingBus = NULL;
    monitorDevice = NULL;
    monitor = NULL;
    
    rev0 = false;
    palTiming = false;
    characterSet = "Standard";
    
    for (int i = 0; i < 2; i++)
        textMemory[i] = NULL;
    for (int i = 0; i < 2; i++)
        hiresMemory[i] = NULL;
    canvasShouldUpdate = true;
    
    mode = 0;
    
    image.setFormat(OEIMAGE_LUMINANCE);
    vector<float> colorBurst;
    colorBurst.push_back(M_PI * -33.0 / 180.0);
    image.setColorBurst(colorBurst);
    
    flash = false;
    flashCount = 0;
    
    vsyncTimer = false;
    vsyncCycleCount = 0;
    powerState = CONTROLBUS_POWERSTATE_ON;
}

bool AppleIIVideo::setValue(string name, string value)
{
	if (name == "rev")
        rev0 = (value == "Revision 0");
	else if (name == "tvSystem")
		palTiming = (value == "PAL");
	else if (name == "characterSet")
		characterSet = value;
	else if (name == "text")
        OESetBit(mode, APPLEIIVIDEO_TEXT, getInt(value));
	else if (name == "mixed")
        OESetBit(mode, APPLEIIVIDEO_MIXED, getInt(value));
	else if (name == "page2")
        OESetBit(mode, APPLEIIVIDEO_PAGE2, getInt(value));
	else if (name == "hires")
        OESetBit(mode, APPLEIIVIDEO_HIRES, getInt(value));
	else
		return false;
	
	return true;
}

bool AppleIIVideo::getValue(string name, string& value)
{
    if (name == "rev")
        value = rev0 ? "Revision 0" : "Revision 1";
	else if (name == "tvSystem")
		value = palTiming ? "PAL" : "NTSC";
	else if (name == "characterSet")
		value = characterSet;
	else if (name == "text")
		value = getString(OEGetBit(mode, APPLEIIVIDEO_TEXT));
	else if (name == "mixed")
		value = getString(OEGetBit(mode, APPLEIIVIDEO_MIXED));
	else if (name == "page2")
		value = getString(OEGetBit(mode, APPLEIIVIDEO_PAGE2));
	else if (name == "hires")
		value = getString(OEGetBit(mode, APPLEIIVIDEO_HIRES));
	else
		return false;
	
	return true;
}

bool AppleIIVideo::setRef(string name, OEComponent *ref)
{
    if (name == "device")
        device = ref;
    else if (name == "controlBus")
    {
        if (controlBus)
        {
            controlBus->removeObserver(this, CONTROLBUS_POWERSTATE_DID_CHANGE);
            controlBus->removeObserver(this, CONTROLBUS_TIMER_DID_FIRE);
        }
        controlBus = ref;
        if (controlBus)
        {
            controlBus->addObserver(this, CONTROLBUS_POWERSTATE_DID_CHANGE);
            controlBus->addObserver(this, CONTROLBUS_TIMER_DID_FIRE);
        }
    }
	else if (name == "floatingBus")
		floatingBus = ref;
    else if ((name == "ram1") || (name == "ram"))
        ram1 = ref;
    else if (name == "ram2")
        ram2 = ref;
    else if (name == "ram3")
        ram3 = ref;
    else if (name == "monitorDevice")
    {
        if (monitorDevice)
            monitorDevice->removeObserver(this, DEVICE_DID_CHANGE);
        monitorDevice = ref;
        if (monitorDevice)
            monitorDevice->addObserver(this, DEVICE_DID_CHANGE);
    }
	else if (name == "monitor")
    {
        if (monitor)
        {
            monitor->removeObserver(this, CANVAS_MOUSE_DID_CHANGE);
            monitor->removeObserver(this, CANVAS_DID_COPY);
        }
        monitor = ref;
        if (monitor)
        {
            monitor->addObserver(this, CANVAS_MOUSE_DID_CHANGE);
            monitor->addObserver(this, CANVAS_DID_COPY);
        }
    }
    else
		return false;
	
	return true;
}

bool AppleIIVideo::setData(string name, OEData *data)
{
    if (name.substr(0, 4) == "font")
        loadTextMap(name.substr(4), data);
    else
        return false;
    
    return true;
}

bool AppleIIVideo::init()
{
    if (!device)
    {
        logMessage("device not connected");
        
        return false;
    }
    
    if (!controlBus)
    {
        logMessage("controlBus not connected");
        
        return false;
    }
    
    if (!floatingBus)
    {
        logMessage("floatingBus not connected");
        
        return false;
    }
    
    image.setSampleRate(14318180);
    
    updateTiming();
    
    oldPALTiming = palTiming;
    
    OEAddress startAddress = 0;
    getVRAM(ram1, startAddress);
    getVRAM(ram2, startAddress);
    getVRAM(ram3, startAddress);
    
    initOffsets();
    
    initLoresMap();
    
    initHCountMap();
    
    controlBus->postMessage(this, CONTROLBUS_GET_POWERSTATE, &powerState);
    
    scheduleTimer();
    
    return true;
}

void AppleIIVideo::update()
{
    updateMode();
    
    if (palTiming != oldPALTiming)
    {
        updateTiming();
        
        oldPALTiming = palTiming;
    }
    
    float clockFrequency = palTiming ? (14318180.0 * 65 / 912) : (14250000.0 * 65 / 912);
    
    controlBus->postMessage(this, CONTROLBUS_SET_CLOCKFREQUENCY, &clockFrequency);
    
    initHiresMap();
    
    if (monitor)
    {
        CanvasCaptureMode captureMode;
        captureMode = CANVAS_CAPTUREMODE_CAPTURE_ON_MOUSE_CLICK;
        monitor->postMessage(this, CANVAS_SET_CAPTUREMODE, &captureMode);
    }
    
    canvasShouldUpdate = true;
}

bool AppleIIVideo::postMessage(OEComponent *sender, int message, void *data)
{
    switch (message)
    {
        case APPLEIIVIDEO_GET_MODE:
            *((OEUInt32 *)data) = mode;
            
            return true;
            
        case APPLEIIVIDEO_UPDATE:
            updateVideo();
            
            return true;
            
        case APPLEIIVIDEO_READ_FLOATINGBUS:
            *((OEUInt8 *)data) = readFloatingBus();
            
            return true;
    }
    
    return false;
}

void AppleIIVideo::notify(OEComponent *sender, int notification, void *data)
{
    if (sender == controlBus)
    {
        switch (notification)
        {
            case CONTROLBUS_POWERSTATE_DID_CHANGE:
                powerState = *((ControlBusPowerState *)data);
                
                break;
                
            case CONTROLBUS_TIMER_DID_FIRE:
                if (vsyncTimer)
                    vsync();
                
                scheduleTimer();
                
                break;
        }
    }
    else if (sender == monitorDevice)
        device->postNotification(sender, notification, data);
    else if (sender == monitor)
    {
        switch (notification)
        {
            case CANVAS_MOUSE_DID_CHANGE:
                postNotification(this, notification, data);
                
                break;
                
            case CANVAS_DID_COPY:
                copy((wstring *)data);
                
                break;
        }
    }
}

OEUInt8 AppleIIVideo::read(OEAddress address)
{
    write(address, 0);
    
	return floatingBus->read(address);
}

void AppleIIVideo::write(OEAddress address, OEUInt8 value)
{
    switch (address & 0x7f)
    {
        case 0x50: case 0x51:
            setMode(APPLEIIVIDEO_TEXT, address & 0x1);
            
            break;
            
        case 0x52: case 0x53:
            setMode(APPLEIIVIDEO_MIXED, address & 0x1);
            
            break;
            
        case 0x54: case 0x55:
            setMode(APPLEIIVIDEO_PAGE2, address & 0x1);
            
            break;
            
        case 0x56: case 0x57:
            setMode(APPLEIIVIDEO_HIRES, address & 0x1);
            
            break;
    }
}

void AppleIIVideo::getVRAM(OEComponent *ram, OEAddress& startAddress)
{
    // Notes:
    // * This requires memory to be mapped contiguously
    // * For hires to work, memory at $2000 and $4000 should be in a single block
    
    if (!ram)
        return;
    
    OEData *data;
    ram->postMessage(this, RAM_GET_DATA, &data);
    
    OEAddress endAddress = startAddress + data->size() - 1;
    
    if ((startAddress <= 0x400) && (endAddress >= 0x7ff))
        textMemory[0] = &data->front() + 0x400 - startAddress;
    if ((startAddress <= 0x800) && (endAddress >= 0xbff))
        textMemory[1] = &data->front() + 0x800 - startAddress;
    if ((startAddress <= 0x2000) && (endAddress >= 0x3fff))
        hiresMemory[0] = &data->front() + 0x2000 - startAddress;
    if ((startAddress <= 0x4000) && (endAddress >= 0x5fff))
        hiresMemory[1] = &data->front() + 0x4000 - startAddress;
    
    startAddress += data->size();
}

void AppleIIVideo::scheduleTimer()
{
    OEUInt64 clocks;
    
    if (!vsyncTimer)
    {
        if (OEGetBit(mode, APPLEIIVIDEO_TEXT))
            renderer = APPLEIIVIDEO_RENDERER_TEXT;
        else if (!OEGetBit(mode, APPLEIIVIDEO_HIRES))
            renderer = APPLEIIVIDEO_RENDERER_LORES;
        else
            renderer = APPLEIIVIDEO_RENDERER_HIRES;
        
        clocks = ((palTiming ? 312 : 262) - 64 - 32) * 65;
        
        updateVideo();
    }
    else
    {
        if (OEGetBit(mode, APPLEIIVIDEO_TEXT) ||
            OEGetBit(mode, APPLEIIVIDEO_MIXED))
            renderer = APPLEIIVIDEO_RENDERER_TEXT;
        else if (!OEGetBit(mode, APPLEIIVIDEO_HIRES))
            renderer = APPLEIIVIDEO_RENDERER_LORES;
        else
            renderer = APPLEIIVIDEO_RENDERER_HIRES;
        
        clocks = (64 + 32) * 65;
        
        updateVideo();
    }
    
    controlBus->postMessage(this, CONTROLBUS_SCHEDULE_TIMER, &clocks);
    
    vsyncTimer = !vsyncTimer;
}

void AppleIIVideo::initOffsets()
{
    textOffset.resize(TERM_HEIGHT);
    
    for (int y = 0; y < TERM_HEIGHT; y++)
        textOffset[y] = (y >> 3) * TERM_WIDTH + (y & 0x7) * 0x80;
    
    hiresOffset.resize(TERM_HEIGHT * BLOCK_HEIGHT);
    
    for (int y = 0; y < TERM_HEIGHT * BLOCK_HEIGHT; y++)
        hiresOffset[y] = (y >> 6) * TERM_WIDTH + ((y >> 3) & 0x7) * 0x80 + (y & 0x7) * 0x400;
}

void AppleIIVideo::loadTextMap(string name, OEData *data)
{
    if (data->size() < MAP_HEIGHT)
        return;
    
    OEData theMap;
    
    theMap.resize(2 * MAP_SIZE * MAP_HEIGHT * MAP_WIDTH);
    
    int cMask = (int) getNextPowerOf2(data->size() / MAP_HEIGHT) - 1;
    
    for (int c = 0; c < 2 * MAP_SIZE; c++)
    {
        for (int y = 0; y < MAP_HEIGHT; y++)
        {
            OEUInt8 byte = data->at((c & cMask) * MAP_HEIGHT + y);
            
            bool inv = !(OEGetBit(c, 0x80) || (OEGetBit(byte, 0x80) && OEGetBit(c, 0x100)));
            
            for (int x = 0; x < MAP_WIDTH; x++)
            {
                bool b = (byte << (x >> 1)) & 0x40;
                
                theMap[(c * MAP_HEIGHT + y) * MAP_WIDTH + x] = (b ^ inv) ? 0xff : 0x00;
            }
        }
    }
    
    textMap[name] = theMap;
}

void AppleIIVideo::initLoresMap()
{
    loresMap.resize(2 * MAP_SIZE * MAP_HEIGHT * MAP_WIDTH);
    
    for (int c = 0; c < 2 * MAP_SIZE; c++)
    {
        for (int y = 0; y < MAP_HEIGHT; y++)
        {
            OEUInt32 v = (y < 4) ? c & 0xf : (c >> 4) & 0xf;
            
            OEUInt32 bitmap = (v << 12) | (v << 8) | (v << 4) | v;
            bitmap >>= OEGetBit(c, 0x100) ? 2 : 0;
            
            for (int x = 0; x < MAP_WIDTH; x++)
            {
                loresMap[(c * MAP_HEIGHT + y) * MAP_WIDTH + x] = (bitmap & 0x1) ? 0xff : 0x00;
                
                bitmap >>= 1;
            }
        }
    }
}

void AppleIIVideo::initHiresMap()
{
    hiresMap.resize(2 * MAP_SIZE * MAP_WIDTH);
    
    for (int c = 0; c < 2 * MAP_SIZE; c++)
    {
        OEUInt8 byte = (c & 0x7f) << 1 | (c >> 8);
        bool delay = !rev0 && (c & 0x80);
        
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            bool b = (byte >> ((x + 2 - delay) >> 1)) & 0x1;
            
            hiresMap[c * MAP_WIDTH + x] = b ? 0xff : 0x00;
        }
    }
}

void AppleIIVideo::initHCountMap()
{
    hCountMap[0] = 0;
    
    for (int i = 0; i < 64; i++)
        hCountMap[i + 1] = 64 + i;
}

void AppleIIVideo::updateTiming()
{
    image.setSize(OEMakeSize(SCREEN_WIDTH, palTiming ? PAL_HEIGHT : NTSC_HEIGHT));
    
    imageOrigin = (palTiming ? (PAL_ORIGIN_Y * SCREEN_WIDTH + SCREEN_ORIGIN_X) :
                   (NTSC_ORIGIN_Y * SCREEN_WIDTH + SCREEN_ORIGIN_X));
    vCountStart = (palTiming ? 0xc8 : 0xfa);
}

void AppleIIVideo::updateMode()
{
    if (rev0 || !OEGetBit(mode, APPLEIIVIDEO_TEXT))
        image.setSubcarrier(3579545);
    else
        image.setSubcarrier(0);
}

void AppleIIVideo::setMode(OEUInt32 mask, bool value)
{
    OEUInt32 oldMode = mode;
    
    OESetBit(mode, mask, value);
    
    if (mode != oldMode)
    {
        updateMode();
        
        postNotification(this, APPLEIIVIDEO_MODE_DID_CHANGE, &mode);
        
        updateVideo();
    }
}

AppleIIVideoCount AppleIIVideo::getCount()
{
    OEUInt64 cycleCount;
    
    controlBus->postMessage(this, CONTROLBUS_GET_CYCLECOUNT, &cycleCount);
    
    cycleCount -= vsyncCycleCount;
    
    AppleIIVideoCount count;
    
    count.h = (OEUInt32) cycleCount % 65;
    count.v = (vCountStart + cycleCount / 65) & 0x1ff;
    
    return count;
}

OEUInt8 AppleIIVideo::readFloatingBus()
{
    AppleIIVideoCount count = getCount();
    
	bool isMixed = OEGetBit(mode, APPLEIIVIDEO_MIXED) && ((count.v & 0xa0) == 0xa0);
	bool isHires = OEGetBit(mode, APPLEIIVIDEO_HIRES) && !(OEGetBit(mode, APPLEIIVIDEO_TEXT) || isMixed);
	bool isPage2 = OEGetBit(mode, APPLEIIVIDEO_PAGE2);
	
	OEUInt32 address = count.h & 0x7;
	OEUInt32 h5h4h3 = count.h & 0x38;
	
    OEUInt32 v4v3 = count.v & 0xc0;
	OEUInt32 v4v3v4v3 = (v4v3 >> 3) | (v4v3 >> 1);
	
    address |= ((0x68 + h5h4h3 + v4v3v4v3) & 0x78);
	address |= (count.v & 0x38) << 4;
    
	if (isHires)
    {
		address |= (count.v & 0x7) << 10;
        
        return hiresMemory[isPage2][address];
	}
    else
        return textMemory[isPage2][address];
}

void AppleIIVideo::vsync()
{
    if (!monitor || (powerState == CONTROLBUS_POWERSTATE_OFF))
        return;
    
    controlBus->postMessage(this, CONTROLBUS_GET_CYCLECOUNT, &vsyncCycleCount);
    
    if (powerState == CONTROLBUS_POWERSTATE_ON)
    {
        if (flashCount)
            flashCount--;
        else
        {
            flash = !flash;
            flashCount = FLASH_HALFCYCLE;
            
            canvasShouldUpdate = true;
        }
    }
    
//    if (!canvasShouldUpdate)
  //      return;
    
    monitor->postMessage(this, CANVAS_POST_IMAGE, &image);
    
    canvasShouldUpdate = false;
}

void AppleIIVideo::updateVideo()
{
    switch (renderer)
    {
        case APPLEIIVIDEO_RENDERER_TEXT:
            drawText();
            
            break;
            
        case APPLEIIVIDEO_RENDERER_LORES:
            drawLores();
            
            break;
            
        case APPLEIIVIDEO_RENDERER_HIRES:
            drawHires();
            
            break;
    }
}

// Copy a 14-pixel char scanline
#define copyBlockLine(x) \
*((OEUInt64 *)(p + x * SCREEN_WIDTH + 0)) = *((OEUInt64 *)(m + x * MAP_WIDTH + 0));\
*((OEUInt32 *)(p + x * SCREEN_WIDTH + 8)) = *((OEUInt32 *)(m + x * MAP_WIDTH + 8));\
*((OEUInt16 *)(p + x * SCREEN_WIDTH + 12)) = *((OEUInt16 *)(m + x * MAP_WIDTH + 12));

void AppleIIVideo::drawText()
{
    AppleIIVideoCount count = getCount();
    
    OEUInt32 page = OEGetBit(mode, APPLEIIVIDEO_PAGE2) ? 1 : 0;
    
    OEUInt8 *vp = textMemory[page];
    OEUInt8 *mp = (OEUInt8 *)&textMap[characterSet].front();
    OEUInt8 *ip = (OEUInt8 *)image.getPixels();
    
    if (!vp || !mp || !ip)
        return;
    
    if (flash)
        mp += MAP_SIZE * MAP_WIDTH * MAP_HEIGHT;
    
    for (int y = 0; y < TERM_HEIGHT; y++)
    {
        OEUInt8 *p = ip + imageOrigin + y * SCREEN_WIDTH * BLOCK_HEIGHT;
        
        for (int x = 0; x < TERM_WIDTH; x++)
        {
            OEUInt8 c = vp[textOffset[y] + x];
            OEUInt8 *m = mp + c * MAP_HEIGHT * MAP_WIDTH;
            
            copyBlockLine(0);
            copyBlockLine(1);
            copyBlockLine(2);
            copyBlockLine(3);
            copyBlockLine(4);
            copyBlockLine(5);
            copyBlockLine(6);
            copyBlockLine(7);
            
            p += BLOCK_WIDTH;
        }
    }
    
    lastCount = count;
}

void AppleIIVideo::drawLores()
{
    AppleIIVideoCount count = getCount();
    
    OEUInt32 page = OEGetBit(mode, APPLEIIVIDEO_PAGE2) ? 1 : 0;
    
    OEUInt8 *vp = textMemory[page];
    OEUInt8 *mp = (OEUInt8 *)&loresMap.front();
    OEUInt8 *ip = (OEUInt8 *)image.getPixels();
    
    if (!vp || !mp || !ip)
        return;
    
    for (int y = 0; y < TERM_HEIGHT; y++)
    {
        OEUInt8 *p = ip + imageOrigin + y * SCREEN_WIDTH * BLOCK_HEIGHT;
        
        for (int x = 0; x < TERM_WIDTH; x++)
        {
            OEUInt8 c = vp[textOffset[y] + x];
            OEUInt8 *m = mp + c * MAP_HEIGHT * MAP_WIDTH + (x & 1) * MAP_SIZE * MAP_WIDTH * MAP_HEIGHT;
            
            copyBlockLine(0);
            copyBlockLine(1);
            copyBlockLine(2);
            copyBlockLine(3);
            copyBlockLine(4);
            copyBlockLine(5);
            copyBlockLine(6);
            copyBlockLine(7);
            
            p += BLOCK_WIDTH;
        }
    }
    
    lastCount = count;
}

void AppleIIVideo::drawHires()
{
    AppleIIVideoCount count = getCount();
    
    OEUInt32 page = OEGetBit(mode, APPLEIIVIDEO_PAGE2) ? 1 : 0;
    
    OEUInt8 *vp = hiresMemory[page];
    OEUInt8 *mp = (OEUInt8 *)&hiresMap.front();
    OEUInt8 *ip = (OEUInt8 *)image.getPixels();
    
    if (!vp || !mp || !ip)
        return;
    
    OEUInt8 lastC = 0;
    
    for (int y = 0; y < VIDEO_HEIGHT; y++)
    {
        OEUInt8 *p = ip + imageOrigin + y * SCREEN_WIDTH;
        
        for (int x = 0; x < VIDEO_WIDTH; x++)
        {
            OEUInt32 c = vp[hiresOffset[y] + x] | ((lastC & 0x40) << 2);
            OEUInt8 *m = mp + c * MAP_WIDTH;
            copyBlockLine(0);
            
            p += BLOCK_WIDTH;
            
            lastC = c;
        }
    }
    
    lastCount = count;
}

void AppleIIVideo::copy(wstring *s)
{
    if (!OEGetBit(mode, APPLEIIVIDEO_TEXT))
        return;
    
    OEUInt8 charMap[] = {0x40, 0x20, 0x40, 0x20, 0x40, 0x20, 0x40, 0x60};
    
    OEUInt32 page = OEGetBit(mode, APPLEIIVIDEO_PAGE2) ? 1 : 0;
    
    OEUInt8 *vp = textMemory[page];
    
    if (!vp)
        return;
    
    for (int y = 0; y < TERM_HEIGHT; y++)
    {
        wstring line;
        
        for (int x = 0; x < TERM_WIDTH; x++)
        {
            OEUInt8 value = vp[textOffset[y] + x];
            
            // To-Do: Mousetext
            line += charMap[value >> 5] | (value & 0x1f);
        }
        
        line = rtrim(line);
        line += '\n';
        
        *s += line;
    }
}