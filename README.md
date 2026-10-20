# Layrix

**A standalone zone-based window manager for Windows. No PowerToys installation required.**

[Русская документация](README.ru.md) · [Download](https://github.com/salvrave001/Layrix/releases/latest) · [MIT license](LICENSE)

Layrix focuses on arranging windows into zones. It is an independent alternative for people who want a FancyZones-style workflow without installing the full PowerToys suite. It is not affiliated with Microsoft or PowerToys.

## Features

- Hold **Shift** while dragging a window to reveal zones; release the mouse to snap it.
- Hold **Shift + Ctrl** to select several zones on one monitor.
- Move the active window between zones with **Win + Left / Right**.
- Edit zones directly on your desktop: move, resize, split, add, and remove them.
- Resize shared borders together, including borders between one large and several smaller zones.
- Assign different layouts to different monitors.
- Built-in layouts: focus, columns, rows, grid, and priority grid, plus custom layouts.
- Dark WinUI editor and floating editing toolbar.
- Optional startup with Windows, running quietly in the notification area.
- Custom highlight color and zone spacing.

The current interface is in Russian. English localization is not included in v1.0.0.

## Download and run

1. Download `Layrix-1.0.0-windows-x64.zip` from [Releases](https://github.com/salvrave001/Layrix/releases).
2. Extract the **entire** archive into a writable folder. Do not run the application from inside the ZIP.
3. Run `Layrix.exe`. It starts in the notification area.
4. Double-click its tray icon to open the layout gallery, or right-click it and choose **Редактор зон**.

Windows 10 version 1809 or later, or Windows 11, **x64**. ARM64 and x86 builds are not provided. The portable archive includes the Windows App SDK runtime and the Visual C++ runtime DLLs. Keep the included DLLs and language folders next to the executable.

The initial release is unsigned. Windows may show a publisher warning. Verify the archive against the included `SHA256SUMS.txt` before running it.

## Desktop editing

Click the pencil on a layout card. The gallery hides and the zones appear over the selected monitor's desktop at their actual size.

Drag a zone or its edges. Draw in an empty area to add a zone, or use the floating toolbar. **Save / Ctrl + S** applies the layout; **Escape** cancels the editing session. Editing a built-in template creates a custom copy.

## Settings

Open **Настройки** from the gallery or tray menu. Changes apply immediately.

- **Запускать вместе с Windows**: startup for the current Windows user. Off by default; no editor window opens at sign-in.
- **Привязка окон к зонам**: enable or disable snapping.
- **Win + Left / Right**: enable or disable arrow-key interception.

Settings and layouts are stored in `layouts.json` next to `Layrix.exe`; this file is created on first launch. Back it up before upgrading and copy it into the new version's folder. Disable startup before moving or deleting the installation, then enable it again from the new location.

## Limitations

- Windows with a higher integrity level (for example, an administrator app while Layrix runs normally) are skipped.
- Some applications enforce their own minimum window size.
- FancyZones layout import and PowerToys feature parity are not provided.
- Do not enable both Layrix's and another window manager's interception of the same shortcuts.
- This first release has been tested locally, not across every supported Windows version or display configuration.

## Build

Install Visual Studio 2022 / Build Tools with **Desktop development with C++**, the **v143 toolset**, and **Windows SDK 10.0.22621.0**. NuGet packages are restored from nuget.org.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -Version 1.0.0
```

`build.ps1` restores dependencies, builds the editor and engine, and runs `Layrix.exe --self-test`. The package is written to `dist/` and excludes personal layouts, debug symbols, and old executables. GitHub Actions runs the same scripts; pushing a `v*` tag publishes a release after a successful build.

Command-line switches: `--editor`, `--settings`, `--self-test`. Starting without a switch runs the tray engine.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Include Windows version, display scaling, monitor configuration, and reproduction steps in bug reports. Avoid sharing private layout files or desktop screenshots without reviewing them first.

## License and credits

Layrix source code is licensed under [MIT](LICENSE). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency licenses.

The workflow is inspired by [FancyZones in Microsoft PowerToys](https://github.com/microsoft/PowerToys). FancyZones and PowerToys names belong to their respective owners.
