# Audio UI components

React components for the Home Audio module (`/audio`).

## Components

| File | Role |
|------|------|
| `zone-card.tsx` | Zone grid card with power, volume, and edit affordance |
| `zone-edit-sheet.tsx` | iOS-style bottom sheet for editing Hi-Fi2 zone settings |
| `zone-settings-dialog.tsx` | Legacy centered dialog (same save contract as the sheet) |
| `zone-settings-patch.ts` | Shared diff/patch helpers for zone save payloads |
| `zone-edit-slider.tsx` | Purple-accent range sliders used in the edit sheet |
| `audio-section-tabs.tsx` | Pill-style icon tab bar for the audio page sections |
| `audio-player-bar.tsx` | Now-playing transport bar above the audio section tabs |

## Edit flow

1. User taps the pencil control on `ZoneCard`.
2. `AudioPage` sets `editZoneNumber` and renders `ZoneEditSheet`.
3. Edits are staged locally until **Save** or **Save Changes**.
4. `buildZoneSettingsPatch` builds a diff-only `ZoneSettingsPatch`.
5. `useAudioModule().saveZoneSettings` sends `PUT /api/audio/zones/:id`.
