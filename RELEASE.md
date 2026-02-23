# Release Guide

## Versioning

- 使用 SemVer：`vMAJOR.MINOR.PATCH`
- 发布前更新：`CHANGELOG.md`、`FINAL_DECLARATION.md`

## Pre-release checklist

1. `make clean all`
2. `make test`
3. `make integration`
4. `make user-tools`
5. `make smoke`
6. `make acceptance`

确保 `tests/results-YYYYMMDD/summary.txt` 与 `build.log` 完整。

## Tag and publish

```bash
git tag -a v0.1.0 -m "release: v0.1.0"
git push origin v0.1.0
```

触发 `release.yml` 后，产物包含：

- `build/kernel.elf`
- `build/kernel-debug.elf`
- `build/kernel.map`
- `build/nevermind-m1.iso`

## Post-release

- 在 release 页面附上验收摘要与性能基线说明。
- 更新下一迭代里程碑计划。
