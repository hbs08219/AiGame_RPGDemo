"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.createThreeCRuntime = createThreeCRuntime;
const EMPTY_INPUT = {
    move: { x: 0, y: 0 },
    interactPressed: false,
    confirmPressed: false,
    cancelPressed: false,
};
function clampAxis(value) {
    return Number.isFinite(value) ? Math.max(-1, Math.min(1, value)) : 0;
}
function resolveFacing(move, current) {
    // 现有精灵表只有四向：斜向移动时优先选择水平动画，
    // 即左上/左下 → left，右上/右下 → right；仅纯垂直输入才播放 up/down。
    if (move.x !== 0)
        return move.x > 0 ? 'right' : 'left';
    if (move.y !== 0)
        return move.y > 0 ? 'up' : 'down';
    return current;
}
function cloneState(state) {
    return {
        position: Object.assign({}, state.position),
        facing: state.facing,
        locomotion: state.locomotion,
    };
}
/**
 * 纯 TypeScript 的固定俯视 3C Kernel。
 * 只输出移动/动画/相机语义命令；碰撞与真实位移由各引擎 Adapter 执行并回传结果。
 */
function createThreeCRuntime(entityId, config) {
    if (!entityId)
        throw new Error('ThreeC entityId is required');
    if (!(config.moveSpeed > 0) || !Number.isFinite(config.moveSpeed))
        throw new Error('ThreeC moveSpeed must be positive');
    if (!(config.moveDeadZone >= 0) || !Number.isFinite(config.moveDeadZone))
        throw new Error('ThreeC moveDeadZone must be non-negative');
    let state = {
        position: Object.assign({}, config.initialPosition),
        facing: config.initialFacing,
        locomotion: 'idle',
    };
    const animation = () => ({
        type: 'setAnimationState',
        entityId,
        state: state.locomotion,
        facing: state.facing,
    });
    return {
        start() {
            return [{ type: 'setCameraMode', mode: config.cameraMode }, animation()];
        },
        tick(input = EMPTY_INPUT, deltaMs) {
            var _a, _b, _c, _d;
            const x = clampAxis((_b = (_a = input.move) === null || _a === void 0 ? void 0 : _a.x) !== null && _b !== void 0 ? _b : 0);
            const y = clampAxis((_d = (_c = input.move) === null || _c === void 0 ? void 0 : _c.y) !== null && _d !== void 0 ? _d : 0);
            const magnitude = Math.hypot(x, y);
            const wasLocomotion = state.locomotion;
            const wasFacing = state.facing;
            if (magnitude <= config.moveDeadZone || deltaMs <= 0 || !Number.isFinite(deltaMs)) {
                state.locomotion = 'idle';
                return wasLocomotion === state.locomotion ? [] : [animation()];
            }
            const normalized = { x: x / magnitude, y: y / magnitude };
            state.facing = resolveFacing(normalized, state.facing);
            state.locomotion = 'walk';
            const distance = config.moveSpeed * deltaMs / 1000;
            const command = {
                type: 'requestMove',
                entityId,
                from: Object.assign({}, state.position),
                delta: { x: normalized.x * distance, y: normalized.y * distance },
                facing: state.facing,
            };
            return wasLocomotion !== state.locomotion || wasFacing !== state.facing ? [command, animation()] : [command];
        },
        dispatch(event) {
            if (event.type !== 'movementResolved' || event.entityId !== entityId)
                return [];
            state.position = Object.assign({}, event.position);
            return [];
        },
        getState() {
            return cloneState(state);
        },
    };
}
