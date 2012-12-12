#pragma once

#include <Windows.h>
#include <collection.h>

#include "ApplicationMetadata.h"

namespace doo {
  namespace metrodriver {
    class Package {
    public:
      enum InstallationMode {
        Reinstall,
        Update,
        Skip
      };

      // create from either an .appx or AppxManifest.xml
      Package(Platform::String^ source);

      // when debugging is enabled, the app won't be shut down when in the background
      void enableDebugging(bool newValue);

      // provide read-access to the package metadata
      ApplicationMetadata^ getMetaData() {
        return metadata;
      }

      Platform::String^ getFullAppId() {
        return metadata->PackageName + packageSuffix;
      }
      

      // uninstall the app, including possible previous versions
      void uninstall();

      // just install the app
      void install(InstallationMode);

      // start the app and return the process id
      long long startApplication();

    private:
      Windows::ApplicationModel::Package^ findSystemPackage();
      Platform::String^ getPackageVersionString(Windows::ApplicationModel::PackageVersion version);
      void findDependencyPackages();
      void findDependenciesInDirectory(std::string appxPath);

      // stage the appx package and return the location of the staged manifest
      // SIDE EFFECT: will change dependencies so they contain the staged manifests, too
      Platform::String^ stageAppx();

      void postInstall();

      Platform::String^ findStagedManifest(Platform::String^ appxPath);
      Windows::Foundation::Collections::IIterable<Windows::Foundation::Uri^>^ getDependencyUris();

      bool isAppx();
      Platform::String^ source;

      ApplicationMetadata^ metadata;
      Windows::ApplicationModel::Package^ systemPackage;
      Platform::String^ packageSuffix;
      
      Windows::Management::Deployment::PackageManager^ packageManager;
      Windows::ApplicationModel::Package^ storePackage;
      std::vector<Platform::String^> dependencies;
    };
  }
}

