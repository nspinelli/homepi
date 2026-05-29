# Consumer ALSA contract

Audio and paging modules should use the generated pcm aliases:

- Primary Audio Output: `plug:AudioOut`
- Primary Paging Output: `plug:AudioPaging`

Generated files (default paths):

- `/opt/homepi/runtime/generated/alsa/10-homepi-audio-out.conf`
- `/opt/homepi/runtime/generated/alsa/20-homepi-audio-paging.conf`

Include the generated directory via `ALSA_CONFIG_PATH` or merge into your module ALSA config.
