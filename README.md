# OmniSync

Synchronize `.ini` configuration files across multiple Unreal Engine projects and engine versions.

---

## How to use

1. Copy the plugin to your project's `Plugins/` directory
2. Enable the plugin in **Edit → Plugins**  
3. Open **Editor → Project Settings → Plugins → OmniSync**
4. Click **Discover All Config Files**
5. Enable the files you want to sync and select their scope:
   - **Global** - Shared across all projects and engine versions
   - **PerEngineVersion** - Shared across projects using the same engine version
   - **PerProject** - Project-specific synchronization
6. Click **Save to Global** to push settings
7. In other projects, use **Load from Global** to pull settings

---

## Compatible Engine Version

| Engine Version | Status |
|---|---|
| UE 5.0 | ⚠️ |
| UE 5.1 | ⚠️ |
| UE 5.2 | ⚠️ |
| UE 5.3 | ⚠️ |
| UE 5.4 | ⚠️ |
| UE 5.5 | ✅ |

✅ Ready | ⚠️ Not tested

---

## Features

- Cross-project editor settings synchronization
- Three sync scopes: Global, PerEngineVersion, PerProject
- Auto-sync with 10-second polling interval
- Selective config file synchronization
- Hierarchical tree UI for file management
- Manual save/load operations

---

## Storage Location

Centralized files are stored in:
---

```
%USERPROFILE%/AppData/Local/UnrealEngine/OmniSync/
```

---

## Contribute

Contributions are welcome! Here's how you can help:

- **Report Issues**: Found a bug or have a feature request? [Open an issue](https://github.com/yourusername/OmniSync/issues) on GitHub
- **Submit Pull Requests**: Want to fix a bug or add a feature? Fork the repository and submit a pull request
- **Test Compatibility**: Help test the plugin with different Unreal Engine versions and report your results

When contributing code, please ensure it follows Unreal Engine naming conventions and includes appropriate reflection macros for UObject properties.
