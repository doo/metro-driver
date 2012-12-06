#include "stdafx.h"

#include "ApplicationMetadata.h"

using doo::metrodriver::ApplicationMetadata;

using Windows::Data::Xml::Dom::XmlDocument;
using Windows::Data::Xml::Dom::XmlLoadSettings;

// helper function to convert a normal string to a WinRT Platform::String
static Platform::String^ stringToPlatformString(const char* _input) {
    DWORD dwNum = MultiByteToWideChar(CP_UTF8, 0, _input, -1, NULL, 0);
    LPWSTR wideName = (PWSTR)malloc(sizeof(WCHAR)*dwNum);
    MultiByteToWideChar(CP_UTF8, 0, _input, -1, wideName, dwNum);
    Platform::String^ result = ref new Platform::String(wideName);
    free(wideName);
    return result;
}

// instantiate Metadata from an extracted manifest on the disk
ApplicationMetadata^ ApplicationMetadata::CreateFromManifest(Platform::String^ manifestPath) {
  std::ifstream input(manifestPath->Data(), std::ifstream::in);
  if (!input.is_open()) {
    throw ref new Platform::InvalidArgumentException(L"Could not open manifest");
  }
  // read the file into a string
  std::string str((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  input.close();
  // skip the BOM (first three bytes) if it exists
  auto platformString = stringToPlatformString(str[0] == -17 ? str.data()+3 : str.data());

  try {
    auto manifest = ref new XmlDocument();
    manifest->LoadXml(platformString);

    auto metadata = ref new ApplicationMetadata();
    metadata->ReadDataFromXml(manifest);

    return metadata;
  } catch (Platform::COMException^ e) {
    _tprintf_s(L"Error parsing XML: %s\n", e->Message->Data());
    throw e;
  }
}



// read metadata from the manifest
void ApplicationMetadata::ReadDataFromXml(Windows::Data::Xml::Dom::XmlDocument^ manifest) {
    auto identityNode = manifest->SelectSingleNodeNS("//mf:Package/mf:Identity", "xmlns:mf=\"http://schemas.microsoft.com/appx/2010/manifest\"");
    packageName = identityNode->Attributes->GetNamedItem("Name")->NodeValue->ToString();
    packageVersion = identityNode->Attributes->GetNamedItem("Version")->NodeValue->ToString();
    publisher = identityNode->Attributes->GetNamedItem("Publisher")->NodeValue->ToString();
  
    auto applicationNode = manifest->SelectSingleNodeNS("//mf:Package/mf:Applications/mf:Application[1]", "xmlns:mf=\"http://schemas.microsoft.com/appx/2010/manifest\"");
    appId = applicationNode->Attributes->GetNamedItem("Id")->NodeValue->ToString();
}