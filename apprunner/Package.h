#pragma once

#include "ApplicationMetadata.h"

namespace doo {
  namespace metrodriver {
    // an application package, currently only supported in extracted form
    ref class Package sealed
    {
    public:
      // constructor using a manifest XML
      Package(Platform::String^ manifestPath);

      // when debugging is enabled, the app won't be shut down when in the background
      property bool DebuggingEnabled {
        bool get();
        void set(bool newValue);
      }

      // provide read-access to the package metadata
      property ApplicationMetadata^ MetaData {
        ApplicationMetadata^ get() {
          return metadata;
        };
      }

      property Platform::String^ FullAppId {
        Platform::String^ get() {
          return metadata->PackageName + packageSuffix;
        };
      }

      // uninstall the app, including possible previous versions
      void Uninstall();

      // just install the app
      void Install();

      // start the app and return the process id
      long long StartApplication();

    private:
      Windows::ApplicationModel::Package^ findSystemPackage();
      Platform::String^ getPackageVersionString(Windows::ApplicationModel::PackageVersion version);

      ApplicationMetadata^ metadata;
      Windows::ApplicationModel::Package^ systemPackage;
      Platform::String^ packageSuffix;
      Windows::Management::Deployment::PackageManager^ packageManager;
      Windows::ApplicationModel::Package^ storePackage;
      Windows::Foundation::Uri^ packageUri;

    };
  }
}

