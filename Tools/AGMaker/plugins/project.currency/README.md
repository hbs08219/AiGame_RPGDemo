# 货币系统插件（project.currency）

RpgDemo 项目私有插件：配置货币种类与兑换比例，并提供货币列表 / 汇率试算静态工具页。

## 能力

- **货币配置**：id / name / description / precision / unitValueInBase（相对基准货币的单位价值）
- **兑换体系**：基准货币锚定 + 定向兑换对（override）+ 手续费（feeRate），按目标货币精度向下截断防增发
- **工具页**：`/plugins/project.currency/`（货币列表、汇率关系、本地试算器）

## 关键说明

本插件**不新增原生 Agent Action**，manifest 声明复用平台已实现的 11 个 `com.aigame.currency.*` 受信 Action 完成读写 / 校验 / 试算。数据根：`Currency`。

## 目录

- `knowledge/currency-schema.md` —— 字段约束（与平台 currency-exchange.cjs 对齐）
- `knowledge/currency-design.md` —— 货币设计指南（基准选择 / 防通胀 / 精度）
- `skills/currency-configuration/SKILL.md` —— Agent 配置操作技能
- `workflows/currency-setup.md` —— 货币体系搭建工作流
- `dist/index.html` —— 静态工具页

## 示例数据

`Content/Data/ToolGen/Currency/`：`CUR_gold`（金币，基准）、`CUR_gem`（钻石，1=100 金币）、`_exchange.json`。
