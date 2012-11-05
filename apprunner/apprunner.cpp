#include "stdafx.h"

#include "helper.h"
#include "Package.h"

using Platform::String;

using namespace doo::metrodriver;

enum Action {
  Run,
  Install,
  Uninstall
};

Action getAction(const wchar_t* name) {
  if (name == nullptr || StrCmpIW(name, L"run") == 0) {
    return Action::Run;
  } else if (StrCmpIW(name, L"install") == 0) { 
    return Action::Install;
  } else if (StrCmpIW(name, L"uninstall") == 0) {
    return Action::Uninstall;
  }
  throw ref new Platform::FailureException("Invalid action");
}

/*
 validate command line arguments
 the first argument must be a file called AppxManifest.xml
 the second argument is the action to perform: run, install, uninstall
 the third is optional but if present must be an existing executable file
*/
bool validateArguments(Platform::Array<String^>^ args) {
  if (args->Length < 2) {
    _tprintf_s(L"Please specify the AppxManifest.xml for the application that should be run.\n");
    return false;
  }
  std::wstring manifestName(args[1]->Data());
  std::transform(manifestName.begin(), manifestName.end(), manifestName.begin(), ::tolower);
  size_t foundPos = manifestName.rfind(L"appxmanifest.xml");
  if (foundPos == std::wstring::npos || foundPos != args[1]->Length()-16 ) {
    _tprintf_s(L"The first parameter needs to be a file called AppXManifest.xml\n");
    return false;
  }

  std::ifstream manifestFile(args[1]->Data(), std::ifstream::in);
  if (!manifestFile.is_open()) {
    _tprintf_s(L"Could not open manifest file %s\n", args[1]->Data());
    return false;
  }
  manifestFile.close();

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

void runPackage(Package^ package) {
  package->Install();
  package->DebuggingEnabled = true;

  // start the application
  _tprintf_s(L"Launching app %s\n", package->FullAppId->Data());
  auto processId = package->StartApplication();
  auto process = ATL::CHandle(OpenProcess(SYNCHRONIZE, false, (DWORD)processId));
  if (process == INVALID_HANDLE_VALUE) {
    throw ref new Platform::FailureException(L"Could not start app. Terminating.\n");
  }

  _tprintf_s(L"Waiting for application %s to finish...\n", package->FullAppId->Data());
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

  Package^ package;
  try {
    package = ref new Package(args[1]);
  } catch (Platform::COMException^ e) {
    _tprintf_s(L"Error while installing the package: %s\n", e->Message);
    return e->HResult;
  }

  switch (getAction(args->Length > 2 ? args[2]->Data() : nullptr)) {
    case Install:
      package->Install();
      package->DebuggingEnabled = false;
      break;
    case Run:
      runPackage(package);
      // check if there was a callback supplied
      if (args->Length > 2) {
        _tprintf_s(L"Invoking callback: %s\n", args[3]->Data());
        InvokeCallback(args[3], package->FullAppId);
      }
    case Uninstall:
      package->Uninstall();
  }
  _tprintf_s(L"Done. Thank you for using MetroDriver.\n");
  return 0;
}