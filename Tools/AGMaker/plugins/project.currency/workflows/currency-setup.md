# 货币体系搭建工作流（currency-setup）

> 目标：从零搭好一套可用的货币 + 兑换体系。以「金币（基准）+ 钻石」为参考。

## 步骤

```aigame-diagram
{"title":"货币体系搭建流程","direction":"TB","nodes":[{"id":"s1","label":"1. list_currencies 摸清现状","status":"pending"},{"id":"s2","label":"2. create_currency 创建各货币","status":"pending"},{"id":"s3","label":"3. set_base_currency 设基准","status":"pending"},{"id":"s4","label":"4. (可选) upsert_exchange_override 设定向兑换","status":"pending"},{"id":"s5","label":"5. quote_exchange 验证典型金额","status":"pending"}],"edges":[{"from":"s1","to":"s2"},{"from":"s2","to":"s3"},{"from":"s3","to":"s4"},{"from":"s4","to":"s5"}]}
```

## 参考产出（金币 + 钻石）

1. `list_currencies` → 确认无 `CUR_gold` / `CUR_gem`。
2. `create_currency`：
   - `{ id:"CUR_gold", name:"金币", precision:0, unitValueInBase:1 }`
   - `{ id:"CUR_gem", name:"钻石", precision:0, unitValueInBase:100 }`
3. `set_base_currency { id:"CUR_gold" }`。
4. （可选）钻石换金币加损耗：`upsert_exchange_override { from:"CUR_gem", to:"CUR_gold", rate:98, feeRate:0.02 }`。
5. 验证：
   - `quote_exchange { from:"CUR_gold", to:"CUR_gem", amount:100 }` → `net = 1`，`path = "base"`
   - `quote_exchange { from:"CUR_gem", to:"CUR_gold", amount:1 }` → 无 override 时 `net = 100`；有上面 override 时 `net = floor(98×0.98)=96`

## 验收标准

- 两种货币均通过 `validate_currency`。
- `get_exchange_config` 返回 `baseCurrencyId = "CUR_gold"`。
- 试算结果符合上表。
