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