#include "stdafx.h"
#include "SystemUtils.h"

using doo::metrodriver::SystemUtils;

Platform::String^ SystemUtils::GetSIDForCurrentUser() {
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
  auto sidString = ref new Platform::String(simpleSidString);

  LocalFree(simpleSidString); // as per documentation of ConvertSidToStringSid
  HeapFree(GetProcessHeap(), 0, userToken);
  CloseHandle(tokenHandle);

  return sidString;
}
