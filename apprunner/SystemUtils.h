#pragma once
namespace doo {
  namespace metrodriver {
    class SystemUtils
    {
    public:
      static Platform::String^ GetSIDForCurrentUser();
    };
  }
}

