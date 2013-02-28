#pragma once

#include <ppltasks.h>
#include <AppxPackaging.h>

namespace doo {
  namespace metrodriver {
    ref class ApplicationMetadata sealed
    {
    public:

      static ApplicationMetadata^ CreateFromManifest(Platform::String^ manifestPath);
      static ApplicationMetadata^ CreateFromAppx(Platform::String^ appxPath);

      property Platform::String^ PackageName {
        Platform::String^ get() { return packageName; };
      };

      property Platform::String^ PackageFullName {
        Platform::String^ get() { return packageFullName; };
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

      property Platform::String^ Architecture {
        Platform::String^ get() { return architecture; };
      }
      
    private:

      ApplicationMetadata(const std::string& xml);
      ApplicationMetadata(Microsoft::WRL::ComPtr<IAppxManifestReader> manifest);
      Platform::String^ packageName;
      Platform::String^ packageFullName;
      Platform::String^ packageVersion;
      Platform::String^ publisher;
      Platform::String^ appId;
      Platform::String^ architecture;
    };
  }
}
