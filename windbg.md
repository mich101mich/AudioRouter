# How to use windbg

## Startup

```sh
# Enable Debug Markup Language (allows fancy output with hyperlinks etc.)
.prefer_dml 1

# Set log filter mask to accept everything
ed nt!Kd_DEFAULT_MASK 0xFFFFFFFF

# Check that filter is set (first block should be all 'f')
dd nt!kd_DEFAULT_MASK
```

## Inspect modules

### Loaded modules
```sh
# list all modules
lm

# show verbose info on module audio_router
lm v m audio_router
```

### Loaded module info
```sh
# Show info on module audio_router
!lmi audio_router
```

### Header info
```sh
# Show info on module audio_router
!dh audio_router
```

## Add symbol paths

### Setup
```sh
.symfix
.sympath+ C:\projects\driver\AudioRouter\target\deploy
.reload /f audio_router
```

### Inspect items
```sh
x /D audio_router!

# supports filters:
x /D audio_router!audio_router::driv*
```


## Other

```sh
# Add symbol paths
.sympath+ C:\projects\driver\AudioRouter\target\debug
.reload /f

# Add source paths
.srcpath+ C:\projects\driver\AudioRouter

```

