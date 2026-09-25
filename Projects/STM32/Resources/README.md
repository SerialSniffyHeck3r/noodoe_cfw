# Exact external resources

`NOODOE.RSC` is the PC-generated1MiB initial container; `slot.bin` is the512KiB prepared slot. Neither proves installation on the donor.

See [memory and installation contract](../MEMORY.md). Generate with `python tools/resource_pack.py`, verify with `--check`. Never copy through an unvalidated FAT allocator or raw arbitrary NOR address. The current donor fails the full FAT preflight; leave its existing data intact.

`manifest.json` records all12 assets, their exact original bytes, CRC32 and SHA256. Original font/TI license notices and source inputs remain in their respective asset directories.
