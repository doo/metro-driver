#include "stdafx.h"

#using <Windows.winmd>

using Platform::String;
using Windows::Foundation::Uri;
using Windows::Foundation::Collections::IIterable;
using Windows::Data::Xml::Dom::XmlDocument;
using namespace Windows::Management::Deployment;

// uninstall the application
void appCleanup(PackageManager^ packageManager, String^ packageName) {
  auto deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->RemovePackageAsync(packageName)).get();
  if (deploymentResult->ErrorText->Length() > 0) {
    _tprintf_s(L"Uninstalling the package failed: %s", deploymentResult->ErrorText->Data());
  }
}

// format the package version the same way as in the manifest
Platform::String^ getPackageVersionString(Windows::ApplicationModel::PackageVersion version) {
  std::wstringstream versionStringBuffer;
  versionStringBuffer << version.Major.ToString()->Data() << 
    "." << version.Minor.ToString()->Data() << 
    "." << version.Build.ToString()->Data() <<
    "." << version.Revision.ToString()->Data();

  return ref new Platform::String(versionStringBuffer.str().c_str());
}

// retrieve the SID of the current user
// because PackageManager::FindPackages requires elevated privileges 
// while PackageManager::FindPackagesForUser does not
Platform::String^ getSidStringForCurrentUser() {
  ATL::CHandle processHandle(GetCurrentProcess());
  HANDLE tokenHandle;
  if(OpenProcessToken(processHandle,TOKEN_READ,&tokenHandle) == FALSE) {
    printf("Error: Couldn't open the process token\n");
    return nullptr;
  }
  PTOKEN_USER userToken;
  DWORD userTokenSize;
  GetTokenInformation(tokenHandle, TOKEN_INFORMATION_CLASS::TokenUser, nullptr, 0, &userTokenSize);
  userToken = (PTOKEN_USER) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, userTokenSize);
  GetTokenInformation(tokenHandle, TOKEN_INFORMATION_CLASS::TokenUser, userToken, userTokenSize, &userTokenSize);

  LPTSTR simpleSidString;
  ConvertSidToStringSid(userToken->User.Sid, &simpleSidString);
  auto sidString = ref new String(simpleSidString);

  LocalFree(simpleSidString); // as per documentation of ConvertSidToStringSid
  HeapFree(GetProcessHeap(), 0, userToken);
  CloseHandle(tokenHandle);

  return sidString;
}

// find the specified package
Windows::ApplicationModel::Package^ findPackage(PackageManager^ packageManager, String^ userSid, String^ packageName, String^ publisher, String^ version) {
  Windows::ApplicationModel::Package^ package = nullptr;
  auto packageIterable = packageManager->FindPackagesForUser(userSid, packageName, publisher);
  auto packageIterator = packageIterable->First();
  while (packageIterator->HasCurrent) {
    auto currentPackage = packageIterator->Current;
    auto currentPackageVersion = getPackageVersionString(currentPackage->Id->Version);
    if (StrCmpW(currentPackageVersion->Data(), version->Data()) == 0) {
      package = currentPackage;
      break;
    }
    packageIterator->MoveNext();
  }
  return package;
}

/**
  Install and run the application identified by the manifest given as first parameter
  If a second parameter is given, it will be called after the application has exited
  The first parameter to the callback will be the name of the package
*/
int __cdecl main(Platform::Array<String^>^ args) {
  // do some param checking first
  if (args->Length < 2) {
    _tprintf_s(L"Please specify the AppxManifest.xml for the application that should be run");
    return -1;
  }
  std::wstring manifestName(args[1]->Data());
  DWORD foundPos = manifestName.rfind(L"AppxManifest.xml");
  if (foundPos == std::wstring::npos || foundPos != args[1]->Length()-16 ) {
    _tprintf_s(L"The first parameter needs to be a file called AppxManifest.xml");
    return -1;
  }
  // read the package name and version from the manifest
  Windows::Storage::StorageFile^ manifestFile;
  try {
     manifestFile = Concurrency::task<Windows::Storage::StorageFile^>(
      Windows::Storage::StorageFile::GetFileFromPathAsync(args[1])
     ).get();
  } catch (Platform::Exception^ e) {
    _tprintf_s(L"Error reading file: %s.", e->Message->Data());
    return e->HResult;
  }

  XmlDocument^ xmlDocument = Concurrency::task<XmlDocument^>(
    XmlDocument::LoadFromFileAsync(manifestFile)
  ).get();

  auto identityNode = xmlDocument->SelectSingleNodeNS("//mf:Package/mf:Identity", "xmlns:mf=\"http://schemas.microsoft.com/appx/2010/manifest\"");
  auto packageName = identityNode->Attributes->GetNamedItem("Name")->NodeValue->ToString();
  auto packageVersion = identityNode->Attributes->GetNamedItem("Version")->NodeValue->ToString();
  auto publisher = identityNode->Attributes->GetNamedItem("Publisher")->NodeValue->ToString();
  auto applicationNode = xmlDocument->SelectSingleNodeNS("//mf:Package/mf:Applications/mf:Application[1]", "xmlns:mf=\"http://schemas.microsoft.com/appx/2010/manifest\"");
  auto appId = applicationNode->Attributes->GetNamedItem("Id")->NodeValue->ToString();

  // install the package
  auto packageUri = ref new Windows::Foundation::Uri(args[1]);
  auto packageManager = ref new PackageManager();
  auto deploymentResult = Concurrency::task<DeploymentResult^>(packageManager->RegisterPackageAsync(
    packageUri, 
    nullptr, 
    DeploymentOptions::DevelopmentMode
    )).get();

  if (deploymentResult->ErrorText->Length() > 0) {
    _tprintf_s(L"Error while installing the package: %s", deploymentResult->ErrorText->Data());
    return -1;
  }

  // package installed, retrieve it from the system to find out about its crazy suffix ^^
  auto userSid = getSidStringForCurrentUser();
  auto package = findPackage(packageManager, userSid, packageName, publisher, packageVersion);
  if (package == nullptr) {
    _tprintf_s(L"Package supposedly installed but not found in in the package manager.\n");
    return -1;
  }

  // package found, retrieve the suffix from the package name to build the application id
  auto packageSuffix = ref new Platform::String(StrRChrW(package->Id->FullName->Data(), nullptr, '_'));
  auto fullAppId = packageName + packageSuffix + "!" + appId;

  _tprintf_s(L"Preparing to launch app %s\n", appId->Data());

  // start the application
  ATL::CComPtr<IApplicationActivationManager> appManager;
  HRESULT res = appManager.CoCreateInstance(__uuidof(ApplicationActivationManager));
  ATLVERIFY(SUCCEEDED(res));
  if FAILED(res) {
    _tprintf_s(L"Could not create ApplicationActivationManager\n");
    appCleanup(packageManager, packageName);
    return res;
  }

  DWORD processId;
  res = appManager->ActivateApplication(fullAppId->Data(), nullptr, AO_NONE, &processId);
  if FAILED(res) {
    _tprintf_s(L"Could not activate application %s\n", fullAppId->Data());
    appCleanup(packageManager, packageName);
    return res;
  }
  ATL::CHandle process(OpenProcess(SYNCHRONIZE, false, processId));  
  if (process == INVALID_HANDLE_VALUE) {
    return -1;
  }

  _tprintf_s(L"Waiting for %s to finish...\n",fullAppId->Data());
  WaitForSingleObjectEx(process, INFINITE, false);

  // check if there was a callback supplied
  if (args->Length > 2) {
    PROCESS_INFORMATION processInformation;
    STARTUPINFO startupInfo;
    ZeroMemory(&processInformation, sizeof(processInformation));
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    std::wstring commandLine(("\"" + args[2] + "\" " + packageName + packageSuffix)->Data());
    if (CreateProcessW(NULL, &commandLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInformation)) {
      WaitForSingleObjectEx(processInformation.hProcess, INFINITE, false );
      // Close process and thread handles. 
      CloseHandle( processInformation.hProcess );
      CloseHandle( processInformation.hThread );
    } else {
      auto errorCode = GetLastError();
      LPVOID messageBuffer;
      FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&messageBuffer,
        0, NULL );
      _tprintf_s(L"Couldn't execute callback: %s", messageBuffer);
      LocalFree(messageBuffer);
    }
  }
  appCleanup(packageManager, package->Id->FullName);
  return 0;
}