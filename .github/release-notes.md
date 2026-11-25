**[Download V3 Pocket for firmware update (.bin)]({download_url})** · `{filename}`

For a **Heltec WiFi LoRa 32 V3** already running a MeshCore **repeater** with the compatible 8 MB dual-OTA layout.

1. Back up your repeater identity and settings. Open its MeshCore admin CLI and run `start ota`.
2. Join the `MeshCore-OTA` Wi-Fi network from your computer or phone. Open the update address printed by the repeater. Choose **Firmware**, select the downloaded **`-app.bin`** and start the update.
3. Keep the repeater powered until it restarts. Confirm `ver`, node identity and radio settings. Tap PRG for Repeaters. The four-result scan starts when you hold PRG on that page.

**Roll back to stock.** {stock_instruction} You can also use USB with the [official MeshCore flasher](https://flasher.meshcore.io/). Choose **Heltec v3 → Repeater** and leave **Erase device** unchecked. A merged image is for a different installation path and must stay out of the firmware OTA upload.

[Graph preview, customization and source](https://github.com/{repo}#the-packet-graph) · [Checksums]({manifest_url})
