# Jianjian

Jianjian is an ESP32-S3 desktop controller with a built-in display, CO2 monitor, Wi-Fi setup portal, answer book, market ticker, and password vault.

## v1.0.0-beta

This beta includes the desktop companion for saving credentials, USB HID typing, safe-code protection, the CO2 sensor screen, and the on-device menu UI.

### Firmware

Install PlatformIO, connect the ESP32-S3 in download mode, then run:

```powershell
pio run -t upload
```

### Desktop companion

`release/Jianjian-Vault-v1.0.0-beta.zip` contains the Windows desktop companion. Connect Jianjian over USB, select its COM port, press Connect, then approve the pairing on the device.

## Beta notice

Password Vault remains a beta feature. Treat credentials stored on the device as convenience data, not as a replacement for a dedicated password manager.