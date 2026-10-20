# Contributing

Use Visual Studio 2022, MSVC v143, Windows SDK 10.0.22621.0, and PowerShell. Run `scripts/build.ps1` before opening a pull request. Release archives are assembled with `scripts/package.ps1`.

Keep the window-snapping engine independent of the WinUI editor. Store rectangles in normalized monitor work-area coordinates. Preserve existing layouts when adding settings. Do not add generated files, local `layouts.json` files, personal shortcuts, or binaries to Git.

For UI changes, verify the gallery, desktop editing, saving, cancellation, and settings. Include cropped screenshots that do not expose unrelated desktop content. Test keyboard operation and display scaling where possible.

Bug reports should include the app version, Windows build, monitor sizes and scaling, and concise reproduction steps. Please use issues for proposals before making a large change.
