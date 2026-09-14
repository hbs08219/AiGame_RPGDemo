# 称号系统

项目私有插件：称号系统。为游戏内的「称号」定义提供数据结构与可视化编辑器，供成就、称号佩戴等后续系统引用。

## 数据模型（`schemas/title.schema.json`）

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | string | 是 | 系统自动生成的唯一标识，只读，不可编辑 |
| `name` | string | 是 | 称号名，全项目内唯一 |
| `description` | string | 否 | 称号描述，可留空 |

Schema 为 `additionalProperties: false`，不接受未声明字段。

## 数据资源

- 资源 ID：`title`
- 存储根：`title/title`，每个称号一个文件（`{id}.json`）
- 支持操作：`list`、`get`、`create`、`update`、`delete`、`validate`
- 当前版本未开放 `findReferences`

## 编辑器

`dist/index.html` 是本插件的项目私有静态编辑器，通过 `#agmaker-editor-contract` 中声明的受控 Action（`project.title.list_titles` / `create_title` / `update_title` / `delete_title`）读写称号数据，不直接读写底层 JSON 文件。编辑器实现了：

- 列表 + 详情的浏览与编辑工作流（list-detail 布局，参考物品编辑器范式）
- 新建 / 编辑 / 保存的必填校验与重名校验
- 未保存修改的脏数据保护（切换选中项、离开页面前提示）
- 保存冲突检测与“覆盖保存 / 放弃并重新加载”选择
- 删除采用编辑器内单次确认：选中称号后点删除，弹出一次确认，取消不删除，确认后直接调用 `delete_title` 并刷新列表、清空详情、显示成功反馈；无审批、无令牌，不携带任何审批请求头
- 列表、详情在加载中、空、错误、未选中状态下的对应提示

## 编辑器构建与交付

编辑器采用无框架的受控静态源文件，避免构建链改变 `#agmaker-editor-contract`、Manifest URL 或已声明的 Action 合同：

- 源文件：`src/index.html`
- 运行时交付物：`dist/index.html`（Manifest 引用的唯一编辑器文件，受版本控制）
- 构建：在本目录执行 `npm run build`
- 一致性检查：执行 `npm run check:build`
- 构建回归测试：执行 `npm run test:build`

`npm run build` 只会从受控源确定性再生 `dist/index.html`；不要直接编辑 `dist/index.html`。每次修改源文件后先构建，再运行一致性检查和测试。

## Agent 使用约束

Agent 应通过 `project.title.*` 已声明的 Action 读写称号数据，不应直接编辑 `Content/Data/ToolGen/title/title/` 下的原始 JSON 文件，以保证 Schema 校验与唯一性规则一致生效。删除仅可用于用户明确指定的数据，必须先读取目标的当前 revision 并在编辑器内单次确认后直接删除。
