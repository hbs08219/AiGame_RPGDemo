# 货币配置技能（currency-configuration）

> 适用范围：RpgDemo 项目货币种类与兑换体系的配置、校验与试算。
> 操作前必读：`knowledge/currency-schema.md`（字段约束）、`knowledge/currency-design.md`（设计取舍）。

## 能力边界

本插件不新增原生 Action，全部读写通过平台已实现的 `com.aigame.currency.*` 受信 Action 完成。

| 场景 | 用的 Action | 风险 |
|------|------------|------|
| 列出/读取/校验货币 | `list_currencies` `get_currency` `validate_currency` | safe |
| 查引用 | `find_currency_references` | safe |
| 兑换试算 | `quote_exchange` | safe（纯函数不写盘） |
| 创建货币 | `create_currency` | safe（仅新增不覆盖） |
| 改货币 / 设基准 / 增删兑换对 | `update_currency` `set_base_currency` `upsert_exchange_override` `remove_exchange_override` | critical |

## 标准操作流程

### A. 新增一种货币

1. `list_currencies` —— 确认 id / 显示名不冲突（**必须先调**）。
2. 读本参考确认 `precision`、`unitValueInBase` 取值。
3. （可选）`validate_currency` 传候选对象预校验。
4. `create_currency` 创建。
5. 若是项目第一种货币，或要切换基准 → 走流程 C。

### B. 修改一种货币

1. `get_currency` 读现状（**必须先调**）。
2. `find_currency_references` 查引用，评估改动影响面。
3. `update_currency` 增量更新（只传 id + 要改的字段）。

### C. 配置兑换体系

1. `get_exchange_config` 读当前 `baseCurrencyId` 与 `overrides`。
2. 设/换基准：`set_base_currency`（目标货币 `unitValueInBase` 必须为 1）。
3. 加定向兑换对：`upsert_exchange_override`（from/to/rate[/feeRate]）。
4. 移除兑换对：`remove_exchange_override`。

### D. 验证兑换结果

`quote_exchange(from, to, amount)` 试算，核对返回的 `gross / fee / net / path / rate`：
- `path=base` 表示走基准锚定；`path=override` 表示命中定向兑换对。
- `net` 已按目标货币 `precision` 向下截断。

## 防错要点

- 任何写操作前先 list/get 摸清现状，避免 id 冲突或覆盖他人改动。
- 改 `unitValueInBase` 或基准货币会影响所有相关兑换，先 `quote_exchange` 实测。
- 基准货币的 `unitValueInBase` 必须保持 = 1，否则基准锚定换算报错。
- 删除/大改前必须 `find_currency_references`。
