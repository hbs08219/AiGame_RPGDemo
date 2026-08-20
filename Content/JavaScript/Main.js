"use strict";
const UE = require('ue');
const { argv } = require('puerts');
const { createThreeCRuntime } = require('./kernel/index');
const host = argv.getByName('Host');
let started = false;
function readJson(relativePath) {
    const raw = host.LoadToolGenJson(relativePath);
    if (!raw)
        throw new Error(`ToolGen data missing: ${relativePath}`);
    return JSON.parse(raw);
}
function startThreeC() {
    var _a, _b, _c, _d, _e;
    if (started)
        return;
    started = true;
    const settings = readJson('ProjectSettings.json');
    const player = readJson(`Characters/Classes/${settings.playerCharacter}.json`);
    const camera = readJson(`Camera/${settings.defaultCamera}.json`);
    const cameraPoseId = settings.defaultCameraPose;
    if (!cameraPoseId || !((_a = camera.poses) === null || _a === void 0 ? void 0 : _a[cameraPoseId])) {
        throw new Error(`ProjectSettings.defaultCameraPose is invalid for Camera/${settings.defaultCamera}.json`);
    }
    const input = readJson(`Input/${settings.defaultInputContext}.json`);
    const level = readJson(`Level/${settings.defaultMap}.json`);
    const gameContextId = (_b = input.defaultContexts) === null || _b === void 0 ? void 0 : _b[0];
    const move = (_e = (_d = (_c = input.contexts) === null || _c === void 0 ? void 0 : _c[gameContextId]) === null || _d === void 0 ? void 0 : _d.axes2D) === null || _e === void 0 ? void 0 : _e.Move;
    if (!move)
        throw new Error('AGMaker Input config is missing Move Axis2D');
    const bridge = host.GetGameSubsystem(UE.RpgDemoTs3CBridge.StaticClass());
    if (!bridge)
        throw new Error('RpgDemoTs3CBridge is unavailable');
    const worldRuntime = level.worldRuntime;
    const spawn = worldRuntime === null || worldRuntime === void 0 ? void 0 : worldRuntime.playerSpawn;
    if (level.type !== 'world' || !spawn || worldRuntime.movementMode !== 'fixedPlane') {
        throw new Error(`Level/${settings.defaultMap}.json must define worldRuntime.fixedPlane and playerSpawn`);
    }
    bridge.PlacePlayer(spawn.x, spawn.y, worldRuntime.movementPlaneZ);
    const initialPosition = bridge.GetLogicalPosition();
    const walkSpeed = player.moveSpeed === null || player.moveSpeed === void 0 ? void 0 : player.moveSpeed.walk;
    if (typeof walkSpeed !== 'number' || walkSpeed < 0) {
        throw new Error(`Characters/Classes/${settings.playerCharacter}.json must define moveSpeed.walk as a non-negative number`);
    }
    const runtime = createThreeCRuntime(settings.playerCharacter, {
        // 3C 基础移动只消费角色编辑器配置的步行速度；run/sprint 留给未来由 Gameplay 状态显式切换。
        moveSpeed: walkSpeed,
        moveDeadZone: 0.001,
        initialPosition: { x: initialPosition.X, y: initialPosition.Y },
        initialFacing: spawn.facing,
        cameraMode: cameraPoseId,
    });
    const dispatch = (commands) => {
        for (const command of commands) {
            if (command.type === 'setCameraMode') {
                bridge.ApplyCameraMode(command.mode);
            }
            else if (command.type === 'setAnimationState') {
                bridge.ApplyAnimationState(command.state === 'walk' ? 'Walk' : 'Idle', command.facing);
            }
            else if (command.type === 'requestMove') {
                bridge.ApplyMoveIntent(command.delta.x, command.delta.y);
                const resolved = bridge.GetLogicalPosition();
                runtime.dispatch({
                    type: 'movementResolved',
                    entityId: command.entityId,
                    position: { x: resolved.X, y: resolved.Y },
                });
            }
        }
    };
    dispatch(runtime.start());
    let previousInput = '0,0';
    let frameCount = 0;
    host.OnFrame.Add((deltaSeconds) => {
        frameCount += 1;
        if (frameCount === 1 || frameCount % 120 === 0)
            console.log(`[AGMaker][3C] frame=${frameCount}`);
        const rawMove = bridge.ReadMoveInput();
        const inputKey = `${rawMove.X},${rawMove.Y}`;
        const inputChanged = inputKey !== previousInput;
        if (inputChanged) {
            previousInput = inputKey;
            console.log(`[AGMaker][3C] input=${inputKey}`);
        }
        dispatch(runtime.tick({
            move: { x: rawMove.X, y: rawMove.Y },
            interactPressed: false,
            confirmPressed: false,
            cancelPressed: false,
        }, deltaSeconds * 1000));
        if (inputChanged && inputKey === '0,0') {
            const state = runtime.getState();
            console.log(`[AGMaker][3C] stoppedAt=${state.position.x},${state.position.y}`);
        }
    });
    console.log(`[AGMaker][3C] TS Kernel started: player=${settings.playerCharacter}, level=${level.id}, input=${gameContextId}, camera=${camera.id}/${cameraPoseId}`);
}
host.OnReady.Add(() => {
    try {
        startThreeC();
    }
    catch (error) {
        console.error(`[AGMaker][3C] TS bootstrap failed: ${error instanceof Error ? error.message : String(error)}`);
    }
});
