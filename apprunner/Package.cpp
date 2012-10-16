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
  metadata = ref new ApplicationMetadata(manifestPath);
  auto deploymentResult = Deploy().get();
  if (deploymentResult->ErrorText->Length() > 0) {
    throw ref new Platform::FailureException("Deployment failed");
  }
  systemPackage = findSystemPackage();
  packageSuffix = ref new Platform::String(StrRChrW(systemPackage->Id->FullName->Data(), nullptr, '_'));
}

Concurrency::task<DeploymentResult^> Package::Deploy() {
  return Concurrency::task<DeploymentResult^>(packageManager->RegisterPackageAsync(
    packageUri, 
    nullptr, 
    DeploymentOptions::DevelopmentMode
    ));
};

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

Package::~Package() {
  auto deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->RemovePackageAsync(systemPackage->Id->FullName)).get();
  if (deploymentResult->ErrorText->Length() > 0) {
    _tprintf_s(L"Uninstalling the package failed: %s", deploymentResult->ErrorText->Data());
  }
}

void Package::DebuggingEnabled::set(bool newValue) { 
  ATL::CComQIPtr<IPackageDebugSettings> sp;
  HRESULT res = sp.CoCreateInstance(CLSID_PackageDebugSettings, NULL, CLSCTX_ALL);
  if FAILED(res) {
    _tprintf_s(L"Failed to instantiate a PackageDebugSettings object. Continuing anyway.");
  } else {
    sp->EnableDebugging(systemPackage->Id->FullName->Data(), NULL, NULL);
  }
}

long long Package::StartApplication() {
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