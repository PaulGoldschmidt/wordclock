# LichtbildUtilitys (archived)

Desktop tool (Java/Swing, 2019–2020) for configuring the clock over USB: set the time (automatically from the PC or manually), choose colour and brightness, switch summer/winter time automatically, reset to factory settings, and save/load settings as `.Lichtbild` files. The interface is in German.

This software is archived and not maintained.

## Running it

Requires Java 8 or newer.

```
java -jar LichtbildUtilitys.jar
```

Connect the clock via USB, click **Scannen...**, pick the port, then **Verbinden**. On Linux your user needs access to serial ports (usually via the `dialout` group).

## About this build

The Java sources of this tool no longer exist. `LichtbildUtilitys.jar` is the final 2020 build (version 2020-09-19), repackaged in 2026 so it can be published and still runs on current systems:

- **Icons redrawn.** The 2020 build contained third-party images without a licence. The toolbar and app icons are now drawn from [Material Icons](https://github.com/google/material-design-icons) (Apache-2.0).
- **jSerialComm 2.4.0 → 2.11.4.** Adds Apple Silicon and Windows on ARM support and is Apache-2.0 licensed. jSerialComm 2.6.1 changed two setters from `void` to `boolean`, so the two calls in `ArduinoCommunication.openComPort` were patched at the bytecode level to call the new signatures and discard the result.
- **Saving and loading settings works again.** ini4j and Commons IO were only included as nested jars, which Java does not load, so these menu items crashed with `NoClassDefFoundError`. Both are now on the classpath. Commons IO was updated from 2.4 to 2.22.0.
- **`JHyperLink`** (the link in the About dialog) was rewritten from scratch with the same API.
- **About 4 MB of unused files removed** (Photoshop files, unused images, duplicate libraries).
- **`Enable-Native-Access: ALL-UNNAMED`** in the manifest, so newer Java versions do not block the serial library's native code.

ini4j 0.5.4 is the last release of that library. Its known issue, CVE-2022-41404 (denial of service in `fetch()`), is not reachable from this app, which only uses `get`, `put` and `store`.

## Licences

Third-party components and their licences are listed in [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt). The same file is included in the jar as `META-INF/THIRD-PARTY-NOTICES.txt`.
