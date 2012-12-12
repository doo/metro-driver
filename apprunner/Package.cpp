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

Platform::String^ Package::stageAppx() {
  _tprintf_s(L"Staging package version %s\n", metadata->PackageVersion->Data());
  auto appxUri = ref new Windows::Foundation::Uri(source);

  auto deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->StagePackageAsync(
    appxUri, getDependencyUris())).get();

  if (deploymentResult->ErrorText->Length() > 0) {
    throw ref new Platform::FailureException("Staging failed");
  }
  // not nice but working for nows
  std::transform(dependencies.begin(), dependencies.end(), dependencies.begin(), [this](Platform::String^ dependency) {
    return findStagedManifest(dependency);
  });

  return findStagedManifest(source);
}

Windows::Foundation::Collections::IIterable<Windows::Foundation::Uri^>^ Package::getDependencyUris() {
  Platform::Collections::Vector<Windows::Foundation::Uri^>^ dependencyUris = ref new Platform::Collections::Vector<Windows::Foundation::Uri^>();
  std::for_each(dependencies.begin(), dependencies.end(), [dependencyUris](Platform::String^ dependency) {
    dependencyUris->Append(ref new Windows::Foundation::Uri(dependency));
  });
  return dependencyUris;
}

Platform::String^ Package::findStagedManifest(Platform::String^ appxPath) {
  auto metadata = ApplicationMetadata::CreateFromAppx(appxPath);
  std::wstring stagingDirectory = L"C:\\Program Files\\WindowsApps\\";
  WIN32_FIND_DATAW data;
  auto hnd = FindFirstFileW((stagingDirectory 
    + metadata->PackageName->Data() 
    + L"_" + metadata->PackageVersion->Data() 
    + L"_*").c_str(), &data);
  FindClose(hnd);
  return ref new Platform::String((stagingDirectory+data.cFileName+L"\\AppxManifest.xml").c_str());
}

void Package::install(InstallationMode mode) {
  _tprintf_s(L"Installing app\n");

  DeploymentResult^ deploymentResult;
  // just to satisfy the compiler >:|
  Windows::Foundation::Collections::IIterable<Windows::Foundation::Uri^>^ dependencyUris = nullptr;
  auto existingPackage = findSystemPackage();
  if (existingPackage) {
    bool sameVersionInstalled = getPackageVersionString(existingPackage->Id->Version)->Equals(metadata->PackageVersion);
    switch (mode) {
    case SkipOrUpdate:
      if (sameVersionInstalled) {
        postInstall();
        return;
      }
    case Update:
      if (sameVersionInstalled) {
        throw ref new Platform::InvalidArgumentException(L"Package with the same version already installed, cannot update");
      }
      _tprintf_s(L"Updating package from version %s to version %s\n", getPackageVersionString(existingPackage->Id->Version)->Data(), metadata->PackageVersion->Data());
      dependencyUris = getDependencyUris();
      deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->UpdatePackageAsync(
        ref new Windows::Foundation::Uri(source), dependencyUris, DeploymentOptions::None)).get();
      if (deploymentResult->ErrorText->Length() > 0) {
        throw ref new Platform::FailureException(L"Update failed");
      }
      postInstall();
      return;
    case Reinstall:
      uninstall();
      break;
    }
  }
  Windows::Foundation::Uri^ manifestUri = ref new Windows::Foundation::Uri(isAppx() ? stageAppx() : (source));
  _tprintf_s(L"Registering package\n");
  dependencyUris = getDependencyUris();
  deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->RegisterPackageAsync(
    manifestUri, dependencyUris, DeploymentOptions::None)).get();

  if (deploymentResult->ErrorText->Length() > 0) {
    throw ref new Platform::FailureException(L"Installation failed");
  }
  postInstall();
}

void Package::postInstall() {
  systemPackage = findSystemPackage();
  _tprintf_s(L"Installation successful. Full name is: %s\n", systemPackage->Id->FullName->Data());
  packageSuffix = ref new Platform::String(StrRChrW(systemPackage->Id->FullName->Data(), nullptr, '_'));
}

void Package::findDependencyPackages() {
  std::string stdSourcePath = platformToStdString(source);
  std::string packageDirectory;
  std::copy_n(stdSourcePath.begin(), stdSourcePath.rfind('\\'), std::back_inserter(packageDirectory));

  std::vector<std::string> result;
  dependencies.clear();
  findDependenciesInDirectory(packageDirectory+"\\Dependencies\\");
  
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
  
  findDependenciesInDirectory(packageDirectory+"\\Dependencies\\"+architecturePath+"\\");
}

void Package::findDependenciesInDirectory(std::string appxPath) {
  std::string dependencySearchFilter = appxPath + "*.appx";
  WIN32_FIND_DATAA findData;
  auto findFirstHandle = FindFirstFileA(dependencySearchFilter.c_str(), &findData);
  if (findFirstHandle == INVALID_HANDLE_VALUE) {
    return;
  }
  do {
    auto path = stringToPlatformString((appxPath+findData.cFileName).c_str());
    dependencies.push_back(path);
  } while (FindNextFileA(findFirstHandle, &findData));

  FindClose(findFirstHandle);
  return;
}

Windows::ApplicationModel::Package^ Package::findSystemPackage() {
  Windows::ApplicationModel::Package^ package = nullptr;
  Platform::String^ userSid = SystemUtils::GetSIDForCurrentUser();
  auto packageIterable = packageManager->FindPackagesForUser(userSid, metadata->PackageName, metadata->Publisher);
  auto packageIterator = packageIterable->First();
  if (packageIterator->HasCurrent) {
    return packageIterator->Current;
  } else {
    return nullptr;
  }
}

Platform::String^ Package::getPackageVersionString(Windows::ApplicationModel::PackageVersion version) {
  return version.Major.ToString() +
    "." + version.Minor.ToString() +
    "." + version.Build.ToString() +
    "." + version.Revision.ToString();
}

void Package::enableDebugging(bool newValue) { 
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
void Package::uninstall() {
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

long long Package::startApplication() {
  if (!systemPackage) {
    throw ref new Platform::AccessDeniedException(L"Package not installed, cannot start app");
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