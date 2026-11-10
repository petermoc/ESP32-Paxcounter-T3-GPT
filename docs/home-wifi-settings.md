# Home Wi-Fi settings

The Settings page now contains a separate **Home Wi-Fi networks** form. Enter one SSID per line, enable automatic home GPS, and select **Save home networks**. Names are case-sensitive; blank lines and duplicates are removed. Names must fit the Wi-Fi limit of 32 UTF-8 bytes. Up to 64 networks can be saved.

The initial requested list is `idrsek2` and `slove1`, with automatic home mode enabled. The configured intervals remain HOME 30 seconds with high accuracy off, AWAY 5 seconds with high accuracy on. Existing manual interval/boost overrides retain their behavior.

Settings persist in `PaxCounterWeb/appsettings.homewifi.local.json`. The file contains no passwords or pairing keys. Saving reloads configuration immediately; no server restart is needed for later edits. The updated phone policy is available on the next normal Direct GPS check.

Implementation: `Services/HomeWifiSettingsService.cs`, `Controllers/SettingsController.cs`, `Views/Settings/Index.cshtml`, `ViewModels/DisplaySettingsViewModel.cs`, and configuration loading in `Program.cs`. `PhoneGpsControlStore` uses the saved list for matching.

Validation: 25 isolated checks passed, including persistence, deletion, empty-list behavior, UTF-8 limits and live policy matching. HTTP form/antiforgery submission checks and browser layout inspection passed. Production build `_homewifi` succeeded. Source backup: `C:\Users\peter\source\repos\PaxCounterWeb\homewifi-backup-20260909-135416`.

Deployment completed after explicit user approval. Active runtime is `_homewifi`; public Settings returned HTTP 200 with both saved networks. Automatic home mode is enabled. The previous `_noha` runtime remains available for rollback.
