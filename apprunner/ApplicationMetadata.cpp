#include "stdafx.h"

#include <wrl/client.h>

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
    Microsoft::WRL::ComPtr<IAppxFactory> appxFactory;
    auto hr = CoCreateInstance( 
      __uuidof(AppxFactory), 
      NULL, 
      CLSCTX_INPROC_SERVER, 
      __uuidof(IAppxFactory), 
      (LPVOID*)(&appxFactory)); 

    // Create a stream over the input Appx package 
    Microsoft::WRL::ComPtr<IStream> inputStream;
    if (SUCCEEDED(hr)) { 
      hr = SHCreateStreamOnFileEx( 
        appxPath->Data(), 
        STGM_READ | STGM_SHARE_EXCLUSIVE, 
        0, // default file attributes 
        FALSE, // do not create new file 
        NULL, // no template 
        &inputStream); 
    }
    Microsoft::WRL::ComPtr<IAppxPackageReader> packageReader;
    if (SUCCEEDED(hr)) { 
      hr = appxFactory->CreatePackageReader( 
        inputStream.Get(), 
        &packageReader); 
    }
    Microsoft::WRL::ComPtr<IAppxManifestReader> manifestReader;
    if (SUCCEEDED(hr)) {
      hr = packageReader->GetManifest(&manifestReader);
    }

    return ref new ApplicationMetadata(manifestReader);
  } catch (Platform::COMException^ e) {
    _tprintf_s(L"Error decompressing appx file from %s: %s\n", appxPath->Data(), e->Message->Data());
    throw e;
  }
}

ApplicationMetadata::ApplicationMetadata(Microsoft::WRL::ComPtr<IAppxManifestReader> manifestReader) {

  Microsoft::WRL::ComPtr<IAppxManifestApplicationsEnumerator> appEnumerator;
  auto hr = manifestReader->GetApplications(&appEnumerator);
  if (SUCCEEDED(hr)) {
    Microsoft::WRL::ComPtr<IAppxManifestApplication> app;
    hr = appEnumerator->GetCurrent(&app);
    if (SUCCEEDED(hr)) {
      LPWSTR appUserModelId;
      hr = app->GetStringValue(L"id", &appUserModelId);
      if (SUCCEEDED(hr)) {
        appId = ref new Platform::String(appUserModelId);
        CoTaskMemFree(appUserModelId);
      }
    }
  }
  

  Microsoft::WRL::ComPtr<IAppxManifestPackageId> packageId;
  hr = manifestReader->GetPackageId(&packageId);

  if (SUCCEEDED(hr)) {
    APPX_PACKAGE_ARCHITECTURE arch;
    hr = packageId->GetArchitecture(&arch);
    if (SUCCEEDED(hr)) {
      switch (arch) {
      case APPX_PACKAGE_ARCHITECTURE_ARM:
        architecture = "ARM";
        break;
      case APPX_PACKAGE_ARCHITECTURE_X64:
        architecture = "x64";
        break;
      case APPX_PACKAGE_ARCHITECTURE_X86:
        architecture = "x86";
        break;
      }
    }
  
    LPWSTR appxName;
    packageId->GetName(&appxName);
    packageName = ref new Platform::String(appxName);
    CoTaskMemFree(appxName);

    LPWSTR appxPublisher;
    packageId->GetPublisher(&appxPublisher);
    publisher = ref new Platform::String(appxPublisher);
    CoTaskMemFree(appxPublisher);

    UINT64 appxVersion;
    packageId->GetVersion(&appxVersion);
    WORD revision = (appxVersion);
    WORD build = (appxVersion >> 0x10);
    WORD minor = (appxVersion >> 0x20);
    WORD major = (appxVersion >> 0x30);
    std::wstringstream versionBuilder;
    versionBuilder << major << L"." << minor << L"." << build << L"." << revision;
    packageVersion = ref new Platform::String(versionBuilder.str().c_str());

    LPWSTR appxPackageFullId;
    hr = packageId->GetPackageFullName(&appxPackageFullId);
    if (SUCCEEDED(hr)) {
      packageFullName = ref new Platform::String(appxPackageFullId);
      CoTaskMemFree(appxPackageFullId);
    }

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
