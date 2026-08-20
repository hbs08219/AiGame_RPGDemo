// RpgDemo TypeScript Gameplay bootstrap.
// 游戏规则位于 packages/gameplay-kernel；本文件仅将 Kernel 命令映射到 UE 3C Adapter。
declare const require: (moduleId: string) => any;

const UE: any = require('ue');
const { argv }: { argv: { getByName(name: string): any } } = require('puerts');
const { createThreeCRuntime }: { createThreeCRuntime: (entityId: string, config: any) => any } = require('./kernel/index');

const host = argv.getByName('Host') as any;
let started = false;

function readJson(relativePath: string): any {
  const raw = host.LoadToolGenJson(relativePath) as string;
  if (!raw) throw new Error(`ToolGen data missing: ${relativePath}`);
  return JSON.parse(raw);
}

function startThreeC(): void {
  if (started) return;
  started = true;

  const settings = readJson('ProjectSettings.json');
  const player = readJson(`Characters/Classes/${settings.playerCharacter}.json`);
  const camera = readJson(`Camera/${settings.defaultCamera}.json`);
  const cameraPoseId = settings.defaultCameraPose;
  if (!cameraPoseId || !camera.poses?.[cameraPoseId]) {
    throw new Error(`ProjectSettings.defaultCameraPose is invalid for Camera/${settings.defaultCamera}.json`);
  }
  const input = readJson(`Input/${settings.defaultInputContext}.json`);
  const level = readJson(`Level/${settings.defaultMap}.json`);
  const gameContextId = input.defaultContexts?.[0];
  const move = input.contexts?.[gameContextId]?.axes2D?.Move;
  if (!move) throw new Error('AGMaker Input config is missing Move Axis2D');

  const bridge = host.GetGameSubsystem(UE.RpgDemoTs3CBridge.StaticClass());
  if (!bridge) throw new Error('RpgDemoTs3CBridge is unavailable');

  const worldRuntime = level.worldRuntime;
  const spawn = worldRuntime?.playerSpawn;
  if (level.type !== 'world' || !spawn || worldRuntime.movementMode !== 'fixedPlane') {
    throw new Error(`Level/${settings.defaultMap}.json must define worldRuntime.fixedPlane and playerSpawn`);
  }
  bridge.PlacePlayer(spawn.x, spawn.y, worldRuntime.movementPlaneZ);
  const initialPosition = bridge.GetLogicalPosition();
  const walkSpeed = player.moveSpeed?.walk;
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

  const dispatch = (commands: readonly any[]): void => {
    for (const command of commands) {
      if (command.type === 'setCameraMode') {
        bridge.ApplyCameraMode(command.mode);
      } else if (command.type === 'setAnimationState') {
        bridge.ApplyAnimationState(command.state === 'walk' ? 'Walk' : 'Idle', command.facing);
      } else if (command.type === 'requestMove') {
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
  host.OnFrame.Add((deltaSeconds: number) => {
    frameCount += 1;
    if (frameCount === 1 || frameCount % 120 === 0) console.log(`[AGMaker][3C] frame=${frameCount}`);
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
  } catch (error) {
    console.error(`[AGMaker][3C] TS bootstrap failed: ${error instanceof Error ? error.message : String(error)}`);
  }
});
