
/**
 * libemulator
 * Apple II Audio Input
 * (C) 2010 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Controls Apple II audio input.
 */

#include "AppleIIAudioIn.h"

bool AppleIIAudioIn::setRef(string name, OEComponent *id)
{
	if (name == "sampleConverter")
		sampleConverter = id;
	else if (name == "floatingBus")
		floatingBus = id;
	else
		return false;
	
	return true;
}

OEUInt8 AppleIIAudioIn::read(OEAddress address)
{
	return (floatingBus->read(0) & 0x7f);
}
