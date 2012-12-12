#include "stdafx.h"

#include <collection.h>

#include "Package.h"
#include "SystemUtils.h"
#include "helper.h"

using Windows::Storage::StorageFile;
using Windows::Data::Xml::Dom::XmlDocument;

using namespace Windows::Management::Deployment;

using doo::metrodriver::Package;

Package::Package(Platform::String^ sourcePath) 
  : source(sourcePath) 
{
  packageManager = ref new PackageManager();
  packageUri = ref new Windows::Foundation::Uri(sourcePath);

  dependencyUris = ref new Platform::Collections::Vector<Windows::Foundation::Uri^>();

  if (isAppx()) {
    metadata = ApplicationMetadata::CreateFromAppx(sourcePath);
    findDependencyPackages();
  } else {
    metadata = ApplicationMetadata::CreateFromManifest(sourcePath);
  }
}

bool Package::isAppx() {
  return StrCmpIW(source->Data()+(source->Length()-5), L".appx") == 0;
}

void Package::Install(bool update) {
  _tprintf_s(L"Installing app\n");
  DeploymentResult^ deploymentResult;
  auto existingPackage = findSystemPackage(nullptr);
  Platform::String^ manifestPath;
  if (isAppx()) {
    deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->StagePackageAsync(
      packageUri, dependencyUris)).get();

    if (deploymentResult->ErrorText->Length() > 0) {
      throw ref new Platform::FailureException("Staging failed");
    }
    std::wstring stagingDirectory = L"C:\\Program Files\\WindowsApps\\";
    WIN32_FIND_DATAW data;
    auto hnd = FindFirstFileW((stagingDirectory 
      + metadata->PackageName->Data() 
      + L"_" + metadata->PackageVersion->Data() 
      + L"_*").c_str(), &data);
    manifestPath = ref new Platform::String((stagingDirectory+data.cFileName+L"\\AppxManifest.xml").c_str());
    CloseHandle(hnd);
  } else {
    manifestPath = source;
  }
  deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->RegisterPackageAsync(
    ref new Windows::Foundation::Uri(manifestPath), dependencyUris, DeploymentOptions::None)).get();
  if (deploymentResult->ErrorText->Length() > 0) {
    throw ref new Platform::FailureException(L"Registering failed");
  }

  systemPackage = findSystemPackage(metadata->PackageVersion);
  _tprintf_s(L"Installation successful. Full name is: %s\n", systemPackage->Id->FullName->Data());
  packageSuffix = ref new Platform::String(StrRChrW(systemPackage->Id->FullName->Data(), nullptr, '_'));

}

void Package::findDependencyPackages() {
  std::string stdSourcePath = platformToStdString(source);
  std::string packageDirectory;
  std::copy_n(stdSourcePath.begin(), stdSourcePath.rfind('\\'), std::back_inserter(packageDirectory));

  std::vector<std::string> result;
  findPackagesInDirectory(packageDirectory+"\\Dependencies\\", dependencyUris);
  
  std::string architecturePath;
  SYSTEM_INFO systemInfo;
  GetNativeSystemInfo(&systemInfo);
  switch (systemInfo.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64:
    architecturePath = "x64";
    break;
  case PROCESSOR_ARCHITECTURE_ARM:
    architecturePath = "ARM";
    break;
  case PROCESSOR_ARCHITECTURE_INTEL:
    architecturePath = "x86";
    break;
  default:
    return; // maybe throw exception?
  }
  
  findPackagesInDirectory(packageDirectory+"\\Dependencies\\"+architecturePath+"\\", dependencyUris);
}

void Package::findPackagesInDirectory(std::string appxPath, Platform::Collections::Vector<Windows::Foundation::Uri^>^ target) {
  std::string dependencyDir = appxPath + "*.appx";
  WIN32_FIND_DATAA findData;
  auto findFirstHandle = FindFirstFileA(dependencyDir.c_str(), &findData);
  if (findFirstHandle == INVALID_HANDLE_VALUE) {
    return;
  }
  do {
    auto path = stringToPlatformString((appxPath+findData.cFileName).c_str());
    auto uri = ref new Windows::Foundation::Uri(path);
    target->Append(uri);
  } while (FindNextFileA(findFirstHandle, &findData));

  FindClose(findFirstHandle);
  return;
}

Windows::ApplicationModel::Package^ Package::findSystemPackage(Platform::String^ version) {
  Windows::ApplicationModel::Package^ package = nullptr;
  Platform::String^ userSid = SystemUtils::GetSIDForCurrentUser();
  auto packageIterable = packageManager->FindPackagesForUser(userSid, metadata->PackageName, metadata->Publisher);
  auto packageIterator = packageIterable->First();
  if (version != nullptr) {
    while (packageIterator->HasCurrent) {
      auto currentPackage = packageIterator->Current;
      auto currentPackageVersion = getPackageVersionString(currentPackage->Id->Version);
      if (StrCmpW(currentPackageVersion->Data(), version->Data()) == 0) {
        package = currentPackage;
        break;
      }
      packageIterator->MoveNext();
    }
    if (package == nullptr) {
      throw ref new Platform::FailureException("Could not find installed package in registry");
    }
  } else if (packageIterator->HasCurrent) {
    package = packageIterator->Current;
  }

  return package;
}

Platform::String^ Package::getPackageVersionString(Windows::ApplicationModel::PackageVersion version) {
  return version.Major.ToString() +
    "." + version.Minor.ToString() +
    "." + version.Build.ToString() +
    "." + version.Revision.ToString();
}

void Package::DebuggingEnabled::set(bool newValue) { 
  static ATL::CComQIPtr<IPackageDebugSettings> packageDebugSettings;
  if (!systemPackage) {
    throw ref new Platform::FailureException(L"Package needs to be installed before configuring debugging");
  }
  if (!packageDebugSettings) {
    HRESULT res = packageDebugSettings.CoCreateInstance(CLSID_PackageDebugSettings, NULL, CLSCTX_ALL);
    if FAILED(res) {
      _tprintf_s(L"Failed to instantiate a PackageDebugSettings object. Debugging could not be configured");
      return;
    }  
  }
  if (newValue) {
    _tprintf_s(L"Enabling debugging for %s\n", systemPackage->Id->FullName->Data());
    packageDebugSettings->EnableDebugging(systemPackage->Id->FullName->Data(), NULL, NULL);
  } else {
    _tprintf_s(L"Disabling debugging\n");
    packageDebugSettings->DisableDebugging(systemPackage->Id->FullName->Data());
  }
}

// uninstall the current and all previous versions of this package
void Package::Uninstall() {
  Windows::ApplicationModel::Package^ package = nullptr;
  Platform::String^ userSid = SystemUtils::GetSIDForCurrentUser();
  auto packageIterable = packageManager->FindPackagesForUser(userSid, metadata->PackageName, metadata->Publisher);
  auto packageIterator = packageIterable->First();
  while (packageIterator->HasCurrent) {
    auto currentPackage = packageIterator->Current;
    _tprintf_s(L"Uninstalling %s %s\n", currentPackage->Id->Name->Data(), getPackageVersionString(currentPackage->Id->Version)->Data());
    auto deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->RemovePackageAsync(currentPackage->Id->FullName)).get();
    if (deploymentResult->ErrorText->Length() > 0) {
      throw ref new Platform::FailureException(L"Could not uninstall previously installed version: " + deploymentResult->ErrorText);
    }
    packageIterator->MoveNext();
  }
}

long long Package::StartApplication() {
  if (!systemPackage) {
    Install();
  }

  auto fullAppId = metadata->PackageName + packageSuffix + "!" + metadata->AppId;

  ATL::CComPtr<IApplicationActivationManager> appManager;
  HRESULT res = appManager.CoCreateInstance(__uuidof(ApplicationActivationManager));
  ATLVERIFY(SUCCEEDED(res));
  if FAILED(res) {
    throw ref new Platform::FailureException(L"Could not create ApplicationActivationManager");
  }

  DWORD processId;
  res = appManager->ActivateApplication(fullAppId->Data(), nullptr, AO_NONE, &processId);
  if FAILED(res) {
    throw ref new Platform::FailureException(L"Could not activate application %s\n" + fullAppId);
  }
  return processId;
}