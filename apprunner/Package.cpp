#include "stdafx.h"

#include "Package.h"
#include "SystemUtils.h"

using Windows::Storage::StorageFile;
using Windows::Data::Xml::Dom::XmlDocument;

using namespace Windows::Management::Deployment;

using doo::metrodriver::Package;

Package::Package(Platform::String^ manifestPath) {
  packageManager = ref new PackageManager();
  packageUri = ref new Windows::Foundation::Uri(manifestPath);
  metadata = ApplicationMetadata::CreateFromManifest(manifestPath);
}

void Package::Install() {
  Uninstall(); // make sure the package can be installed in this version
  _tprintf_s(L"Installing app\n");
  auto deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->RegisterPackageAsync(
    packageUri, 
    nullptr, 
    DeploymentOptions::DevelopmentMode
    )).get();
  if (deploymentResult->ErrorText->Length() > 0) {
    throw ref new Platform::FailureException("Deployment failed");
  }
  systemPackage = findSystemPackage();
  _tprintf_s(L"Installation successful. Full name is: %s\n", systemPackage->Id->FullName->Data());
  packageSuffix = ref new Platform::String(StrRChrW(systemPackage->Id->FullName->Data(), nullptr, '_'));

}

Windows::ApplicationModel::Package^ Package::findSystemPackage() {
  Windows::ApplicationModel::Package^ package = nullptr;
  Platform::String^ userSid = SystemUtils::GetSIDForCurrentUser();
  auto packageIterable = packageManager->FindPackagesForUser(userSid, metadata->PackageName, metadata->Publisher);
  auto packageIterator = packageIterable->First();
  while (packageIterator->HasCurrent) {
    auto currentPackage = packageIterator->Current;
    auto currentPackageVersion = getPackageVersionString(currentPackage->Id->Version);
    if (StrCmpW(currentPackageVersion->Data(), metadata->PackageVersion->Data()) == 0) {
      package = currentPackage;
      break;
    }
    packageIterator->MoveNext();
  }

  if (package == nullptr) {
    throw ref new Platform::FailureException("Could not find installed package in registry");
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