## 1.3.1
* Fixed an issue with missing icon id error

## 1.3.0

* Added `AudioRole` class, with `  console,  multimedia,  communications,` as parameters
* This parameters is ignored on functions that have audioRole and also separate roles as paramters, such as `setDefaultDevice` and `switchDefaultDevice`.

## 1.2.0

* Added new parameters for setDefaultDevice and switchDefaultDevice:
* `{bool console = false, bool multimedia = true, bool communications = false}`

## 1.1.1

* Tiny tweak at how Method sends data.
  
## 1.1.0

* Changed icon extraction process because old one crashed on monitors with higher DPI
* Added a new class `WinIcons` with 3 functions `extractExecutableIcon`, `extractWindowIcon`, `extractIconHandle`

## 1.0.2

* Documentation still missing from pub.dev -.-

## 1.0.1

* Fixed version to a more serios version
* Generated Documentation

## 0.0.1

* Initial Release with the main functionality of the package.

