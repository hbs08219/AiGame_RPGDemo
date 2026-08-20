# 货币 Schema 参考（project.currency）

> 适用范围：RpgDemo 项目「货币系统」插件配置的所有货币与兑换体系。
> 本参考与平台 `currency-exchange.cjs` 的硬校验规则逐条对齐。

## 何时使用

- 新增/修改一种货币前，先读本参考确认字段合法。
- 配置兑换体系（基准货币 / 定向兑换对 / 手续费）前，读本参考的「兑换体系配置」一节。

## 货币文档（每种货币一个 JSON）

| 字段 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `id` | string | 是 | 非空字符串 | 货币唯一 ID，建议 `CUR_` 前缀，如 `CUR_gold` |
| `name` | string | 否 | 字符串 | 显示名，如「金币」 |
| `description` | string | 否 | 字符串 | 货币描述 |
| `precision` | number | 否 | 整数 ∈ [0,8]，缺省 0 | 小数位数。兑换结果按目标货币 precision 向下截断（防增发） |
| `unitValueInBase` | number | 否 | 有限数 > 0，缺省 1 | 相对基准货币的单位价值。例：1 钻石 = 100 金币 → 钻石填 100 |

**硬校验错误（validate_currency 返回非空即不通过）**：
- 货币文档必须是 JSON 对象
- `id` 必须是非空字符串
- `precision` 必须是 [0,8] 的整数
- `unitValueInBase` 必须是 > 0 的有限数值

### 示例

```json
{
  "id": "CUR_gem",
  "name": "钻石",
  "description": "付费货币，可兑换金币",
  "precision": 0,
  "unitValueInBase": 100
}
```

## 兑换体系配置（`Currency/_exchange.json`）

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| `version` | number | 缺省 1 | 配置版本 |
| `baseCurrencyId` | string \| null | 必须真实存在，且其 `unitValueInBase` 必须等于 1 | 基准货币，任意两种货币经它锚定换算 |
| `overrides` | array | 见下 | 定向兑换对，覆盖基准锚定换算 |

### overrides[i]（定向兑换对）

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| `from` | string | 非空、真实存在、≠ to | 源货币 ID |
| `to` | string | 非空、真实存在、≠ from | 目标货币 ID |
| `rate` | number | 有限数 > 0 | 1 单位 from 兑 `rate` 单位 to |
| `feeRate` | number | 有限数 ∈ [0,1)，缺省 0 | 手续费率，从 gross 中扣除 |

**约束**：`from` 与 `to` 不能相同；同一 `from->to` 不能重复；引用的货币必须真实存在。

### 示例

```json
{
  "version": 1,
  "baseCurrencyId": "CUR_gold",
  "overrides": [
    { "from": "CUR_gem", "to": "CUR_gold", "rate": 98, "feeRate": 0.02 }
  ]
}
```

## 兑换试算规则（quote_exchange）

优先级：`identity`（from==to，rate=1）→ `override`（命中定向兑换对）→ `base`（基准锚定）。

- **override**：`gross = amount × rate`，`fee = gross × feeRate`，`net = (gross - fee)` 按目标 precision 截断
- **base**：`gross = amount × from.unitValueInBase / to.unitValueInBase`，无手续费，`net` 按目标 precision 截断
- **截断**：先修正浮点噪声再 `floor`，保证兑换不增发（如 0.3/0.1=3，1.999→1）

**报错**：未设基准且无对应 override → `EXCHANGE_NO_PATH`；基准货币 `unitValueInBase ≠ 1` → `EXCHANGE_CONFIG_INVALID`。
