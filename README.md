Metro-Driver Readme
===================

The Metro-Driver will install and optionally run and uninstall a Windows Store Application package that has been unzipped onto the disk.
All previous versions of the same package will be uninstalled first.
Afterwards, it will optionally invoke a callback executable or script with the full package name as its first and only argument.

You can use this utility to automatically test your application in a Continuous Integration environment like Jenkins.


Usage
-----

apprunner.exe [Full\Path\To\AppXManifest.xml] [run|install|uninstall] [Full\Path\To\Callback.exe]


Hints
-----

You can close a JavaScript-based application by invoking window.close()
See the packaged sample-callback.cmd on how you can use the fully-qualified package name to copy generated data from the application local storage into a non-volatile directory.


TODO
----

Support for directly installing .appx packages is planned but not yet scheduled.
Pull requests for this or other features are welcome.

For bug reports and contributions, please visit the [project page on Github](https://github.com/doo/metro-driver)
