#include "stdafx.h"
#include <Shobjidl.h>

/**
  Run with AppID as one and only parameter
*/
int _tmain(int argc, _TCHAR* argv[]) {
  if (argc < 1) {
    _tprintf_s(L"You must specify the complete AppID, like \"dooGmbH.doo_exsyrgwabam6t!App\"\n");
    return E_INVALIDARG;
  }

  ATL::CComPtr<IApplicationActivationManager> appManager;
  CoInitializeEx(0, COINIT_MULTITHREADED);
  HRESULT res = appManager.CoCreateInstance(__uuidof(ApplicationActivationManager));
  ATLVERIFY(SUCCEEDED(res));
  if FAILED(res) {
    _tprintf_s(L"Could not create ApplicationActivationManager\n");
    return res;
  }

  DWORD processId;
  res = appManager->ActivateApplication(argv[1], nullptr, AO_NONE, &processId);
  if FAILED(res) {
    _tprintf_s(L"Could not activate application %s\n", argv[1]);
    return res;
  }
  ATL::CHandle process(OpenProcess(SYNCHRONIZE, false, processId));  
  if (process == INVALID_HANDLE_VALUE) {

    return -1;
  }
  _tprintf_s(L"Waiting for %s to finish...\n", argv[1]);
  WaitForSingleObjectEx(process, INFINITE, false);
  
  return 0;
}