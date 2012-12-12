#include "stdafx.h"

#include "helper.h"
#include "Package.h"

using Platform::String;

using namespace doo::metrodriver;

enum Action {
  Run,
  Install,
  Update,
  Uninstall
};

Action getAction(const wchar_t* name) {
  if (name == nullptr || StrCmpIW(name, L"run") == 0) {
    return Action::Run;
  } else if (StrCmpIW(name, L"install") == 0) { 
    return Action::Install;
  } else if (StrCmpIW(name, L"update") == 0) {
    return Action::Update;
  } else if (StrCmpIW(name, L"uninstall") == 0) {
    return Action::Uninstall;
  }
  throw ref new Platform::FailureException("Invalid action");
}

static bool endsWith(const std::wstring& str1, const std::wstring& str2) {
  size_t foundPos = str1.rfind(str2);
  return (foundPos == (str1.length()-str2.length()));
}

/*
 validate command line arguments
 the first argument must be a file called AppxManifest.xml or a valid package file ending on ".appx"
 the second argument is the action to perform: install, update, uninstall or run
 the third is optional but if present must be an existing executable file
*/
bool validateArguments(Platform::Array<String^>^ args) {
  if (args->Length < 2) {
    _tprintf_s(L"Please specify the AppxManifest.xml for the application that should be run.\n");
    return false;
  }
  std::wstring sourceFileName(args[1]->Data());
  std::transform(sourceFileName.begin(), sourceFileName.end(), sourceFileName.begin(), ::tolower);
  if (!(endsWith(sourceFileName, L"appxmanifest.xml") || endsWith(sourceFileName, L".appx"))) {
    _tprintf_s(L"The first parameter needs to be a file called AppXManifest.xml or an .appx file\n");
    return false;
  }

  std::ifstream sourceFile(args[1]->Data(), std::ifstream::in);
  if (!sourceFile.is_open()) {
    _tprintf_s(L"Could not open %s\n", args[1]->Data());
    return false;
  }
  sourceFile.close();

  if (args->Length > 2) {
    try {
      auto action = getAction(args[2]->Data());
    } catch (...) {
      _tprintf_s(L"Invalid action. Available commands are: run, install, uninstall\n");
      return false;      
    }
  }

  if (args->Length > 3) {
    std::ifstream callback(args[3]->Data(), std::ifstream::in);
    if (!callback.is_open()) {
      _tprintf_s(L"Callback not found: %s \n", args[3]->Data());
      return false;
    }
    callback.close();
  }

  return true;
}

// invoke the executable in callback with fullAppId as its argument
void InvokeCallback(Platform::String^ callback, Platform::String^ fullAppId) {
    EmptyStruct<PROCESS_INFORMATION> processInformation;
    EmptyStruct<STARTUPINFO> startupInfo;
    std::wstring commandLine(("\"" + callback + "\" " + fullAppId)->Data());
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
      _tprintf_s(L"Couldn't execute callback: %s\n", messageBuffer);
      LocalFree(messageBuffer);
    }
}

void runPackage(Package& package) {
  package.install(Package::InstallationMode::SkipOrUpdate);
  package.enableDebugging(true);

  // start the application
  _tprintf_s(L"Launching app %s\n", package.getFullAppId()->Data());
  auto processId = package.startApplication();
  auto process = ATL::CHandle(OpenProcess(SYNCHRONIZE, false, (DWORD)processId));
  if (process == INVALID_HANDLE_VALUE) {
    throw ref new Platform::FailureException(L"Could not start app. Terminating.\n");
  }

  _tprintf_s(L"Waiting for application %s to finish...\n", package.getFullAppId()->Data());
  WaitForSingleObjectEx(process, INFINITE, false);
  _tprintf_s(L"Application complete\n");
}

/**
  Install and run the application identified by the manifest given as first parameter
  If a second parameter is given, it will be called after the application has exited
  The first parameter to the callback will be the name of the package
 **/
int __cdecl main(Platform::Array<String^>^ args) {
  if (!validateArguments(args)) {
    return -1;
  }

  try {
    Package package(args[1]);

    switch (getAction(args[2]->Data())) {
    case Install:
      package.install(Package::InstallationMode::Reinstall);
      package.enableDebugging(false);
      break;
    case Update:
      package.install(Package::InstallationMode::Update);
      package.enableDebugging(false);
      break;
    case Run:
      runPackage(package);
      // check if there was a callback supplied
      if (args->Length > 3) {
        _tprintf_s(L"Invoking callback: %s\n", args[3]->Data());
        InvokeCallback(args[3], package.getFullAppId());
      }
      break;
    case Uninstall:
      package.uninstall();
      break;
    }
  } catch (Platform::Exception^ e) {
    _tprintf_s(L"An error occurred: %s\n", e->Message->Data());
  }
  
  _tprintf_s(L"Done. Thank you for using MetroDriver.\n");
  return 0;
}