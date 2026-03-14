# Newlib Patches for AdrOS Target

These patches add `i686-*-adros*` as a recognized target in the Newlib/Binutils
build system. Apply them to a fresh Newlib source tree.

## Files to modify in Newlib source tree

### 1. `config.sub` (Binutils/GCC/Newlib shared)

Add `adros` to the OS list. Find the section with `-dicos*` and add:

```
-adros*)
    os=-adros
    ;;
```

### 2. `newlib/configure.host`

Add this case before the `*` default case:

```
  i[3-7]86-*-adros*)
    sys_dir=adros
    ;;
```

### 3. `libgloss/configure.in` (or `configure.ac`)

Add to the target list:

```
  i[3-7]86-*-adros*)
    AC_CONFIG_SUBDIRS([adros])
    ;;
```

Then copy `libgloss/adros/` from this repo into the Newlib source tree.

### 4. `newlib/libc/include/sys/config.h`

Add AdrOS-specific defines:

```c
#ifdef __adros__
#define _READ_WRITE_RETURN_TYPE int
#define __DYNAMIC_REENT__
#endif
```

## Quick Integration

```bash
NEWLIB_SRC=/path/to/newlib-cygwin

# Copy libgloss port
cp -r newlib/libgloss/adros/ $NEWLIB_SRC/libgloss/adros/

# Then manually add the configure entries listed above
```
