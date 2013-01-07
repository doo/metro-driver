#pragma once

template <class T>
struct EmptyStruct : public T {
  EmptyStruct() { ZeroMemory(this, sizeof(T)); }
  operator T&() { return *this; }
};

/** Initializes the given T structures cbSize to the sizeof(T) after 0-filling its memory.

	This provides an easy way of using common Win32 structures
*/
template <class T>
struct WindowsStruct : public EmptyStruct<T> {
  WindowsStruct() {
    *((DWORD*)this) = sizeof(T);
  }
};


// helper function to convert a normal string to a WinRT Platform::String
static Platform::String^ stringToPlatformString(const char* _input, int _length = -1) {
  DWORD dwNum = MultiByteToWideChar(CP_UTF8, 0, _input, _length, NULL, 0);
  LPWSTR wideName = (PWSTR)malloc(sizeof(WCHAR)*dwNum);
  MultiByteToWideChar(CP_UTF8, 0, _input, _length, wideName, dwNum);
  Platform::String^ result = ref new Platform::String(wideName);
  free(wideName);
  return result;
}

// helper to convert a Platform String to a std::string
static std::string platformToStdString(Platform::String^ platformString) {
  DWORD dwNum = WideCharToMultiByte(
    CP_UTF8, 
    NULL,
    platformString->Data(), 
    -1, 
    NULL, 
    0, 
    NULL, 
    NULL);
  if (dwNum == 0) {
    HRESULT res = HRESULT_FROM_WIN32(GetLastError());
    throw ref new Platform::COMException(res);
  }
  char* multiByteUtf8Text = new char[dwNum];
  WideCharToMultiByte(
    CP_UTF8, 
    NULL, 
    platformString->Data(), 
    -1, 
    multiByteUtf8Text, 
    dwNum, 
    NULL, 
    NULL
    );
  std::string result = multiByteUtf8Text;
  delete[] multiByteUtf8Text;
  return result;
}
