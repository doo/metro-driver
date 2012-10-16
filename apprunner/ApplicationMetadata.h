#pragma once

#include <ppltasks.h>

namespace doo {
  namespace metrodriver {
    ref class ApplicationMetadata sealed
    {
    public:

      ApplicationMetadata(Platform::String^ manifestPath);

      property Platform::String^ PackageName {
        Platform::String^ get() { return packageName; };
      };

      property Platform::String^ PackageVersion {
        Platform::String^ get() { return packageVersion; };
      }

      property Platform::String^ Publisher {
        Platform::String^ get() { return publisher; };
      }

      property Platform::String^ AppId {
        Platform::String^ get() { return appId; };
      }
      
    private:
      Platform::String^ packageName;
      Platform::String^ packageVersion;
      Platform::String^ publisher;
      Platform::String^ appId;
    };
  }
}
