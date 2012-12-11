#include "stdafx.h"

#include <fstream>
#include <streambuf>

#include "tinfl.c"

#include "ziparchive.h"

using namespace doo::zip;

// the expected signatures for different parts of a ZIP file
#define ZipArchive_ENTRY_LOCAL_HEADER_SIGNATURE 0x04034b50
#define ZipArchive_CENTRAL_DIRECTORY_RECORD_SIGNATURE 0x02014b50
#define ZipArchive_END_OF_CENTRAL_RECORD_SIGNATURE 0x06054b50


/************************************************************************/
/* Instantiate a ZipArchiveEntry from a stream positioned at the central   */
/* directory record for a file. Will leave the stream positioned at     */
/* the beginning of the next record.                                    */
/************************************************************************/
ZipArchive::ZipArchiveEntry::ZipArchiveEntry(std::ifstream& input) 
  : inputStream(input) 
{
  input.read(reinterpret_cast<char *>(&centralDirectoryHeader), sizeof(ZipArchiveEntry::CentralDirectoryHeader));

  if (centralDirectoryHeader.signature != ZipArchive_CENTRAL_DIRECTORY_RECORD_SIGNATURE) {
    throw ref new Platform::FailureException(L"Invalid ZIP file entry header");
  }

  filename.reserve(centralDirectoryHeader.filenameLength);
  std::copy_n((std::istreambuf_iterator<char>(input)), centralDirectoryHeader.filenameLength, std::back_inserter(filename));

  fpos_t afterCentralDirectory = input.tellg();
  input.seekg(centralDirectoryHeader.localHeaderOffset, std::ios_base::beg);

  ReadAndCheckLocalHeader();
  
  contentStreamStart = centralDirectoryHeader.localHeaderOffset +
    + sizeof(LocalFileHeader) 
    + localHeader.filenameLength 
    + localHeader.extraFieldLength;

  // make sure the stream is ready to read the next header
  input.seekg(afterCentralDirectory + 1 + centralDirectoryHeader.extraFieldLength, std::ios_base::beg);
}

/************************************************************************/
/* Read the local header and check it against the central directory     */
/************************************************************************/
void ZipArchive::ZipArchiveEntry::ReadAndCheckLocalHeader() {
  inputStream.read(reinterpret_cast<char*>(&localHeader), sizeof(localHeader));
  if (localHeader.signature != ZipArchive_ENTRY_LOCAL_HEADER_SIGNATURE) {
    throw ref new Platform::FailureException(L"Invalid local header");
  }
  std::string localFilename;
  localFilename.reserve(localHeader.filenameLength);
  std::copy_n((std::istreambuf_iterator<char>(inputStream)), localHeader.filenameLength, std::back_inserter(localFilename));
  if (strcmp(localFilename.c_str(), filename.c_str()) != 0) {
    throw ref new Platform::FailureException(L"Filename in local header does not match");
  }
}

/************************************************************************/
/* The file isn't compressed, just pass it through from the stream      */
/* If maxBufSize is larger 0, the buffer size will be limitied          */
/************************************************************************/
std::vector<byte> ZipArchive::ZipArchiveEntry::UncompressedFromStream() {
  uint32 bytesToRead = centralDirectoryHeader.compressedSize;
  std::vector<byte> result;
  result.reserve(centralDirectoryHeader.compressedSize);
  std::copy_n((std::istreambuf_iterator<char>(inputStream)), centralDirectoryHeader.compressedSize, std::back_inserter(result));
  return result;
}

/************************************************************************/
/* Decompress a file compressed using the DEFLATE algorithm             */
/************************************************************************/
std::vector<byte> ZipArchive::ZipArchiveEntry::DeflateFromStream() {
  auto compressedBuffer = UncompressedFromStream();
  // allocate buffer for decompression
  std::vector<byte> decompressedData;
  decompressedData.resize(centralDirectoryHeader.uncompressedSize);

  auto decompressionResult = tinfl_decompress_mem_to_mem(
    decompressedData.data(),
    centralDirectoryHeader.uncompressedSize, 
    compressedBuffer.data(), 
    centralDirectoryHeader.compressedSize, 
    0);

  if (decompressionResult != centralDirectoryHeader.uncompressedSize) {
    throw ref new Platform::FailureException(L"Could not extract data");
  }

  return decompressedData;
}

std::vector<byte> ZipArchive::ZipArchiveEntry::GetUncompressedFileContents() {
  inputStream.seekg(contentStreamStart, std::ios_base::beg);
  switch (centralDirectoryHeader.compressionMethod) {
  case 0:  // file is uncompressed
    return UncompressedFromStream();
  case 8: // deflate
    return DeflateFromStream();
  default:
    throw ref new Platform::FailureException(L"Compression algorithm not supported: " + 
      centralDirectoryHeader.compressionMethod);
  }
}

/************************************************************************/
/* Instantiate the ZipArchive and read its directory of contents        */
/************************************************************************/
ZipArchive::ZipArchive(const std::string& filename) {
  
  fileStream = std::ifstream(filename, std::ios::binary | std::ios::in | std::ios::ate);

  // the central directory record is located at the end of the file
  fpos_t fileSize = fileStream.tellg();
  fileStream.seekg(fileSize-sizeof(ZipArchive::EndOfCentralDirectoryRecord), std::ios_base::beg);
  EndOfCentralDirectoryRecord endOfCentralDirectoryRecord;
  fileStream.read(reinterpret_cast<char *>(&endOfCentralDirectoryRecord), sizeof(endOfCentralDirectoryRecord));
  if (endOfCentralDirectoryRecord.signature != ZipArchive_END_OF_CENTRAL_RECORD_SIGNATURE) {
    throw ref new Platform::FailureException("Could not read ZIP file");
  }

  // check if the real data is in the zip64 header
  uint64 entryCount;
  fpos_t centralDirectoryStart;
  if (endOfCentralDirectoryRecord.centralDirectoryOffset != -1) {
    entryCount = endOfCentralDirectoryRecord.entryCountThisDisk;
    centralDirectoryStart = endOfCentralDirectoryRecord.centralDirectoryOffset;
  } else {
    Zip64EndOfCentralDirectoryRecordLocator zip64EndOfCentralDirectoryLocator;
    fileStream.seekg(fileSize-(sizeof(EndOfCentralDirectoryRecord)+sizeof(zip64EndOfCentralDirectoryLocator)), std::ios_base::beg);
    fileStream.read(reinterpret_cast<char*>(&zip64EndOfCentralDirectoryLocator), sizeof(zip64EndOfCentralDirectoryLocator));
    fileStream.seekg(zip64EndOfCentralDirectoryLocator.centralDirectoryOffset, std::ios_base::beg);
    Zip64EndOfCentralDirectoryRecord zip64EndOfCentralDirectoryRecord;
    fileStream.read(reinterpret_cast<char*>(&zip64EndOfCentralDirectoryRecord), sizeof(zip64EndOfCentralDirectoryRecord));
    
    entryCount = zip64EndOfCentralDirectoryRecord.entryCountThisDisk;
    centralDirectoryStart = zip64EndOfCentralDirectoryRecord.startingDiskCentralDirectoryOffset;
  }

  fileStream.seekg(centralDirectoryStart, std::ios_base::beg);

  archiveEntries.reserve(entryCount);
  for (int i = 0; i < entryCount; i++) {
    archiveEntries.push_back(std::shared_ptr<ZipArchiveEntry>(new ZipArchiveEntry(fileStream)));
  }
}


/************************************************************************/
/* Get the uncompressed file contents as an IBuffer                     */
/************************************************************************/
std::vector<byte> ZipArchive::GetFileContentsAsync(const std::string& filename) {
  std::vector<std::shared_ptr<ZipArchiveEntry>>::iterator entry = std::find_if(archiveEntries.begin(), archiveEntries.end(), 
    [filename](std::shared_ptr<ZipArchiveEntry> archiveEntry) -> bool {
      return (strcmp(archiveEntry->filename.c_str(), filename.c_str()) == 0);
    });
  if (entry != archiveEntries.end()) {
    return (*entry)->GetUncompressedFileContents();
  } else {
    throw ref new Platform::InvalidArgumentException(L"File not in archive");
  }
}
