# Audio UI components

React components for the Home Audio module (`/audio`).

## Components

| File | Role |
|------|------|
| `zone-card.tsx` | Zone grid card with power, volume, and edit affordance |
| `zone-power-button.tsx` | Shared circular play/stop control for zone cards and header quick actions |
| `zone-volume-knob.tsx` | Circular volume control for now-playing zone quick actions |
| `zone-volume-slider.tsx` | Horizontal volume slider for zone cards |
| `zone-edit-sheet.tsx` | iOS-style bottom sheet for editing Hi-Fi2 zone settings |
| `zone-settings-dialog.tsx` | Legacy centered dialog (same save contract as the sheet) |
| `zone-settings-patch.ts` | Shared diff/patch helpers for zone save payloads |
| `zone-edit-slider.tsx` | Purple-accent range sliders used in the edit sheet |
| `source-card.tsx` | Source grid card with status badges and edit affordance |
| `source-edit-sheet.tsx` | Bottom sheet for editing Hi-Fi2 source settings and AirPlay slot |
| `source-settings-patch.ts` | Shared diff/patch helpers for source save payloads |
| `audio-section-tabs.tsx` | Pill-style icon tab bar for the audio page sections |
| `audio-bottom-nav.tsx` | Fixed bottom nav with audio section tabs |
| `audio-now-playing-dropdown-panel.tsx` | Now-playing panel for the header dropdown (track info, progress, zone quick actions) |
| `audio-playback-ui.tsx` | Shared playback progress, cover art, and time formatting |

## Zone edit flow

1. User taps the pencil control on `ZoneCard`.
2. `AudioPage` sets `editZoneNumber` and renders `ZoneEditSheet`.
3. Edits are staged locally until **Save** or **Save Changes**.
4. `buildZoneSettingsPatch` builds a diff-only `ZoneSettingsPatch`.
5. `useAudioModule().saveZoneSettings` sends `PUT /api/audio/zones/:id`.

## Source edit flow

1. User taps the pencil control on `SourceCard`.
2. `AudioPage` sets `editSourceNumber` and renders `SourceEditSheet`.
3. Edits are staged locally until **Save**.
4. `buildSourceSettingsPatch` builds a diff-only `SourceSettingsPatch` (controller fields + optional `airplay: true`).
5. `useAudioModule().saveSourceSettings` sends `PUT /api/audio/sources/:id`.
6. Only one source may be designated AirPlay; the backend enforces exclusivity via `is_airplay` in SQLite.
