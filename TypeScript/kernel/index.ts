export type Facing = 'up' | 'down' | 'left' | 'right';
export type Locomotion = 'idle' | 'walk';

export interface MoveInput {
  x: number;
  y: number;
}

export interface InputSnapshot {
  move: MoveInput;
  interactPressed: boolean;
  confirmPressed: boolean;
  cancelPressed: boolean;
}

export interface Vec2 {
  x: number;
  y: number;
}

export interface ThreeCConfig {
  moveSpeed: number;
  moveDeadZone: number;
  initialPosition: Vec2;
  initialFacing: Facing;
  cameraMode: string;
}

export interface ThreeCState {
  position: Vec2;
  facing: Facing;
  locomotion: Locomotion;
}

export type EngineCommand =
  | { type: 'setCameraMode'; mode: string }
  | { type: 'requestMove'; entityId: string; from: Vec2; delta: Vec2; facing: Facing }
  | { type: 'setAnimationState'; entityId: string; state: Locomotion; facing: Facing };

export interface MovementResolvedEvent {
  type: 'movementResolved';
  entityId: string;
  position: Vec2;
}

export interface GameplayRuntime {
  start(): readonly EngineCommand[];
  tick(input: InputSnapshot, deltaMs: number): readonly EngineCommand[];
  dispatch(event: MovementResolvedEvent): readonly EngineCommand[];
  getState(): Readonly<ThreeCState>;
}

const EMPTY_INPUT: InputSnapshot = {
  move: { x: 0, y: 0 },
  interactPressed: false,
  confirmPressed: false,
  cancelPressed: false,
};

function clampAxis(value: number): number {
  return Number.isFinite(value) ? Math.max(-1, Math.min(1, value)) : 0;
}

function resolveFacing(move: MoveInput, current: Facing): Facing {
  // 现有精灵表只有四向：斜向移动时优先选择水平动画，
  // 即左上/左下 → left，右上/右下 → right；仅纯垂直输入才播放 up/down。
  if (move.x !== 0) return move.x > 0 ? 'right' : 'left';
  if (move.y !== 0) return move.y > 0 ? 'up' : 'down';
  return current;
}

function cloneState(state: ThreeCState): ThreeCState {
  return {
    position: { ...state.position },
    facing: state.facing,
    locomotion: state.locomotion,
  };
}

/**
 * 纯 TypeScript 的固定俯视 3C Kernel。
 * 只输出移动/动画/相机语义命令；碰撞与真实位移由各引擎 Adapter 执行并回传结果。
 */
export function createThreeCRuntime(entityId: string, config: ThreeCConfig): GameplayRuntime {
  if (!entityId) throw new Error('ThreeC entityId is required');
  if (!(config.moveSpeed > 0) || !Number.isFinite(config.moveSpeed)) throw new Error('ThreeC moveSpeed must be positive');
  if (!(config.moveDeadZone >= 0) || !Number.isFinite(config.moveDeadZone)) throw new Error('ThreeC moveDeadZone must be non-negative');

  let state: ThreeCState = {
    position: { ...config.initialPosition },
    facing: config.initialFacing,
    locomotion: 'idle',
  };

  const animation = (): EngineCommand => ({
    type: 'setAnimationState',
    entityId,
    state: state.locomotion,
    facing: state.facing,
  });

  return {
    start(): readonly EngineCommand[] {
      return [{ type: 'setCameraMode', mode: config.cameraMode }, animation()];
    },

    tick(input: InputSnapshot = EMPTY_INPUT, deltaMs: number): readonly EngineCommand[] {
      const x = clampAxis(input.move?.x ?? 0);
      const y = clampAxis(input.move?.y ?? 0);
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
      const command: EngineCommand = {
        type: 'requestMove',
        entityId,
        from: { ...state.position },
        delta: { x: normalized.x * distance, y: normalized.y * distance },
        facing: state.facing,
      };
      return wasLocomotion !== state.locomotion || wasFacing !== state.facing ? [command, animation()] : [command];
    },

    dispatch(event: MovementResolvedEvent): readonly EngineCommand[] {
      if (event.type !== 'movementResolved' || event.entityId !== entityId) return [];
      state.position = { ...event.position };
      return [];
    },

    getState(): Readonly<ThreeCState> {
      return cloneState(state);
    },
  };
}
