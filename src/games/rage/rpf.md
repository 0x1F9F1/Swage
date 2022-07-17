# RPF Archive

RPF (RAGE PackFile) is the archive format used by the RAGE game engine.
So far, there have been 9 iterations of the format, though only 7 have been used publicly.

Version | Usage
--- | ---
RPF0 | Rockstar Games Presents Table Tennis
RPF1 | Unused
RPF2 | Grand Theft Auto IV
RPF3 | Grand Theft Auto IV Audio, Midnight Club: Los Angeles
RPF4 | Max Payne 3
RPF5 | Unused
RPF6 | Red Dead Redemption
RPF7 | Grand Theft Auto V
RPF8 | Red Dead Redemption 2

A packfile typically consists of the following structures:
* A header, starting with the 32-bit identifier `0x3?465052` or `0x5250463?` (where `?` is the version). Decoded as `"RPF?"` or `"?FPR"`.
* A table of contents, stored as an array of entries, representing a directory tree or sorted list.<br/>
  Some versions store the name of each file, while others hash them.<br/>
  Each entry represents either: a directory, a regular "binary" file, or a resource.
* A table of file names, though this is usally not available with hashed archives.

Later versions generally encrypt the TOC and name table, and the endianness may vary depending on the platform.

For a better understanding of a particular version, refer to its corresponding cpp file.

## RPF0

```cpp
struct fiPackHeader0 // 12 bytes
{
    u32 Magic;
    u32 HeaderSize;
    u32 EntryCount;
};

struct fiPackEntry0 // 16 bytes
{
    // NameOffset:31
    // IsDirectory:1
    u32 dword0;

    // Offset (Binary)
    // EntryIndex (Directory)
    u32 dword4;

    // OnDiskSize (Binary)
    // EntryCount (Directory)
    u32 dword8;

    // Size (Binary)
    // EntryCount (Directory)
    u32 dwordC;
};
```

## RPF1 (Unused)

Conflicting evidence suggest it was same as RPF0, but hashed.

## RPF2

```cpp
struct fiPackHeader2 // 20 or 24 bytes
{
    u32 Magic;
    u32 HeaderSize;
    u32 EntryCount;

    u32 dwordC;
    u32 HeaderDecryptionTag;

    // Only in GTA IV, used in game.rpf to signal all files are encrypted
    // u32 FileDecryptionTag;
};

struct fiPackEntry2 // 16 bytes
{
    // NameOffset/NameHash
    u32 dword0;

    // OnDiskSize (Resource)
    // Size (Binary)
    u32 dword4;

    // ResourceVersion:8 (Resource)
    // Offset:23 (Resource)
    // Offset:31 (Binary)
    // EntryIndex:31 (Directory)
    // IsDirectory:1
    u32 dword8;

    // OnDiskSize:30 (Binary)
    // EntryCount:30 (Directory)
    // ResourceFlags:30 (Resource)
    // IsCompressed:1
    // IsResource:1
    u32 dwordC;
};
```

## RPF3

Same as RPF2, but hashed.

## RPF4

Same as RPF2, but with larger file offsets

## RPF5 (Unused)

Same as RPF2, but hashed and with larger file offsets.

## RPF6

```cpp
struct fiPackHeader6 // 16 bytes
{
    u32 Magic;
    u32 EntryCount;
    u32 NamesOffset;
    u32 DecryptionTag;
};

struct fiPackEntry6 // 20 bytes
{
    // NameHash
    u32 dword0;

    // OnDiskSize
    u32 dword4;

    // ResourceVersion:8 (Resource)
    // Offset:23 (Resource)
    // Offset:31 (Binary)
    // EntryIndex:31 (Directory)
    // IsDirectory:1
    u32 dword8;

    // Size:30 (Binary)
    // EntryCount:30 (Directory)
    // ResourceFlags:30 (Resource)
    // IsCompressed:1
    // IsResource:1
    u32 dwordC;

    // VirtualFlags:14 (Resource)
    // PhysicalFlags:14 (Resource)
    // MainChunkOffset:3 (Resource)
    // HasExtendedFlags:1 (Resource)
    u32 dword10;
};
```

## RPF7

```cpp
struct fiPackHeader7 // 16 bytes
{
    u32 Magic;
    u32 EntryCount;

    // NamesLength:28
    // NameShift:3
    // PlatformBit:1
    u32 dwordC;
    u32 DecryptionTag;
};

struct fiPackEntry7
{
    // NameOffset:16
    // OnDiskSize:24
    // Offset:23
    // IsResource:1
    u64 qword0;

    // Size (Binary)
    // VirtualFlags (Resource)
    // EntryIndex (Directory)
    u32 dword8;

    // DecryptionTag (Binary)
    // PhysicalFlags (Resource)
    // EntryCount (Directory)
    u32 dwordC;
};
```

## RPF8

```cpp
struct fiPackHeader8 // 16 bytes
{
    u32 Magic;
    u32 EntryCount;
    u32 NamesLength;
    u16 DecryptionTag;
    u16 PlatformId;
};

struct fiPackEntry8 // 24 bytes
{
    // FileHash:32
    // EncryptionConfig:8
    // EncryptionKeyId:8
    // FileExtId:8
    // IsResource:1
    // IsSignatureProtected:1
    u64 qword0;

    // OnDiskSize:28
    // Offset:31
    // CompresorId:5
    u64 qword8;

    // rage::datResourceInfo
    // ResIdUpper:4
    // Size1:28
    // ResIdLower:4
    // Size2:28
    u64 qword10;
};
```
