# Contributing to NeverMind

## 提交流程

1. 新建分支并按里程碑拆分功能。
2. 本地执行：
   - `make clean all`
   - `make test`
   - `make integration`
   - `make smoke`
3. 提交 PR，填写 `.github/pull_request_template.md`。
4. CI 全绿后合并。

## Commit 规范（Conventional Commits）

- 格式：`type(scope?): description`
- 示例：
  - `feat(mm): add bitmap pmm allocator`
  - `fix(fs): prevent invalid fd dereference`
  - `docs(ci): add release workflow notes`

常用 `type`：`feat` / `fix` / `docs` / `refactor` / `test` / `chore`。

## PR 必填内容

- 实现目的
- 设计要点
- 测试说明
- 回归风险
- 兼容性影响

## 发布规范

- 使用 semver tag：`vMAJOR.MINOR.PATCH`
- 发布前更新：`CHANGELOG.md`、`FINAL_DECLARATION.md`
