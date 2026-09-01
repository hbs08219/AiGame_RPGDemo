# 称号系统

项目私有插件：为 RPGDemo 提供称号配置能力，参考物品编辑器设计，支持可视化配置称号名与称号描述，并可被后续运行时读取。

## 数据资源

| 资源 | 说明 | 字段 |
|---|---|---|
| `title`（称号） | 一个可授予玩家的称号 | `id`（必填，唯一，自动生成）、`name`（必填，称号名，全工程唯一）、`description`（可选，称号描述） |

数据以 `document-per-file` 方式持久化在项目数据目录 `title/title/`，每个称号一个 `<id>.json` 文件，结构由 `schemas/title.schema.json` 校验。

## 声明式 Action

编辑器通过受控 plugin-action API 访问数据（`toolgen.read` / `toolgen.write`），Action 名由平台统一生成：

- `project.title.list_titles` — 列出称号
- `project.title.create_title` — 创建称号
- `project.title.update_title` — 更新称号（需携带 revision，冲突时返回 409）
- `project.title.delete_title` — 删除称号

所有请求统一为 `POST /plugin-tool/project.title/<actionName>?projectId=<projectId>`，当前项目身份由宿主上下文提供。

## 编辑器

静态编辑器位于 `dist/index.html`，采用「列表—详情」布局，支持新建、编辑、删除称号；称号名做非空与唯一性校验；删除需二次确认；revision 冲突时提供「重新加载远端」恢复路径。编辑器通过 `#agmaker-editor-contract` 嵌入数据访问合同，并通过宿主鉴权桥接获取会话令牌。

## 运行时分类

`configuration`（纯配置插件）：由声明式数据资源与通用 CRUD Runner 提供能力，无 UE/C++ 运行时组件。

