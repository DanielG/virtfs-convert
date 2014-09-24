virtfs-convert
==============

```
Usage: virtfs-convert [-m] [FILES...]
```

Convert file metadata to the `mapped-xattr` format used by QEMU's VirtFS
paravirtualized filesystem.


Convert a whole directory
-------------------------

```
find DIRECTORY -print0 | xargs -0 virtfs-convert
```
