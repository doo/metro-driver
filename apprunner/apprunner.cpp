#include "stdafx.h"

#include <iostream>
#include <fstream>

#include "Package.h"

using Platform::String;

using namespace doo::metrodriver;

// validate command line arguments
// the first argument must be a file called AppxManifest.xml
// the second is optional but must be an existing file if given
bool validateArguments(Platform::Array<String^>^ args) {
  if (args->Length < 2) {
    _tprintf_s(L"Please specify the AppxManifest.xml for the application that should be run.\n");
    return false;
  }
  std::wstring manifestName(args[1]->Data());
  size_t foundPos = manifestName.rfind(L"AppxManifest.xml");
  if (foundPos == std::wstring::npos || foundPos != args[1]->Length()-16 ) {
    _tprintf_s(L"The first parameter needs to be a file called AppxManifest.xml\n");
    return false;
  }

  std::ifstream manifestFile(args[1]->Data(), std::ifstream::in);
  if (!manifestFile.is_open()) {
    _tprintf_s(L"Could not open manifest file %s\n", args[1]->Data());
    return false;
  }
  manifestFile.close();

  if (args->Length > 2) {
    std::ifstream callback(args[2]->Data(), std::ifstream::in);
    if (!callback.is_open()) {
      _tprintf_s(L"Callback not found: %s \n", args[2]->Data());
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

  _tprintf_s(L"Enabling debugging\n");
  package->DebuggingEnabled = true;

  // start the application
  _tprintf_s(L"Launching app %s\n", package->FullAppId->Data());
  auto processId = package->StartApplication();
  auto process = ATL::CHandle(OpenProcess(SYNCHRONIZE, false, (DWORD)processId));
  if (process == INVALID_HANDLE_VALUE) {
    _tprintf_s(L"Could not start app. Terminating.\n");
    return -1;
  }

  _tprintf_s(L"Waiting for application %s to finish...\n", package->FullAppId->Data());
  WaitForSingleObjectEx(process, INFINITE, false);
  _tprintf_s(L"Application complete\n");

  // check if there was a callback supplied
  if (args->Length > 2) {
    _tprintf_s(L"Invoking callback: %s\n", args[2]->Data());
    InvokeCallback(args[2], package->FullAppId);
  }
  _tprintf_s(L"Done. Thank you for using MetroDriver.\n");
  return 0;
}