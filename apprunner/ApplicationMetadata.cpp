#include "stdafx.h"

#include "ApplicationMetadata.h"
#include "ziparchive.h"
#include "helper.h"

using doo::metrodriver::ApplicationMetadata;

using Windows::Data::Xml::Dom::XmlDocument;
using Windows::Data::Xml::Dom::XmlLoadSettings;


// instantiate Metadata from an extracted manifest on the disk
ApplicationMetadata^ ApplicationMetadata::CreateFromManifest(Platform::String^ manifestPath) {
  std::ifstream input(manifestPath->Data(), std::ifstream::in);
  if (!input.is_open()) {
    throw ref new Platform::InvalidArgumentException(L"Could not open manifest");
  }
  // read the file into a string
  std::string str((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  input.close();
  return ref new ApplicationMetadata(str);
}

#define THROW_ERROR(msg) { \
  wchar_t* errorMsg = msg;\
  _tprintf_s(errorMsg); \
  throw ref new Platform::InvalidArgumentException(ref new Platform::String(errorMsg));\
}

// instantiate Metadata from an appx file
ApplicationMetadata^ ApplicationMetadata::CreateFromAppx(Platform::String^ appxPath) {
  try {
    auto zipFilePath = platformToStdString(appxPath);
    doo::zip::ZipArchive appx(zipFilePath);
    std::vector<byte> manifest = appx.GetFileContents("AppxManifest.xml");
    return ref new ApplicationMetadata(std::string(manifest.begin(), manifest.end()));
  } catch (Platform::COMException^ e) {
    _tprintf_s(L"Error decompressing appx file from %s: %s\n", appxPath->Data(), e->Message->Data());
    throw e;
  }
}

// read metadata from the manifest
ApplicationMetadata::ApplicationMetadata(const std::string& xml) {
    auto manifest = ref new XmlDocument();
    // skip the BOM (first three bytes) if it exists
    auto platformString = stringToPlatformString(xml[0] == -17 ? xml.data()+3 : xml.data());
    manifest->LoadXml(platformString);

    auto identityNode = manifest->SelectSingleNodeNS("//mf:Package/mf:Identity", "xmlns:mf=\"http://schemas.microsoft.com/appx/2010/manifest\"");
    packageName = identityNode->Attributes->GetNamedItem("Name")->NodeValue->ToString();
    packageVersion = identityNode->Attributes->GetNamedItem("Version")->NodeValue->ToString();
    publisher = identityNode->Attributes->GetNamedItem("Publisher")->NodeValue->ToString();
    architecture = identityNode->Attributes->GetNamedItem("ProcessorArchitecture")->NodeValue->ToString();
  
    auto applicationNode = manifest->SelectSingleNodeNS("//mf:Package/mf:Applications/mf:Application[1]", "xmlns:mf=\"http://schemas.microsoft.com/appx/2010/manifest\"");
    appId = applicationNode ? applicationNode->Attributes->GetNamedItem("Id")->NodeValue->ToString() : nullptr;
}
