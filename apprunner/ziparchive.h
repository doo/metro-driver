#pragma once

#include <iostream>
#include <string>
#include <ppltasks.h>

namespace doo {
  namespace zip {
    // the main archive class
    class ZipArchive {
    public:
      ZipArchive(const std::string& filename);
      std::vector<byte> GetFileContentsAsync(const std::string& filename);

    private:
#pragma pack(1)
      struct EndOfCentralDirectoryRecord {
        uint32 signature;
        uint16 diskNumber;
        uint16 directoryDiskNumber;
        uint16 entryCountThisDisk;
        uint16 entryCountTotal;
        uint32 centralDirectorySize;
        uint32 centralDirectoryOffset;
        uint16 zipFileCommentLength;
      };

      struct Zip64EndOfCentralDirectoryRecord {
        uint32 signature;
        uint64 recordSize;
        uint16 versionMadeBy;
        uint16 versionToExtract;
        uint32 diskNumber;
        uint32 centralDirectoryDiskNumber;
        uint64 entryCountThisDisk;
        uint64 totalEntryCount;
        uint64 centralDirectorySize;
        uint64 startingDiskCentralDirectoryOffset;
      };

      struct Zip64EndOfCentralDirectoryRecordLocator {
        uint32 signature;
        uint32 centralDirectoryStartDiskNumber;
        uint64 centralDirectoryOffset;
        uint32 numberOfDisks;
      };
#pragma pack()

      class ZipArchiveEntry {
      public:
        // ctor which takes an input stream that's already positioned at
        // the start of the central directory header for a new archive entry
        ZipArchiveEntry(std::ifstream& centralDirectoryHeaderStream);

        std::vector<byte> GetUncompressedFileContents();
        std::string filename;

      private:
      
  #pragma pack(1)
        struct LocalFileHeader {
          uint32 signature;
          uint16 version;
          uint16 flags;
          uint16 compressionMethod;
          uint16 lastModifiedTime;
          uint16 lastModifiedDate;
          uint32 crc32;
          uint32 compressedSize;
          uint32 uncompressedSize;
          uint16 filenameLength;
          uint16 extraFieldLength;
        } localHeader;

        struct CentralDirectoryHeader {
          uint32 signature;
          uint16 versionCreated;
          uint16 versionNeeded;
          uint16 flags;
          uint16 compressionMethod;
          uint16 lastModifiedTime;
          uint16 lastModifiedDate;
          uint32 crc32;
          uint32 compressedSize;
          uint32 uncompressedSize;
          uint16 filenameLength;
          uint16 extraFieldLength;
          uint16 fileCommentLength;
          uint16 diskNumberStart;
          uint16 internalFileAttributes;
          uint32 externalFileAttributes;
          uint32 localHeaderOffset;
        } centralDirectoryHeader;
  #pragma pack()

        std::ifstream& inputStream;
        std::string extraField;
        DWORD64 contentStreamStart;

        void ReadAndCheckLocalHeader();
        std::vector<byte> DeflateFromStream();
        std::vector<byte> UncompressedFromStream();
      };

      std::vector<std::shared_ptr<ZipArchiveEntry>> archiveEntries;
      std::ifstream fileStream;
    };
  }
}
