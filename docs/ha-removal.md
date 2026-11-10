# Prehod s HA na Direct GPS — 9. september 2026

PaxCounterWeb source: `C:\Users\peter\source\repos\PaxCounterWeb\PaxCounterWeb`.
Active build: `bin\_homewifi\PaxCounterWeb.dll` (port 5283).
Restart helper: `tools\Start-PaxWeb.ps1` in this firmware repository.

## Implemented

- Phone status and route history use the local SQLite database. Existing historical samples remain stored.
- Authenticated phone control polling delivers interval, power mode and a 120-second Force GPS boost directly over HTTPS.
- Desired phone settings are stored in `Data\phone-gps-controls.json`, surviving server restarts.
- S23 activates if A52 has no contact for 90 seconds or no usable fix for 10 minutes. A selected S23 Force GPS boost temporarily overrides standby.
- New Direct GPS points are preferred and correctly labelled when joining Wi-Fi Pax uplinks.
- Outbound HA implementations are excluded from the build. Runtime overrides in `appsettings.directgps.local.json` disable the legacy MQTT subscriber and HA background service.
- Android 0.2.0 has a serialized offline queue, independent control polling, explicit network permissions, and resumes previously enabled tracking when opened.

## Validation

- Production and staging .NET builds passed.
- 17 isolated checks passed: authorization, phone separation, duplicate and delayed samples, HOME/AWAY policy, durable settings, boost, backup takeover and invalid coordinates.
- Isolated HTTP checks passed: main/Tracking/Settings pages, GPS ingest, GPS-to-Pax join, TTN ingest and Force GPS.
- Android assembleDebug and lintDebug passed.
- Live cutover passed authenticated API readiness; public Tracking/Settings return HTTP 200.
- A52 0.2.0 confirmed on cellular: five-second server interval applied, fresh GPS fix received, queue empty. HA Companion was then disabled for Android user 0.
- S23 (`SM-S911B`) 0.2.0 installed and configured as the A52 backup. Real takeover verified while A52 was replaying old queued points: S23 used the five-second interval, sent fresh cellular GPS and had no queued samples. HA Companion was then disabled for Android user 0 on S23 too.

## A52 DNS recovery

Tailscale's active VPN routed DNS to an unreachable `100.100.100.100`, preventing all hostname lookups. Temporarily stopping `com.tailscale.ipn` restored DNS and HTTPS uploads. Tailscale remains installed, but is stopped. The temporary Private DNS experiment was fully reverted to the original unset values. If Tailscale is reopened, verify its DNS configuration before relying on GPS uploads.

## Recovery

Source preimages: `C:\Users\peter\source\repos\PaxCounterWeb\ha-source-backup-20260909-133603`.
Pre-cutover local configuration: `C:\Users\peter\source\repos\PaxCounterWeb\ha-cutover-backup-20260909-134047`.
Previous executable remains under `bin\_directgps_backup`. Restoring the old configuration and executable also restores HA/MQTT dependencies; do not do this silently.

## Remaining device/configuration checks

- A52 migration is verified; HA Companion is disabled, not uninstalled.
- S23 migration and real backup activation are verified. After A52 caught up, the live control API returned `primaryOnline=true` and `trackingActive=false` for S23, requesting standby again.
- Home Wi-Fi settings are now editable on the Settings page. `idrsek2` and `slove1` are saved; automatic HOME/AWAY is enabled (30 s / 5 s).
- The physical HA host has not been shut down or uninstalled. This migration removes PaxCounterWeb's runtime use of HA.
- Firmware and the T-Beam screen were not modified.
