
/**
 * libemulation
 * Common types and functions
 * (C) 2011-2012 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Common emulation types and functions
 */

#ifndef _OECOMMON_H
#define _OECOMMON_H

#include <string>
#include <vector>
#include <map>
#include <iostream>

using namespace std;

#define OEAssertBit(x,m) ((x)|=(m))
#define OEClearBit(x,m) ((x)&=~(m))
#define OEToggleBit(x,m) ((x)^=(m))

#define OESetBit(x,m,v) if (v) OEAssertBit(x,m); else OEClearBit(x,m)
#define OEGetBit(x,m) ((x)&(m))

typedef unsigned char OEUInt8;
typedef signed char OEInt8;
typedef unsigned short int OEUInt16;
typedef signed short int OEInt16;
typedef unsigned int OEUInt32;
typedef signed int OEInt32;
typedef unsigned long long OEUInt64;
typedef signed long long OEInt64;

typedef OEUInt64 OEAddress;

typedef union
{
#ifdef BYTES_BIG_ENDIAN
    struct { OEUInt8 h7, h6, h5, h4, h3, h2, h, l; } b;
    struct { OEInt8 h7, h6, h5, h4, h3, h2, h, l; } sb;
    struct { OEUInt16 h3, h2, h, l; } w;
    struct { OEInt8 h3, h2, h, l; } sw;
    struct { OEUInt32 l, h; } d;
    struct { OEInt32 l, h; } sd;
#else
    struct { OEUInt8 l, h, h2, h3, h4, h5, h6, h7; } b;
    struct { OEInt8 l, h, h2, h3, h4, h5, h6, h7; } sb;
    struct { OEUInt16 l, h, h2, h3; } w;
    struct { OEInt16 l, h, h2, h3; } sw;
    struct { OEUInt32 l, h; } d;
    struct { OEInt32 l, h; } sd;
#endif
    OEUInt64 q;
    OEInt64 qd;
} OEUnion;

typedef vector<OEUInt8> OEData;

#ifdef _WIN32
#define OE_PATH_SEPARATOR "\\"
#else
#define OE_PATH_SEPARATOR "/"
#endif

void logMessage(string message);

OEUInt64 getUInt(const string& value);
OEInt64 getInt(const string& value);
double getFloat(const string& value);
OEData getCharVector(const string& value);

string getString(OEUInt32 value);
string getString(OEInt32 value);
string getString(OEUInt64 value);
string getString(OEInt64 value);
string getString(float value);
string getHexString(OEUInt64 value);

string rtrim(string value);
wstring rtrim(wstring value);
vector<string> strsplit(string value, char c);
string strjoin(vector<string>& value, char c);
string strfilter(string value, string filter);
string strtolower(string value);
string strtoupper(string value);

OEUInt64 getNextPowerOf2(OEUInt64 value);

bool readFile(string path, OEData *data);
bool writeFile(string path, OEData *data);
string getPathExtension(string path);

#endif
