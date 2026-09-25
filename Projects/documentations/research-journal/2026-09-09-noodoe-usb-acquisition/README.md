# Actual Noodoe donor USB acquisition, 2026-09-09

- [Consolidated Korean report](../../docs/2026-09-09-noodoe-usb-acquisition.md)
- [Donor observations](../../docs/2026-09-09-donor-hardware-usb-observations.md)
- [USB exposure versus physical NOR and MCU flash](firmware/README.md)
- [FAT damage and validated recovery](filesystem/README.md)
- [Copied content, dashboard JSON, and OTA comparison](content/README.md)
- [Image acquisition verification](acquisition-verification.json)
- [Recovered manifest files verified against image bytes](manifest-recovery-verification.json)

Two read-only USB volume acquisitions produced133693440-byte images, SHA256:
`0ab3d0de83f774e61eb548e5109fa53f1f2f86cd138b9e5a7d16fef8a45879b8`.

Original captures and193 successfully copied files are under
`evidence/usb/2026-09-09-noodoe-[redacted USB serial]/`.
The images preserve only the127.5MiB USB-exposed external-NOR range, not the
MCU internal flash or the last512KiB of the128MiB external NOR.

All analysis/recovery outputs are derived locally. A recovered resource with a
reference-manifest hash is not evidence that its path is currently allocated
or that the complete active filesystem tree has been restored.